#include "bmatrix/core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace bmatrix {
namespace {
std::uint32_t hash(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}
std::uint8_t byte(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}
Rgb sample(const Image& image, double x0, double y0, double x1, double y1) {
    double r = 0, g = 0, b = 0, total = 0;
    for (int y = static_cast<int>(std::floor(y0)); y < std::min(image.height, static_cast<int>(std::ceil(y1))); ++y) {
        const double wy = std::max(0.0, std::min(y1, y + 1.0) - std::max(y0, static_cast<double>(y)));
        for (int x = static_cast<int>(std::floor(x0)); x < std::min(image.width, static_cast<int>(std::ceil(x1))); ++x) {
            const double weight = wy * std::max(0.0, std::min(x1, x + 1.0) - std::max(x0, static_cast<double>(x)));
            const auto pixel = image.pixels[static_cast<std::size_t>(y) * image.width + x];
            r += pixel.r * weight;
            g += pixel.g * weight;
            b += pixel.b * weight;
            total += weight;
        }
    }
    return {byte(r / total), byte(g / total), byte(b / total)};
}
void validate(const Frame& frame) {
    if (frame.width < 1 || frame.height < 1 || frame.width > 500 || frame.height > 200 ||
        frame.cells.size() != static_cast<std::size_t>(frame.width) * frame.height) {
        throw std::invalid_argument("invalid frame dimensions");
    }
}
double luminance(Rgb color) {
    return (0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b) / 255.0;
}
double source_tone(const Frame& frame, int x, int y) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) return 0.0;
    const auto cell = frame.cells[static_cast<std::size_t>(y) * frame.width + x];
    return cell.glyph == ' ' ? 0.0 : luminance(cell.color);
}
double edge_tone(const Frame& frame, int x, int y) {
    return std::abs(source_tone(frame, x - 1, y) - source_tone(frame, x + 1, y)) +
           std::abs(source_tone(frame, x, y - 1) - source_tone(frame, x, y + 1));
}
double image_tone(const Frame& frame, int x, int y) {
    const double center = source_tone(frame, x, y);
    const double edge = edge_tone(frame, x, y);
    // The image lives mainly in the density of the field. Dark subjects become
    // calm negative space within the rain; edges add just enough recognition.
    return std::clamp(center * 0.70 + edge * 0.22, 0.0, 0.72);
}
double glitch_offset(std::uint32_t seed, int row, double seconds) {
    // Adjacent row bands drift independently. Smooth interpolation prevents a
    // full-frame snap while still reading as an occasional analog-style glitch.
    constexpr double rate = 1.4;
    const int band = row / 3;
    const double phase = seconds * rate + band * 0.37;
    const auto epoch = static_cast<int>(std::floor(phase));
    const double local = phase - epoch;
    const auto offset_at = [seed, band](int time_slice) {
        const auto value = hash(seed ^ static_cast<std::uint32_t>(band) * 0x9e3779b9U ^
                                static_cast<std::uint32_t>(time_slice) * 0x85ebca6bU);
        return ((value & 0xffffU) / 65535.0 * 2.0 - 1.0) * 3.25;
    };
    const double eased = local * local * (3.0 - 2.0 * local);
    return offset_at(epoch) * (1.0 - eased) + offset_at(epoch + 1) * eased;
}
double persistent_tone(const Frame& frame, int x, int y, Palette palette) {
    const auto cell = frame.cells[static_cast<std::size_t>(y) * frame.width + x];
    if (cell.glyph == ' ') return 0;
    const double palette_peak = palette == Palette::green ? 0.724586 : 1.0;
    return std::clamp(luminance(cell.color) / palette_peak * 0.85 + edge_tone(frame, x, y) * 1.80,
                      0.0, 1.0);
}
} // namespace

