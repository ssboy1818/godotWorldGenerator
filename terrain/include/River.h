#pragma once

#include "Vector2.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace worldgen {

struct RiverNode {
    Vector2d vertex;
    double strength{1.0};
};

using RiverId = std::uint32_t;
inline constexpr auto INVALID_RIVER_ID = std::numeric_limits<RiverId>::max();

struct River {
    std::vector<RiverNode> nodes;
    RiverId downstreamRiver{INVALID_RIVER_ID};
};

} // namespace worldgen
