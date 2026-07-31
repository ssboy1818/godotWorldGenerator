#pragma once

#include "Id.h"

namespace worldgen {

class Edge {
public:
    EdgeId id{INVALID_ID};
    VertexId origin{INVALID_ID};
    EdgeId twin{INVALID_ID};
    EdgeId prev{INVALID_ID};
    EdgeId next{INVALID_ID};
    PolygonId face{INVALID_ID};

public:
    explicit Edge(VertexId origin_, PolygonId face_ = INVALID_ID) noexcept;
};

// Implementation

inline Edge::Edge(VertexId origin_, PolygonId face_) noexcept
    : origin(origin_), face(face_) {}

} // namespace worldgen
