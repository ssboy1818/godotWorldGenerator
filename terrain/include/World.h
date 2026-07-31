#pragma once

#include "BoundingBox.h"
#include "Region.h"
#include "WorldDivision.h"

#include <utility>
#include <vector>

namespace worldgen {

class World {
public:
    World(BoundingBox boundingBox,
          WorldDivision division,
          std::vector<Region> regions)
        : m_boundingBox(std::move(boundingBox)),
          m_division(std::move(division)),
          m_regions(std::move(regions)) {}

    [[nodiscard]] const BoundingBox &boundingBox() const noexcept {
        return m_boundingBox;
    }

    [[nodiscard]] const WorldDivision &division() const noexcept {
        return m_division;
    }

    [[nodiscard]] const std::vector<Region> &regions() const noexcept {
        return m_regions;
    }

private:
    BoundingBox m_boundingBox;
    WorldDivision m_division;
    std::vector<Region> m_regions;
};

} // namespace worldgen
