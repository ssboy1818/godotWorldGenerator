#pragma once

#include "Vector2.h"

#include <cstdint>
#include <vector>

namespace worldgen {

using CellId = std::uint32_t;

struct Cell {
    // Cell IDs are the zero-based indices of the corresponding input sites.
    CellId id{0};
    Vector2d sitePosition;
    std::vector<Vector2d> vertices;
    std::vector<CellId> neighbors;
};

struct WorldDivision {
    std::vector<Cell> cells;
};

} // namespace worldgen
