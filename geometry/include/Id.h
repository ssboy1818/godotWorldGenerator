#pragma once

#include <sys/types.h>

using SiteId = ssize_t;
using VertexId = ssize_t;
using EdgeId = ssize_t;
using PolygonId = ssize_t;
inline constexpr ssize_t INVALID_ID = -1;
inline constexpr double EPS = 1e-9;