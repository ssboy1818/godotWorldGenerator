#pragma once

#include "BoundingBox.h"
#include "Region.h"
#include "River.h"
#include "WorldDivision.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace worldgen {

[[nodiscard]] std::vector<River> generateRivers(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<const Region> regions,
    std::uint64_t seed,
    std::size_t riverSourceCount,
    double minimumSourceElevation,
    double randomness);

} // namespace worldgen
