#pragma once

#include "Vector2.h"
#include "Id.h"

class Edge {
public:
    Vector2d origin;
    EdgeId twin{INVALID_ID};
    EdgeId prev{INVALID_ID};
    EdgeId next{INVALID_ID};
    PolygonId face{INVALID_ID};

public:
    Edge(Vector2d origin_) noexcept;
};

// Implementation

inline Edge::Edge(Vector2d origin_) noexcept
    : origin(origin_) {}
