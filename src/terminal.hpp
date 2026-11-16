#pragma once
#include "bmatrix/core.hpp"
#include <string>
#include <vector>

namespace bmatrix {
enum class Effect { matrix, flow, still };
struct SessionOptions {
    RenderOptions render;
    std::string color{"auto"};
    bool fixed_size{false};
    Effect effect{Effect::matrix};
    double fps{30};
    double duration{3};
    unsigned long frames{0};
};
// An empty path selects the built-in demo. Only the current image is decoded.
int run_terminal(const std::vector<std::filesystem::path>& paths, const SessionOptions& options);
} // namespace bmatrix
