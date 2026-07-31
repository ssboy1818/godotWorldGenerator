#pragma once

#include "Id.h"

#include <vector>

struct Polygon {
    PolygonId id {INVALID_ID};
    SiteId site{INVALID_ID};
    EdgeId edge{INVALID_ID};
    std::vector<VertexId> vertices;
};
