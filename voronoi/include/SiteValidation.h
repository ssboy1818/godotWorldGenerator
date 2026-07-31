#pragma once

#include "BoundingBox.h"
#include "NumericalPolicy.h"
#include "Site.h"
#include "WorldDivision.h"

#include <span>

namespace worldgen {

[[nodiscard]] NumericalTolerance validateSites(
    std::span<const Site> sites,
    const BoundingBox &boundingBox);

} // namespace worldgen
