#pragma once

#include "Vector2.h"

#include <stdexcept>

class BoundingBox {
public:
    Vector2d min;
    Vector2d max;

public:
    BoundingBox(Vector2d min_, Vector2d max_)
        : min(min_), max(max_) {
        if (min.x >= max.x || min.y >= max.y)
            throw std::invalid_argument("A bounding box must have positive width and height.");
    }

    [[nodiscard]] bool contains(Vector2d point) const noexcept {
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y;
    }
};
