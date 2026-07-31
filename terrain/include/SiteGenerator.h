#pragma once

#include "BoundingBox.h"
#include "Site.h"

#include <vector>

class SiteGenerator {
public:
    virtual ~SiteGenerator() noexcept = default;

    [[nodiscard]] virtual std::vector<Site> generateSites(
        const BoundingBox &boundingBox) const = 0;
};
