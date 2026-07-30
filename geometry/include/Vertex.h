#pragma once

#include "Id.h"
#include "Vector2.h"

struct Vertex {
    VertexId id{INVALID_ID};
    Vector2d position;
    EdgeId edge{INVALID_ID};
};
