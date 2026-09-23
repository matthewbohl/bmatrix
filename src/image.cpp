#include "bmatrix/core.hpp"

#include <png.h>

#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace bmatrix {
namespace {
constexpr std::size_t max_pixels = 32U * 1024U * 1024U;
void dimensions(unsigned width, unsigned height) {
    if (width == 0 || height == 0 || width > 16384 || height > 16384 ||
        static_cast<std::size_t>(width) * height > max_pixels) {
        throw std::runtime_error("image exceeds limits (16384 per side, 32 megapixels)");
    }
}
struct PngGuard {
    png_image value{};
    PngGuard() { value.version = PNG_IMAGE_VERSION; }
    ~PngGuard() { png_image_free(&value); }
};
std::string token(std::istream& stream) {
    std::string result;
    while (stream) {
        const int c = stream.peek();
        if (c == '#') {
            stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else if (c != EOF && std::isspace(static_cast<unsigned char>(c))) {
            stream.get();
        } else {
            break;
        }
    }
    while (stream) {
        const int c = stream.peek();
        if (c == EOF || c == '#' || std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
        if (result.size() > 16) {
            throw std::runtime_error("invalid PPM header");
        }
        result.push_back(static_cast<char>(stream.get()));
    }
    return result;
}
unsigned number(std::istream& stream) {
    const auto value = token(stream);
    unsigned result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::runtime_error("invalid PPM header");
    }
    return result;
}
} // namespace

Image load_image(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open image: " + path.string());
    }
    std::array<unsigned char, 8> signature{};
    input.read(reinterpret_cast<char*>(signature.data()), signature.size());
    input.clear();
    input.seekg(0);
    if (png_sig_cmp(signature.data(), 0, signature.size()) == 0) {
        PngGuard png;
        if (!png_image_begin_read_from_file(&png.value, path.c_str())) {
            throw std::runtime_error(std::string("PNG: ") + png.value.message);
        }
        dimensions(png.value.width, png.value.height);
        png.value.format = PNG_FORMAT_RGBA;
        std::vector<png_byte> rgba(PNG_IMAGE_SIZE(png.value));
        if (!png_image_finish_read(&png.value, nullptr, rgba.data(), 0, nullptr)) {
            throw std::runtime_error(std::string("PNG: ") + png.value.message);
        }
        Image image{static_cast<int>(png.value.width), static_cast<int>(png.value.height), {}};
        image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);
        for (std::size_t i = 0; i < image.pixels.size(); ++i) {
            const unsigned alpha = rgba[i * 4 + 3];
            image.pixels[i] = {
                static_cast<std::uint8_t>((rgba[i * 4] * alpha + 127) / 255),
                static_cast<std::uint8_t>((rgba[i * 4 + 1] * alpha + 127) / 255),
                static_cast<std::uint8_t>((rgba[i * 4 + 2] * alpha + 127) / 255)};
        }
        return image;
    }
    if (token(input) != "P6") {
        throw std::runtime_error("unsupported image; milestone 1 accepts PNG and binary PPM (P6)");
    }
    const unsigned width = number(input), height = number(input);
    dimensions(width, height);
    if (number(input) != 255) {
        throw std::runtime_error("PPM requires max value 255");
    }
    const int delimiter = input.get();
    if (delimiter == EOF || !std::isspace(static_cast<unsigned char>(delimiter))) {
        throw std::runtime_error("invalid PPM pixel delimiter");
    }
    Image image{static_cast<int>(width), static_cast<int>(height), {}};
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    static_assert(sizeof(Rgb) == 3);
    input.read(reinterpret_cast<char*>(image.pixels.data()),
               static_cast<std::streamsize>(image.pixels.size() * sizeof(Rgb)));
    if (!input) {
        throw std::runtime_error("truncated PPM image");
    }
    return image;
}

Image demo_image() {
    Image image{320, 200, {}};
    image.pixels.resize(320 * 200);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const double dx = (x - 160.0) / 80.0, dy = (y - 100.0) / 80.0;
            const double radius = dx * dx + dy * dy;
            Rgb color{static_cast<std::uint8_t>(20 + x / 3),
                      static_cast<std::uint8_t>(12 + y / 3), 35};
            if (radius < 1.0) {
                color = {static_cast<std::uint8_t>(60 + x / 2),
                         static_cast<std::uint8_t>(90 + (200 - y) / 2), 220};
            }
            if ((x > 117 && x < 136 && y > 65 && y < 84) ||
                (x > 185 && x < 204 && y > 65 && y < 84) ||
                (y > 120 && y < 136 && x > 126 && x < 195)) {
                color = {245, 245, 220};
            }
            image.pixels[static_cast<std::size_t>(y) * image.width + x] = color;
        }
    }
    return image;
}
} // namespace bmatrix