Frame render_image(const Image& image, const RenderOptions& options) {
    if (image.width <= 0 || image.height <= 0 || image.width > 16384 || image.height > 16384 ||
        image.pixels.size() != static_cast<std::size_t>(image.width) * image.height) {
        throw std::invalid_argument("invalid source image");
    }
    if (options.width < 1 || options.width > 500 || options.height < 1 || options.height > 200 ||
        !std::isfinite(options.cell_aspect) || !std::isfinite(options.image_time) ||
        options.cell_aspect < 0.2 || options.cell_aspect > 2) {
        throw std::invalid_argument("invalid render dimensions or cell aspect");
    }
    Frame frame{options.width, options.height, {}};
    frame.cells.resize(static_cast<std::size_t>(frame.width) * frame.height);
    const double ratio = static_cast<double>(image.width) / image.height / options.cell_aspect;
    const int width = std::max(1, std::min(frame.width, static_cast<int>(std::lround(frame.height * ratio))));
    const int height = std::max(1, std::min(frame.height, static_cast<int>(std::lround(width / ratio))));
    const int left = (frame.width - width) / 2, top = (frame.height - height) / 2;
    // Approximate ink coverage bands; select stable variants to avoid contour
    // stripes without introducing frame-to-frame glyph flicker.
    constexpr std::array<std::string_view, 17> ramp{
        " ", "`", ".'-", ",_", "~^", ":!", "=r+;", "/\\()<>", "cl?|",
        "sit[]vLz", "f7jx{}TJ", "*CnuY1", "IoF3ye2", "a5ShkwV%Z4",
        "EXdpPmbGq$96", "UAOg&K8HD#QR", "0BW@MN"};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double shifted_x = std::clamp(static_cast<double>(x) +
                                                    (options.image_glitch ? glitch_offset(options.seed, y + top, options.image_time) : 0.0),
                                                0.0, static_cast<double>(width - 1));
            const auto rgb = sample(image, shifted_x * image.width / width,
                                    static_cast<double>(y) * image.height / height,
                                    std::min(static_cast<double>(width), shifted_x + 1.0) * image.width / width,
                                    static_cast<double>(y + 1) * image.height / height);
            const double light = (0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b) / 255.0;
            const double tone = std::pow(std::clamp((light - 0.02) * 1.06, 0.0, 1.0), 0.8);
            const auto index = static_cast<std::size_t>(std::lround(tone * (ramp.size() - 1)));
            const auto brightness = byte(32 + 223 * std::pow(light, 0.8));
            Rgb color{};
            switch (options.palette) {
            case Palette::green: color = {0, brightness, byte(brightness * 0.13)}; break;
            case Palette::mono: color = {brightness, brightness, brightness}; break;
            case Palette::full: {
                constexpr double gain = 1.2;
                color = {byte(rgb.r * gain), byte(rgb.g * gain), byte(rgb.b * gain)};
                break;
            }
            }
            const auto position = static_cast<std::size_t>(y + top) * frame.width + x + left;
            const auto variants = ramp[index];
            const char glyph = variants[hash(static_cast<std::uint32_t>(position) ^ options.seed) % variants.size()];
            frame.cells[position] = {glyph, color};
        }
    }
    return frame;
}

Frame flow_down(const Frame& target, double progress, std::uint32_t seed) {
    validate(target);
    if (!std::isfinite(progress)) {
        throw std::invalid_argument("progress must be finite");
    }
    if (progress >= 1) {
        return target;
    }
    Frame output{target.width, target.height, std::vector<Cell>(target.cells.size())};
    if (progress <= 0) {
        return output;
    }
    constexpr std::string_view rain = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:;=+*";
    for (int x = 0; x < target.width; ++x) {
        const double jitter = (hash(static_cast<std::uint32_t>(x) ^ seed) % 1000) / 1000.0;
        const double delay = jitter * 0.20;
        const double local = std::clamp((progress - delay) / 0.8, 0.0, 1.0);
        const double head = local * (target.height + 10.0) - 5;
        for (int y = 0; y < target.height; ++y) {
            const auto index = static_cast<std::size_t>(y) * target.width + x;
            const double behind = head - y;
            if (behind < 0) {
                continue;
            }
            const double reveal = std::clamp(behind / 4.0, 0.0, 1.0);
            Cell cell = target.cells[index];
            if (reveal < 1 && cell.glyph != ' ') {
                cell.glyph = rain[hash(static_cast<std::uint32_t>(index) ^ seed) % rain.size()];
                // A restrained bright leading edge fades into stable image glyphs.
                const double glow = (1 - reveal) * 0.7;
                cell.color = {byte(cell.color.r + (255 - cell.color.r) * glow),
                              byte(cell.color.g + (255 - cell.color.g) * glow),
                              byte(cell.color.b + (255 - cell.color.b) * glow)};
            }
            output.cells[index] = cell;
        }
    }
    return output;
}

