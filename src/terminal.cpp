#include "terminal.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>
#undef columns
#undef lines

namespace bmatrix {
namespace {
volatile std::sig_atomic_t stopped = 0, resized = 0, suspended = 0;
void signal_handler(int signal) {
    if (signal == SIGWINCH || signal == SIGCONT) resized = 1;
    else if (signal == SIGTSTP) suspended = 1;
    else stopped = signal;
}
void write_all(const std::string& text) {
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto count = ::write(STDOUT_FILENO, text.data() + offset, text.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("terminal write failed");
        offset += static_cast<std::size_t>(count);
    }
}
std::string* capability_output = nullptr;
int collect(int c) { capability_output->push_back(static_cast<char>(c)); return c; }
std::string cap(const char* name, long a = -1, long b = -1) {
    char* raw = tigetstr(const_cast<char*>(name));
    if (raw == nullptr || raw == reinterpret_cast<char*>(-1)) return {};
    std::string result;
    capability_output = &result;
    tputs(a < 0 ? raw : tparm(raw, a, b), 1, collect);
    capability_output = nullptr;
    return result;
}
class Terminal {
    termios saved{};
    bool active = false;
    std::array<struct sigaction, 5> previous{};
    const std::array<int, 5> signals{SIGINT, SIGTERM, SIGWINCH, SIGTSTP, SIGCONT};
public:
    int colors = 0;
    bool rgb = false;
    bool wrap_control = false;
    explicit Terminal(const std::string& color) {
        int error = 0;
        if (setupterm(nullptr, STDOUT_FILENO, &error) == ERR || error != 1)
            throw std::runtime_error("unsupported TERM; use --output plain");
        if (cap("cup", 0, 0).empty() || cap("clear").empty())
            throw std::runtime_error("TERM does not support cursor-addressed rendering; use --output plain");
        if (tcgetattr(STDIN_FILENO, &saved) != 0) throw std::runtime_error("cannot read terminal attributes");
        colors = std::max(0, tigetnum(const_cast<char*>("colors")));
        const char* color_term = std::getenv("COLORTERM");
        rgb = color == "truecolor" || (color == "auto" && color_term && (std::strcmp(color_term, "truecolor") == 0 || std::strcmp(color_term, "24bit") == 0));
        if (color == "none" || (color == "auto" && std::getenv("NO_COLOR"))) { colors = 0; rgb = false; }
        if (color == "16") colors = std::min(colors, 16);
        if (color == "256") colors = std::min(colors, 256);
        if (cap("setaf", 0).empty()) colors = 0;
        wrap_control = !cap("rmam").empty() && !cap("smam").empty();
        stopped = 0;
        resized = 0;
        suspended = 0;
        struct sigaction action{};
        action.sa_handler = signal_handler;
        sigemptyset(&action.sa_mask);
        for (std::size_t i = 0; i < signals.size(); ++i) sigaction(signals[i], &action, &previous[i]);
        try { enter(); } catch (...) { leave(); restore_signals(); throw; }
    }
    void restore_signals() noexcept {
        for (std::size_t i = 0; i < signals.size(); ++i) sigaction(signals[i], &previous[i], nullptr);
    }
    void enter() {
        termios raw = saved;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) throw std::runtime_error("cannot set terminal attributes");
        active = true;
        write_all(cap("smcup") + cap("civis") + (wrap_control ? cap("rmam") : "") + cap("sgr0") + ((colors > 0 || rgb) ? cap("setab", 0) : "") + cap("clear"));
    }
    void leave() noexcept {
        if (!active) return;
        try { write_all(cap("sgr0") + (wrap_control ? cap("smam") : "") + cap("cnorm") + cap("rmcup")); } catch (...) {}
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        active = false;
    }
    ~Terminal() { leave(); restore_signals(); }
    std::string foreground(Rgb c) const {
        if (rgb) return "\033[38;2;" + std::to_string(c.r) + ";" + std::to_string(c.g) + ";" + std::to_string(c.b) + "m";
        if (colors <= 0) return {};
        static constexpr std::array<Rgb, 16> basic{{{0,0,0},{128,0,0},{0,128,0},{128,128,0},{0,0,128},{128,0,128},{0,128,128},{192,192,192},{128,128,128},{255,0,0},{0,255,0},{255,255,0},{0,0,255},{255,0,255},{0,255,255},{255,255,255}}};
        int best = 0, distance = 1000000;
        const int count = colors >= 256 ? 256 : std::min(colors, 16);
        for (int i = 0; i < count; ++i) {
            Rgb candidate{};
            if (i < 16) candidate = basic[static_cast<std::size_t>(i)];
            else if (i >= 232) { const auto level = static_cast<std::uint8_t>(8 + (i - 232) * 10); candidate = {level, level, level}; }
            else {
                const int index = i - 16;
                auto level = [](int n) { return static_cast<std::uint8_t>(n ? 55 + n * 40 : 0); };
                candidate = {level(index / 36), level(index / 6 % 6), level(index % 6)};
            }
            const int dr = int(c.r) - int(candidate.r), dg = int(c.g) - int(candidate.g), db = int(c.b) - int(candidate.b);
            const int d = dr * dr + dg * dg + db * db;
            if (d < distance) { distance = d; best = i; }
        }
        return cap("setaf", best);
    }
    void paint(const Frame& frame, const Frame& previous_frame) const {
        std::string output;
        Rgb last{}; bool has_color = false;
        bool adjacent = false;
        for (int y = 0; y < frame.height; ++y) {
            adjacent = false;
            for (int x = 0; x < frame.width; ++x) {
                const auto index = static_cast<std::size_t>(y * frame.width + x);
                const auto& cell = frame.cells[index];
                if (previous_frame.width == frame.width && previous_frame.height == frame.height && previous_frame.cells[index] == cell) { adjacent = false; continue; }
                if (!adjacent) output += cap("cup", y, x);
                if (!has_color || last != cell.color) { output += foreground(cell.color); last = cell.color; has_color = true; }
                output += cell.glyph;
                adjacent = true;
            }
        }
        write_all(output);
    }
};
}
int run_terminal(const Image& image, const SessionOptions& options) {
    Terminal terminal(options.color);
    auto render = options.render;
    auto size = [&]() {
        winsize window{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col && window.ws_row) {
            render.width = std::min(options.fixed_size ? options.render.width : 500, static_cast<int>(window.ws_col));
            render.height = std::min(options.fixed_size ? options.render.height : 200, static_cast<int>(window.ws_row));
            // Reserve the final column on terminals without reliable wrap controls.
            if (!terminal.wrap_control && render.width == window.ws_col && render.width > 1) --render.width;
        }
    };
    size();
    Frame target = render_image(image, render), previous_frame;
    using Clock = std::chrono::steady_clock;
    const auto interval = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1 / options.fps));
    auto deadline = Clock::now(), last_time = deadline;
    double elapsed = 0;
    bool paused = false;
    unsigned long count = 0;
    while (!stopped) {
        if (suspended) {
            suspended = 0;
            terminal.leave();
            // SIGSTOP works even in orphaned PTY process groups; SIGCONT resumes.
            raise(SIGSTOP);
            terminal.enter();
            resized = 1;
            last_time = deadline = Clock::now();
        }
        if (resized) {
            resized = 0; size(); target = render_image(image, render);
            previous_frame = {}; write_all(cap("clear"));
        }
        auto now = Clock::now();
        if (!paused) elapsed += std::chrono::duration<double>(now - last_time).count();
        last_time = now;
        if (!paused && now >= deadline) {
            const auto frame = options.effect == Effect::matrix ? matrix_rain(target, elapsed, render.seed, render.palette, render.image_strength) :
                options.effect == Effect::flow ? flow_down(target, std::min(1.0, elapsed / options.duration), render.seed) : target;
            terminal.paint(frame, previous_frame); previous_frame = frame;
            if (options.frames && ++count >= options.frames) break;
            now = Clock::now();
            do { deadline += interval; } while (deadline <= now);
        }
        const auto milliseconds = std::chrono::duration<double, std::milli>(deadline - Clock::now()).count();
        const int timeout = paused ? 50 : std::clamp(static_cast<int>(std::ceil(milliseconds)), 1, 50);
        pollfd input{STDIN_FILENO, POLLIN, 0};
        const int ready = poll(&input, 1, timeout);
        if (ready < 0 && errno != EINTR) throw std::runtime_error("terminal input failed");
        if (ready > 0 && (input.revents & (POLLHUP | POLLERR | POLLNVAL))) break;
        if (ready > 0 && (input.revents & POLLIN)) {
            char keys[32]; const auto length = read(STDIN_FILENO, keys, sizeof(keys));
            for (ssize_t i = 0; i < length; ++i) {
                if (keys[i] == 'q' || keys[i] == 'Q') stopped = 1;
                if (keys[i] == ' ') { paused = !paused; last_time = deadline = Clock::now(); }
                if (keys[i] == 'r') { elapsed = 0; previous_frame = {}; }
            }
        }
    }
    return 0;
}
} // namespace bmatrix
