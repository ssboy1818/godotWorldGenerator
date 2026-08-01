#pragma once

#include "Vector2.h"

#include <vector>

namespace worldgen {

struct RiverNode {
    Vector2d vertex;
    double strength{1.0};
};

using River = std::vector<RiverNode>;

} // namespace worldgen
