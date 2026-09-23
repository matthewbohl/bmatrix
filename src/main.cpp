#include "bmatrix/core.hpp"
#include "terminal.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unistd.h>
#ifndef BMATRIX_VERSION
#define BMATRIX_VERSION "0.1.0"
#endif

namespace {
double number(const std::string& value, double low, double high) {
    std::size_t consumed{};
    const double result = std::stod(value, &consumed);
    if (consumed != value.size() || !std::isfinite(result) || result < low || result > high)
        throw std::runtime_error("numeric argument out of range: " + value);
    return result;
}
unsigned long integer(const std::string& value, unsigned long high) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("expected unsigned integer: " + value);
    const auto result = std::stoul(value);
    if (result > high) throw std::runtime_error("integer argument out of range");
    return result;
}
}
int main(int argc, char** argv) {
    try {
        bmatrix::SessionOptions options;
        std::string path, output = isatty(STDOUT_FILENO) ? "terminal" : "plain";
        double progress = 1;
        double time = 0;
        bool demo = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (++i == argc) throw std::runtime_error("missing value for " + arg);
                return argv[i];
            };
            if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: bmatrix [IMAGE] [options]\n"
                    "  --demo                    Built-in image (also default without IMAGE)\n"
                    "  --mode green|mono|full     Artistic palette; default green\n"
                    "  --color auto|none|16|256|truecolor\n"
                    "  --size COLSxROWS           Maximum 500x200\n"
                    "  --fps N                   1..120; default 30\n"
                    "  --duration SECONDS        Legacy flow reveal duration .1..60; default 3\n"
                    "  --frames N                Stop after N frames\n"
                    "  --seed N                  Deterministic seed\n"
                    "  --cell-aspect N           Cell width/height; default .5\n"
                    "  --image-strength N        Concealed image strength 0..1; default .7\n"
                    "  --effect matrix|flow|none Matrix rain is the default\n"
                    "  --output terminal|plain|json\n"
                    "  --progress N              Snapshot reveal progress 0..1\n"
                    "  --time SECONDS            Deterministic matrix snapshot time\n"
                    "  --version                 Print version\n"
                    "Keys: q quit, space pause, r replay.\n";
                return 0;
            } else if (arg == "--version") { std::cout << "bmatrix " BMATRIX_VERSION "\n"; return 0; }
            else if (arg == "--demo") demo = true;
            else if (arg == "--mode") {
                const auto mode = value();
                if (mode == "green") options.render.palette = bmatrix::Palette::green;
                else if (mode == "mono") options.render.palette = bmatrix::Palette::mono;
                else if (mode == "full") options.render.palette = bmatrix::Palette::full;
                else throw std::runtime_error("unknown artistic mode: " + mode);
            } else if (arg == "--color") {
                options.color = value();
                if (options.color != "auto" && options.color != "none" && options.color != "16" && options.color != "256" && options.color != "truecolor")
                    throw std::runtime_error("unknown color capability");
            } else if (arg == "--size") {
                const auto size = value(); const auto x = size.find('x');
                if (x == std::string::npos) throw std::runtime_error("size must be COLSxROWS");
                options.render.width = static_cast<int>(integer(size.substr(0, x), 500));
                options.render.height = static_cast<int>(integer(size.substr(x + 1), 200));
                if (!options.render.width || !options.render.height) throw std::runtime_error("size must be positive");
                options.fixed_size = true;
            } else if (arg == "--fps") options.fps = number(value(), 1, 120);
            else if (arg == "--duration") options.duration = number(value(), .1, 60);
            else if (arg == "--frames") {
                options.frames = integer(value(), 10000000);
                if (!options.frames) throw std::runtime_error("frames must be positive");
            } else if (arg == "--seed") options.render.seed = static_cast<std::uint32_t>(integer(value(), std::numeric_limits<std::uint32_t>::max()));
            else if (arg == "--cell-aspect") options.render.cell_aspect = number(value(), .2, 2);
            else if (arg == "--image-strength") options.render.image_strength = number(value(), 0, 1);
            else if (arg == "--effect") {
                const auto effect = value();
                if (effect == "matrix") options.effect = bmatrix::Effect::matrix;
                else if (effect == "flow") options.effect = bmatrix::Effect::flow;
                else if (effect == "none") options.effect = bmatrix::Effect::still;
                else throw std::runtime_error("unknown effect");
            } else if (arg == "--output") {
                output = value();
                if (output != "terminal" && output != "plain" && output != "json") throw std::runtime_error("unknown output format");
            } else if (arg == "--progress") progress = number(value(), 0, 1);
            else if (arg == "--time") time = number(value(), -3600, 3600);
            else if (!arg.empty() && arg[0] == '-') throw std::runtime_error("unknown option: " + arg);
            else {
                if (!path.empty()) throw std::runtime_error("only one input image is supported");
                path = arg;
            }
        }
        if (demo && !path.empty()) throw std::runtime_error("--demo cannot be combined with IMAGE");
        const auto image = path.empty() ? bmatrix::demo_image() : bmatrix::load_image(path);
        if (output == "terminal") {
            if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO)) throw std::runtime_error("terminal output requires a terminal on stdin and stdout");
            return bmatrix::run_terminal(image, options);
        }
        auto frame = bmatrix::render_image(image, options.render);
        if (options.effect == bmatrix::Effect::matrix) frame = bmatrix::matrix_rain(frame, time, options.render.seed, options.render.palette, options.render.image_strength);
        else if (options.effect == bmatrix::Effect::flow) frame = bmatrix::flow_down(frame, progress, options.render.seed);
        std::cout << (output == "json" ? bmatrix::json_frame(frame) : bmatrix::plain_text(frame));
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "bmatrix: " << error.what() << '\n'; return 1;
    }
}
