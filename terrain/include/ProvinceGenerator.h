#pragma once

#include "BoundingBox.h"
#include "Province.h"
#include "WorldDivision.h"

#include <cstddef>
#include <span>
#include <vector>

namespace worldgen {

[[nodiscard]] std::vector<Province> generateProvinces(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<Region> regions,
    double startScore,
    double riverContribution,
    double elevationContribution,
    double distanceContribution,
    double baseCost,
    std::size_t minimumRegionCount = 3);

} // namespace worldgen
