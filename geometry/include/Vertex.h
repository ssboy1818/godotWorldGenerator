#pragma once

#include "Id.h"
#include "Vector2.h"

namespace worldgen {

struct Vertex {
    VertexId id{INVALID_ID};
    Vector2d position;
    EdgeId edge{INVALID_ID};
};

} // namespace worldgen
