#pragma once

#include "Id.h"

struct Polygon {
    PolygonId id {INVALID_ID};
    SiteId site{INVALID_ID};
    EdgeId edge{INVALID_ID};
};
