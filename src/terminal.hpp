#pragma once
#include "bmatrix/core.hpp"
#include <string>

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
int run_terminal(const Image& image, const SessionOptions& options);
} // namespace bmatrix