Frame matrix_rain(const Frame& image_signal, double seconds, std::uint32_t seed, Palette palette,
                  double image_strength) {
    validate(image_signal);
    if (!std::isfinite(seconds)) {
        throw std::invalid_argument("time must be finite");
    }
    if (!std::isfinite(image_strength) || image_strength < 0 || image_strength > 1) {
        throw std::invalid_argument("image strength must be between zero and one");
    }
    Frame output{image_signal.width, image_signal.height, std::vector<Cell>(image_signal.cells.size())};
    std::vector<double> row_tones(static_cast<std::size_t>(image_signal.height));
    double all_tones = 0;
    for (int y = 0; y < image_signal.height; ++y) {
        for (int x = 0; x < image_signal.width; x += 2) {
            const double tone = persistent_tone(image_signal, x, y, palette);
            row_tones[static_cast<std::size_t>(y)] += tone;
            all_tones += tone;
        }
    }
    // Divide by all stream columns, including empty ones. This balances both
    // image brightness and image occupancy, which is what the eye perceives.
    const double stream_columns = static_cast<double>((image_signal.width + 1) / 2);
    const double average_tone = all_tones / (stream_columns * image_signal.height);
    std::vector<double> row_balances(static_cast<std::size_t>(image_signal.height));
    std::vector<double> row_photo_density(static_cast<std::size_t>(image_signal.height));
    std::vector<int> row_left(static_cast<std::size_t>(image_signal.height), image_signal.width);
    std::vector<int> row_right(static_cast<std::size_t>(image_signal.height), -1);
    for (int y = 0; y < image_signal.height; ++y) {
        const auto row = static_cast<std::size_t>(y);
        const double row_average = row_tones[row] / stream_columns;
        row_balances[row] = row_average > 0 ? std::clamp(average_tone / row_average, 0.40, 1.60) : 1.0;
        double coverage_sum = 0;
        unsigned coverage_count = 0;
        for (int x = 0; x < image_signal.width; x += 2) {
            const auto cell = image_signal.cells[static_cast<std::size_t>(y) * image_signal.width + x];
            if (cell.glyph == ' ') continue;
            row_left[row] = std::min(row_left[row], x);
            row_right[row] = std::max(row_right[row], x);
            const double coverage = persistent_tone(image_signal, x, y, palette) * row_balances[row] *
                                    (0.10 + image_strength * 0.45);
            coverage_sum += std::clamp(coverage, 0.0, 1.0);
            ++coverage_count;
        }
        row_photo_density[row] = coverage_count && image_strength > 0 ? coverage_sum / coverage_count : 0;
    }
    constexpr double speed = 10.5;
    // Advancing the glyph coordinate makes characters travel with the stream.
    // It avoids the noisy whole-screen randomization that breaks the illusion of rain.
    const auto glyph_row_offset = static_cast<int>(std::floor(seconds * speed));
    for (int x = 0; x < image_signal.width; ++x) {
        // CMatrix's normal terminal mode advances j by two, leaving a column of
        // air beside every stream. Preserve that pacing and visual resolution.
        if ((x & 1) != 0) {
            continue;
        }
        const auto column_seed = hash(static_cast<std::uint32_t>(x) ^ seed);
        const double period = 10.0 + column_seed % 12;
        // Start every column above the screen. The image is only exposed after
        // its falling head has passed, so startup reads like CMatrix rather
        // than a finished picture with code layered on top.
        const double entry_delay = 1.0 + (column_seed % 10000) / 10000.0 *
                                             std::min(18.0, image_signal.height * 0.70 + 5.0);
        const double travelled = seconds * speed - entry_delay;
        const double cycle = image_signal.height + period;
        // The first head continues all the way down at a single velocity.
        // Independent ambient trails fade in behind it rather than replacing
        // it abruptly, which avoids a visible jump during startup.
        const double startup_depth = std::min(8.0, std::max(3.0, image_signal.height * 0.25));
        const double phase_offset = (column_seed % 10000) / 10000.0 * cycle;
        const double ambient_head = std::fmod(seconds * speed + phase_offset, cycle);
        const double fade_progress = std::clamp((travelled - startup_depth) / 12.0, 0.0, 1.0);
        const double ambient_fade = fade_progress * fade_progress * (3.0 - 2.0 * fade_progress);
        for (int y = 0; y < image_signal.height; ++y) {
            const auto index = static_cast<std::size_t>(y) * image_signal.width + x;
            const auto& source = image_signal.cells[index];
            const auto base_seed = hash(static_cast<std::uint32_t>(index) ^ seed);
            // The still, dim layer is the visual anchor for the concealed image.
            // It gives the eye time to assemble the subject while brighter streams
            // continually travel over it.
            if (image_strength > 0 && source.glyph != ' ' && travelled >= y) {
                Rgb base{};
                // This is the always-present photographic layer. At zero it
                // is entirely absent; the default leaves enough light for a
                // relaxed gaze to recover the subject beneath the rain.
                const double ghost = 240 * image_strength;
                // A small, persistent edge lift keeps faces, silhouettes, and
                // object boundaries recognizable after terminal downsampling.
                const double ghost_tone = persistent_tone(image_signal, x, y, palette);
                if (palette == Palette::mono) {
                    const auto shade = byte(ghost_tone * ghost);
                    base = {shade, shade, shade};
                } else if (palette == Palette::green) {
                    base = {0, byte(ghost_tone * ghost), byte(ghost_tone * ghost * 0.20)};
                } else {
                    const double scale = image_strength * (0.55 + ghost_tone * 0.28);
                    base = {byte(source.color.r * scale), byte(source.color.g * scale), byte(source.color.b * scale)};
                }
                // Stable coverage conveys image mass as well as luminance:
                // bright areas have more fixed ink while dark subjects form
                // quiet negative space. It is dithered once, never flickers.
                const auto row = static_cast<std::size_t>(y);
                // Row normalization prevents bright sky/wall regions or dark,
                // detailed lower regions from monopolizing the fixed image.
                const double coverage = std::clamp(ghost_tone * row_balances[row] *
                                                   (0.10 + image_strength * 0.45), 0.0, 1.0);
                if ((base_seed & 0xffffU) / 65535.0 <= coverage) {
                    output.cells[index] = {static_cast<char>(33U + base_seed % 90U), base};
                }
            }
            double ambient_distance = ambient_head - y;
            if (ambient_distance < 0) ambient_distance += cycle;
            const double ambient_stream = ambient_distance < 28.0 ? 1.0 - ambient_distance / 31.0 : 0.0;
            const double first_distance = travelled - y;
            // The tail keeps moving after its head leaves the bottom. Removing
            // the entire first stream at head exit would erase visible rows.
            const double first_stream = first_distance >= 0 && first_distance < 28.0
                ? 1.0 - first_distance / 31.0 : 0.0;
            // The first descent remains continuous. Ambient trails make the
            // field whole as they fade in, then own the recurring animation.
            const double stream = std::max(first_stream, ambient_stream * ambient_fade);
            const double head_glow = std::max(first_stream > 0 ? std::exp(-first_distance * first_distance * 0.42) : 0.0,
                                              std::exp(-ambient_distance * ambient_distance * 0.42) * ambient_fade);
            const auto row = static_cast<std::size_t>(y);
            const double signal = image_tone(image_signal, x, y) * image_strength * row_balances[row];
            // Outside the fitted image bounds, use the row's average photo
            // coverage rather than a low global floor. This carries the visual
            // weight into side bars without stretching the source image.
            const bool in_letterbox = x < row_left[row] || x > row_right[row];
            const double fitted_row_density = in_letterbox ? row_photo_density[row] : 0.0;
            const auto moving_row = static_cast<std::uint32_t>(y - glyph_row_offset);
            const auto cell_seed = hash(static_cast<std::uint32_t>(x) * 0x9e3779b9U ^
                                        moving_row * 0x85ebca6bU ^ seed);
            const double noise = (cell_seed & 0xffffU) / 65535.0;
            // Motion belongs to the falling trails, rather than becoming a
            // second noisy copy of the entire photo.  That leaves the stable
            // layer available to the eye between passing streams.
            // Ambient rain exists across the full terminal. The photo merely
            // modulates this field within its aspect-preserved bounds, so
            // letterboxed side bars never become empty black panels.
            const double density = std::clamp(first_stream * 0.76 +
                                              ambient_fade * (0.100 + ambient_stream * 0.76 + signal * 0.08 + fitted_row_density),
                                              0.0, 0.90);
            if (density <= 0 || noise > density) {
                continue;
            }
            const double level = std::clamp(0.04 + stream * 0.67 + signal * 0.14 + head_glow * 0.42, 0.0, 1.0);
            Rgb color{};
            if (palette == Palette::mono) {
                const auto shade = byte(35 + 215 * level);
                color = {shade, shade, shade};
            } else if (palette == Palette::green) {
                color = {0, byte(35 + 205 * level), byte(8 + 48 * level)};
            } else {
                // Green is the ambient field. In full mode source hue leaks through
                // at low opacity, preserving the concealed-image effect.
                const double source_weight = std::clamp(signal * 1.7 + stream * image_strength * 0.08, 0.0, 0.58);
                const Rgb green{0, byte(35 + 205 * level), byte(8 + 48 * level)};
                color = {byte(green.r * (1 - source_weight) + source.color.r * source_weight * level),
                         byte(green.g * (1 - source_weight) + source.color.g * source_weight * level),
                         byte(green.b * (1 - source_weight) + source.color.b * source_weight * level)};
            }
            if (head_glow > 0.62) {
                color = {byte(color.r + (255 - color.r) * 0.34),
                         byte(color.g + (255 - color.g) * 0.34),
                         byte(color.b + (255 - color.b) * 0.34)};
            }
            // CMatrix's normal terminal character range is ASCII 33 through 122.
            output.cells[index] = {static_cast<char>(33U + cell_seed % 90U), color};
        }
    }
    return output;
}

std::string plain_text(const Frame& frame) {
    validate(frame);
    std::string output;
    output.reserve(frame.cells.size() + frame.height);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            output += frame.cells[static_cast<std::size_t>(y) * frame.width + x].glyph;
        }
        output += '\n';
    }
    return output;
}

std::string json_frame(const Frame& frame) {
    validate(frame);
    std::ostringstream output;
    output << "{\"width\":" << frame.width << ",\"height\":" << frame.height << ",\"cells\":[";
    bool first = true;
    for (const auto& cell : frame.cells) {
        if (!first) {
            output << ',';
        }
        first = false;
        // Numeric ASCII code avoids quoting ambiguities in test and preview consumers.
        output << '[' << static_cast<unsigned>(static_cast<unsigned char>(cell.glyph)) << ','
               << static_cast<unsigned>(cell.color.r) << ',' << static_cast<unsigned>(cell.color.g)
               << ',' << static_cast<unsigned>(cell.color.b) << ']';
    }
    output << "]}\n";
    return output.str();
}
} // namespace bmatrix
