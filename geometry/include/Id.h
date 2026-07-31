#pragma once

#include <cstdint>

namespace worldgen {

using SiteId = std::int64_t;
using VertexId = std::int64_t;
using EdgeId = std::int64_t;
using PolygonId = std::int64_t;
inline constexpr std::int64_t INVALID_ID = -1;
inline constexpr double EPS = 1e-9;

} // namespace worldgen
