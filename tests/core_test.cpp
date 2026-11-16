#include "bmatrix/core.hpp"
#include <png.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}
template <typename F> void rejects(F function, const char* message) {
    bool caught = false;
    try { function(); } catch (const std::exception&) { caught = true; }
    require(caught, message);
}
struct Temp {
    std::filesystem::path path;
    Temp() {
        auto name = (std::filesystem::temp_directory_path() / "bmatrix-tests-XXXXXX").string();
        if (!mkdtemp(name.data())) { throw std::runtime_error("mkdtemp failed"); }
        path = name;
    }
    ~Temp() { std::error_code error; std::filesystem::remove_all(path, error); }
};
}

int main() {
    using namespace bmatrix;
    try {
        const Image white{1, 1, {{255, 255, 255}}};
        RenderOptions options{80, 24, 0.5, Palette::green, 7};
        const auto frame = render_image(white, options);
        require(frame.cells.size() == 1920, "frame size");
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 80; ++x) {
                const auto cell = frame.cells[static_cast<std::size_t>(y) * 80 + x];
                require((cell.glyph != ' ') == (x >= 16 && x < 64), "square aspect fit and centering");
                if (cell.glyph != ' ') {
                    require(cell.color.g > cell.color.r && cell.color.g > cell.color.b, "default green palette");
                }
            }
        }
        auto mono_options = options;
        mono_options.palette = Palette::mono;
        const auto mono = render_image(white, mono_options);
        for (const auto cell : mono.cells) {
            require(cell.color.r == cell.color.g && cell.color.g == cell.color.b, "monochrome equal channels");
        }
        auto full_options = options;
        full_options.palette = Palette::full;
        const auto red = render_image({1, 1, {{255, 0, 0}}}, full_options);
        require(red.cells[40].color.r > 0 && red.cells[40].color.g == 0 && red.cells[40].color.b == 0,
                "full color preserves source hue");
        const auto black = render_image({1, 1, {{0, 0, 0}}}, options);
        require(std::all_of(black.cells.begin(), black.cells.end(), [](Cell c) { return c.glyph == ' '; }), "black uses spaces");
        require(render_image(white, options) == frame, "stable repeated rendering");
        require(flow_down(frame, 1, 7) == frame, "exact completed endpoint");
        require(flow_down(frame, 2, 7) == frame, "clamped completed endpoint");
        const auto start = flow_down(frame, 0, 7);
        require(std::all_of(start.cells.begin(), start.cells.end(), [](Cell c) { return c.glyph == ' '; }), "empty start");
        require(flow_down(frame, 0.5, 7) == flow_down(frame, 0.5, 7), "deterministic transition");
        require(flow_down(frame, 0.5, 7) != flow_down(frame, 0.5, 8), "seed affects column timing");
        require(flow_down(frame, 0.5, 7) != frame && flow_down(frame, 0.5, 7) != start, "intermediate frame");
        for (const auto cell : flow_down(frame, 0.5, 7).cells) {
            require(cell.glyph >= 32 && cell.glyph <= 126, "single-column ASCII only");
        }
        const auto text = plain_text(frame);
        require(text.size() == 81 * 24 && std::count(text.begin(), text.end(), '\n') == 24, "plain dimensions");
        require(json_frame(frame).find("\"width\":80") != std::string::npos, "JSON frame metadata");
        rejects([&] { auto invalid = options; invalid.width = 501; render_image(white, invalid); }, "oversized frame rejected");
        rejects([&] { auto invalid = options; invalid.cell_aspect = std::nan(""); render_image(white, invalid); }, "nonfinite aspect rejected");
        rejects([&] { render_image({2, 2, {}}, options); }, "invalid image buffer rejected");
        rejects([&] { flow_down(frame, std::numeric_limits<double>::infinity(), 1); }, "nonfinite progress rejected");
        const auto tiny = render_image(white, {1, 1, 0.5, Palette::green, 1});
        require(tiny.cells.size() == 1, "tiny frame");
        const Image primaries{3, 1, {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}}};
        const auto mixed = render_image(primaries, {2, 1, 1.5, Palette::full, 1});
        require(mixed.cells[0].color == Rgb{204, 102, 0} && mixed.cells[1].color == Rgb{0, 102, 204},
                "fractional area sampling preserves weighted edge pixels");
        Image stripes{30, 2, {}};
        for (int y = 0; y < stripes.height; ++y) {
            for (int x = 0; x < stripes.width; ++x) {
                stripes.pixels.push_back((x / 2) % 2 ? Rgb{255, 255, 255} : Rgb{});
            }
        }
        RenderOptions glitch_options{30, 12, 0.5, Palette::mono, 4};
        glitch_options.image_glitch = true;
        glitch_options.image_time = .0;
        const auto glitch_start = render_image(stripes, glitch_options);
        glitch_options.image_time = .6;
        const auto glitch_later = render_image(stripes, glitch_options);
        require(glitch_start != glitch_later, "image-space glitch shifts row bands over time");
        require(glitch_later == render_image(stripes, glitch_options), "glitch snapshot is deterministic");
        glitch_options.image_glitch = false;
        require(render_image(stripes, glitch_options) == render_image(stripes, glitch_options),
                "disabled image glitch remains stable");
        const auto portrait = render_image({1, 10, std::vector<Rgb>(10, {255, 255, 255})},
                                           {40, 20, 0.5, Palette::mono, 1});
        require(std::count_if(portrait.cells.begin(), portrait.cells.end(), [](Cell c) { return c.glyph != ' '; }) == 80,
                "portrait fitting preserves aspect");
        require(portrait.cells[17].glyph == ' ' && portrait.cells[18].glyph != ' ' && portrait.cells[22].glyph == ' ',
                "portrait centered");
        const auto portrait_rain = matrix_rain(portrait, 5, 11, Palette::mono, .70);
        unsigned photo_ink = 0, side_ink = 0, side_columns = 0;
        for (int y = 0; y < portrait_rain.height; ++y) {
            for (int x = 0; x < portrait_rain.width; x += 2) {
                const bool ink = portrait_rain.cells[static_cast<std::size_t>(y) * portrait_rain.width + x].glyph != ' ';
                if (x == 18 || x == 20) photo_ink += ink;
                else { side_ink += ink; ++side_columns; }
            }
        }
        const unsigned photo_columns = static_cast<unsigned>(portrait_rain.height) * 2;
        require(side_ink * photo_columns >= photo_ink * side_columns / 2,
                "letterbox rain follows fitted image row density");
        const auto middle = flow_down(frame, 0.4, 7), later = flow_down(frame, 0.6, 7);
        for (std::size_t i = 0; i < frame.cells.size(); ++i) {
            if (middle.cells[i] == frame.cells[i]) {
                require(later.cells[i] == frame.cells[i], "settled cells remain stable");
            }
        }
        require(flow_down(frame, -1, 7) == start, "negative progress clamps to empty");
        require(flow_down(frame, 0.9999, 7) == frame, "no final frame jump");
        const auto rain0 = matrix_rain(frame, 0, 7, Palette::green, .70);
        const auto rain1 = matrix_rain(frame, 1, 7, Palette::green, .70);
        require(std::all_of(rain0.cells.begin(), rain0.cells.end(), [](Cell cell) { return cell.glyph == ' '; }),
                "matrix starts empty above the terminal");
        require(rain0 != rain1, "matrix field advances downward from startup");
        require(rain0 == matrix_rain(frame, 0, 7, Palette::green, .70), "matrix snapshot is deterministic");
        require(rain1 != matrix_rain(frame, 1, 8, Palette::green, .70), "matrix seed affects streams");
        require(std::any_of(rain1.cells.begin(), rain1.cells.end(), [](Cell cell) { return cell.glyph != ' '; }),
                "matrix field has visible streams after heads enter");
        const auto settled_rain = matrix_rain(frame, 5, 7, Palette::green, .70);
        const auto ghost_cells = static_cast<std::size_t>(std::count_if(settled_rain.cells.begin(), settled_rain.cells.end(), [](Cell cell) {
            return cell.glyph != ' ';
        }));
        require(ghost_cells > 100 && ghost_cells < rain0.cells.size() / 2,
                "persistent image uses restrained stable coverage");
        for (const auto cell : settled_rain.cells) {
            require(cell.glyph == ' ' || (cell.glyph >= '!' && cell.glyph <= 'z'),
                    "matrix matches CMatrix normal ASCII character range");
            if (cell.glyph != ' ') {
                require(cell.color.g > cell.color.r && cell.color.g > cell.color.b, "matrix green remains green");
            }
        }
        for (int y = 0; y < settled_rain.height; ++y) {
            for (int x = 1; x < settled_rain.width; x += 2) {
                require(settled_rain.cells[static_cast<std::size_t>(y) * settled_rain.width + x].glyph == ' ',
                        "matrix matches CMatrix two-column stream spacing");
            }
        }
        const auto no_image = matrix_rain(frame, 5, 7, Palette::green, 0);
        const auto strong_image = matrix_rain(frame, 5, 7, Palette::green, 1);
        const auto green_total = [](const Frame& candidate) {
            unsigned total = 0;
            for (const auto cell : candidate.cells) total += cell.color.g;
            return total;
        };
        require(green_total(strong_image) > green_total(no_image), "image strength makes persistent image visible");
        for (const auto palette : {Palette::green, Palette::mono, Palette::full}) {
            for (const double time : {0.0, 1.0, 5.0, 12.0}) {
                require(matrix_rain(frame, time, 7, palette, 0) == matrix_rain(black, time, 7, palette, 0),
                        "zero image strength is independent of source glyphs, colors, and fitted bounds");
                for (const auto cell : matrix_rain(frame, time, 7, palette, 0).cells) {
                    require(cell.glyph == ' ' || cell.color != Rgb{},
                            "zero strength emits no invisible non-space glyphs");
                }
            }
        }
        // Fixed-seed reproduction: column zero's first head exits row 24 at
        // 3.401295238 seconds. Its still-visible tail must not vanish then.
        const Frame single_column{1, 24, std::vector<Cell>(24)};
        require(matrix_rain(single_column, 3.401295238 - .000001, 1, Palette::green, 0) ==
                matrix_rain(single_column, 3.401295238 + .000001, 1, Palette::green, 0),
                "first trail remains continuous when its head leaves the screen");
        const auto matrix_mono = matrix_rain(mono, 0.7, 7, Palette::mono, .70);
        for (const auto cell : matrix_mono.cells) {
            require(cell.color.r == cell.color.g && cell.color.g == cell.color.b, "matrix monochrome equal channels");
        }
        rejects([&] { matrix_rain(frame, std::numeric_limits<double>::infinity(), 7, Palette::green, .70); },
                "nonfinite matrix time rejected");
        rejects([&] { matrix_rain(frame, 0, 7, Palette::green, 1.1); }, "out-of-range image strength rejected");

        Temp temp;
        const auto png_path = temp.path / "fixture.png";
        png_image png{};
        png.version = PNG_IMAGE_VERSION;
        png.width = 3; png.height = 1; png.format = PNG_FORMAT_RGBA;
        const unsigned char pixels[] = {255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 128};
        require(png_image_write_to_file(&png, png_path.c_str(), 0, pixels, 0, nullptr) != 0, "generate PNG fixture");
        png_image_free(&png);
        const auto decoded = load_image(png_path);
        require(decoded.width == 3 && decoded.height == 1, "decode PNG dimensions");
        require(decoded.pixels[0] == Rgb{255, 0, 0} && decoded.pixels[1] == Rgb{}, "PNG transparency composites to black");
        require(decoded.pixels[2] == Rgb{0, 0, 128}, "PNG partial alpha composites to black");
        const auto ppm_path = temp.path / "fixture.ppm";
        {
            std::ofstream ppm(ppm_path, std::ios::binary);
            ppm << "P6\n# generated fixture\n2 1\n255\n";
            const char bytes[] = {10, 32, 13, 0, 0, 0};
            ppm.write(bytes, 6);
        }
        require(load_image(ppm_path).pixels[0] == Rgb{10, 32, 13}, "PPM preserves whitespace-valued pixels");
        {
            std::ofstream ppm(temp.path / "cr.ppm", std::ios::binary);
            ppm << "P6\n1 1\n255\r";
            const char bytes[] = {10, 20, 30};
            ppm.write(bytes, 3);
        }
        require(load_image(temp.path / "cr.ppm").pixels[0] == Rgb{10, 20, 30}, "PPM single CR delimiter preserves LF-valued red sample");
        {
            std::ofstream broken(temp.path / "broken.ppm");
            broken << "P6\n2 2\n255\na";
        }
        rejects([&] { load_image(temp.path / "broken.ppm"); }, "truncated image rejected");
        {
            std::ofstream huge(temp.path / "huge.ppm");
            huge << "P6\n999999999 2\n255\n";
        }
        rejects([&] { load_image(temp.path / "huge.ppm"); }, "oversized input rejected");
        rejects([&] { load_image(temp.path / "missing.png"); }, "missing input rejected");
        std::cout << checks << " core checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "core test: " << error.what() << '\n';
        return 1;
    }
}
