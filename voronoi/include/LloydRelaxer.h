#pragma once

#include "BoundingBox.h"
#include "Site.h"
#include "WorldDivision.h"

#include <cstddef>
#include <span>

namespace worldgen {

class LloydRelaxer {
public:
    // Generates the bounded diagram and moves every site to its cell centroid
    // after each relaxation iteration. Cell IDs retain the input site order.
    [[nodiscard]] WorldDivision relax(
        std::span<const Site> sites,
        const BoundingBox &boundingBox,
        std::size_t iterations) const;
};

} // namespace worldgen
