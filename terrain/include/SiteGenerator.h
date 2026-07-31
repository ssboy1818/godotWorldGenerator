#pragma once

#include "BoundingBox.h"
#include "Site.h"

#include <vector>

namespace worldgen {

class SiteGenerator {
public:
    virtual ~SiteGenerator() noexcept = default;

    [[nodiscard]] virtual std::vector<Site> generateSites(
        const BoundingBox &boundingBox) const = 0;
};

} // namespace worldgen
