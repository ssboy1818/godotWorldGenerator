#pragma once

#include "Id.h"

namespace worldgen {

struct Polygon {
    PolygonId id {INVALID_ID};
    SiteId site{INVALID_ID};
    EdgeId edge{INVALID_ID};
};

} // namespace worldgen
