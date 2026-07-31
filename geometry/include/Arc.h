#pragma once

#include "Id.h"

namespace worldgen {

class CircleEvent;

class Arc {
public:
    SiteId focus{INVALID_ID};
    CircleEvent *pendingEvent{nullptr};
    EdgeId rightEdge{INVALID_ID};

public:
    explicit Arc(SiteId site) noexcept : focus(site) {}
};

} // namespace worldgen
