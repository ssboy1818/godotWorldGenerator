#pragma once

#include "Id.h"

class CircleEvent;

class Arc {
public:
    SiteId focus{INVALID_ID};
    CircleEvent *pendingEvent{nullptr};
    EdgeId rightEdge{INVALID_ID};

public:
    explicit Arc(SiteId site) noexcept : focus(site) {}
};
