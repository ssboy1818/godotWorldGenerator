#pragma once

#include "BoundingBox.h"
#include "Region.h"
#include "River.h"
#include "WorldDivision.h"

#include <utility>
#include <vector>

namespace worldgen {

class World {
public:
    World(BoundingBox boundingBox,
          WorldDivision division,
          std::vector<Region> regions,
          std::vector<River> rivers)
        : m_boundingBox(std::move(boundingBox)),
          m_division(std::move(division)),
          m_regions(std::move(regions)),
          m_rivers(std::move(rivers)) {}

    [[nodiscard]] const BoundingBox &boundingBox() const noexcept {
        return m_boundingBox;
    }

    [[nodiscard]] const WorldDivision &division() const noexcept {
        return m_division;
    }

    [[nodiscard]] const std::vector<Region> &regions() const noexcept {
        return m_regions;
    }

    [[nodiscard]] const std::vector<River> &rivers() const noexcept {
        return m_rivers;
    }

private:
    BoundingBox m_boundingBox;
    WorldDivision m_division;
    std::vector<Region> m_regions;
    std::vector<River> m_rivers;
};

} // namespace worldgen
