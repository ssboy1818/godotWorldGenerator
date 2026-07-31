#pragma once

#include "BoundingBox.h"
#include "DCEL.h"
#include "Region.h"

#include <utility>
#include <vector>

class World {
public:
    World(BoundingBox boundingBox, DCEL diagram, std::vector<Region> regions)
        : m_boundingBox(std::move(boundingBox)),
          m_diagram(std::move(diagram)),
          m_regions(std::move(regions)) {}

    [[nodiscard]] const BoundingBox &boundingBox() const noexcept {
        return m_boundingBox;
    }

    [[nodiscard]] const DCEL &diagram() const noexcept {
        return m_diagram;
    }

    [[nodiscard]] const std::vector<Region> &regions() const noexcept {
        return m_regions;
    }

private:
    BoundingBox m_boundingBox;
    DCEL m_diagram;
    std::vector<Region> m_regions;
};
