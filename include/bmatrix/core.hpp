#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace bmatrix {
struct Rgb {
    std::uint8_t r{}, g{}, b{};
    bool operator==(const Rgb&) const = default;
};
struct Image {
    int width{}, height{};
    std::vector<Rgb> pixels;
};
enum class Palette { green, mono, full };
struct Cell {
    char glyph{' '};
    Rgb color{};
    bool operator==(const Cell&) const = default;
};
struct Frame {
    int width{}, height{};
    std::vector<Cell> cells;
    bool operator==(const Frame&) const = default;
};
struct RenderOptions {
    int width{80}, height{24};
    double cell_aspect{0.5}; // cell width / height
    Palette palette{Palette::green};
    std::uint32_t seed{1};
    double image_strength{0.70};
    // Pure image-space motion inputs. They are set only for the Matrix effect.
    double image_time{0};
    bool image_glitch{false};
};
Image load_image(const std::filesystem::path& path);
Image demo_image();
Frame render_image(const Image& image, const RenderOptions& options);
// Legacy finite reveal retained for reproducible transition snapshots.
Frame flow_down(const Frame& target, double progress, std::uint32_t seed);
// A perpetual Matrix field whose density and color carry a low-contrast image signal.
Frame matrix_rain(const Frame& image_signal, double seconds, std::uint32_t seed, Palette palette,
                  double image_strength);
std::string plain_text(const Frame& frame);
std::string json_frame(const Frame& frame);
} // namespace bmatrix
