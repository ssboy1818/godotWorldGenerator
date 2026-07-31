#pragma once

#include "Id.h"
#include "Vector2.h"

class Arc {
public:
    SiteId focus{INVALID_ID};
    Vector2d topLimit;

public:
    explicit Arc(SiteId site) noexcept : focus(site) {};

    bool operator<(const Arc &other) const noexcept {
        return topLimit < other.topLimit;
    };
};
