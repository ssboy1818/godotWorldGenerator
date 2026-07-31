#pragma once

#include "Id.h"
#include "Vector2.h"

class Arc {
public:
    SiteId focus{INVALID_ID};
    Vector2d topLimit;

public:
    Arc() noexcept = default;

    bool operator<(const Arc &other) const noexcept {
        if ((topLimit.x < other.topLimit.x) ||
            (topLimit.x == other.topLimit.x && topLimit.y < other.topLimit.y))
            return true;
        return false;
    };
};
