#pragma once

#include "BoundingBox.h"
#include "Site.h"
#include "WorldDivision.h"

#include <span>

namespace worldgen {

class Fortune {
public:
    Fortune() noexcept = default;

    // Sites at or below max(1e-10 * maximum box dimension,
    // 64 * machine epsilon * coordinate scale) are rejected before the sweep.
    // Cell IDs in the result correspond to indices in `sites`.
    [[nodiscard]] WorldDivision generate(
        std::span<const Site> sites,
        const BoundingBox &boundingBox) const;
};

} // namespace worldgen
