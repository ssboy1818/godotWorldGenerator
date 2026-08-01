#include "RiverGenerator.h"

#include "NumericalPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace worldgen {

namespace {

using RiverVertexId = std::size_t;
constexpr auto INVALID_RIVER_VERTEX = std::numeric_limits<RiverVertexId>::max();
constexpr double elevationTolerance = 1e-12;

constexpr std::uint64_t mix(std::uint64_t value) noexcept {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

struct RiverGraphVertex {
    Vector2d position;
    std::vector<RiverVertexId> neighbors;
    double elevationSum{0.0};
    std::size_t elevationSamples{0};
    RiverVertexId downstream{INVALID_RIVER_VERTEX};
    bool touchesWater{false};
    bool boundary{false};

    [[nodiscard]] double elevation() const noexcept {
        return elevationSum / static_cast<double>(elevationSamples);
    }
};

struct GridKey {
    std::int64_t x;
    std::int64_t y;

    bool operator==(const GridKey &) const noexcept = default;
};

struct GridKeyHash {
    [[nodiscard]] std::size_t operator()(GridKey key) const noexcept {
        return static_cast<std::size_t>(
            mix(static_cast<std::uint64_t>(key.x))
            ^ mix(static_cast<std::uint64_t>(key.y)));
    }
};

class RiverGraphBuilder {
public:
    RiverGraphBuilder(const BoundingBox &boundingBox, double tolerance)
        : m_boundingBox(boundingBox),
          m_tolerance(tolerance) {}

    [[nodiscard]] RiverVertexId addVertex(Vector2d position,
                                           const Region &region) {
        const auto key = gridKey(position);
        for (std::int64_t offsetY = -1; offsetY <= 1; ++offsetY) {
            for (std::int64_t offsetX = -1; offsetX <= 1; ++offsetX) {
                const auto bucket = m_buckets.find({key.x + offsetX,
                                                    key.y + offsetY});
                if (bucket == m_buckets.end())
                    continue;

                for (const auto vertex : bucket->second) {
                    if (!pointsAlmostEqual(m_vertices[vertex].position,
                                           position,
                                           m_tolerance)) {
                        continue;
                    }
                    addRegionSample(m_vertices[vertex], region);
                    return vertex;
                }
            }
        }

        const auto vertex = m_vertices.size();
        m_vertices.push_back({
            .position = position,
            .boundary = onBoundary(position),
        });
        addRegionSample(m_vertices.back(), region);
        m_buckets[key].push_back(vertex);
        return vertex;
    }

    void addEdge(RiverVertexId first, RiverVertexId second) {
        if (first == second)
            return;
        m_vertices[first].neighbors.push_back(second);
        m_vertices[second].neighbors.push_back(first);
    }

    [[nodiscard]] std::vector<RiverGraphVertex> finish() && {
        for (auto &vertex : m_vertices) {
            std::ranges::sort(vertex.neighbors);
            const auto duplicates = std::ranges::unique(vertex.neighbors);
            vertex.neighbors.erase(duplicates.begin(), duplicates.end());
        }
        return std::move(m_vertices);
    }

private:
    const BoundingBox &m_boundingBox;
    double m_tolerance;
    std::vector<RiverGraphVertex> m_vertices;
    std::unordered_map<GridKey, std::vector<RiverVertexId>, GridKeyHash> m_buckets;

    [[nodiscard]] GridKey gridKey(Vector2d position) const noexcept {
        return {
            static_cast<std::int64_t>(std::floor(
                (position.x - m_boundingBox.min.x) / m_tolerance)),
            static_cast<std::int64_t>(std::floor(
                (position.y - m_boundingBox.min.y) / m_tolerance)),
        };
    }

    [[nodiscard]] bool onBoundary(Vector2d position) const noexcept {
        return almostEqual(position.x, m_boundingBox.min.x, m_tolerance)
            || almostEqual(position.x, m_boundingBox.max.x, m_tolerance)
            || almostEqual(position.y, m_boundingBox.min.y, m_tolerance)
            || almostEqual(position.y, m_boundingBox.max.y, m_tolerance);
    }

    static void addRegionSample(RiverGraphVertex &vertex,
                                const Region &region) noexcept {
        vertex.elevationSum += region.elevation();
        ++vertex.elevationSamples;
        vertex.touchesWater = vertex.touchesWater || region.isWater();
    }
};

[[nodiscard]] std::vector<const Region *> indexRegions(
    std::span<const Region> regions,
    std::size_t cellCount) {
    if (regions.size() != cellCount)
        throw std::logic_error("River generation requires one region per cell.");

    std::vector<const Region *> indexed(cellCount, nullptr);
    for (const auto &region : regions) {
        const auto cell = static_cast<std::size_t>(region.cell());
        if (cell >= cellCount || indexed[cell] != nullptr)
            throw std::logic_error("River generation received invalid region cell IDs.");
        indexed[cell] = &region;
    }
    return indexed;
}

[[nodiscard]] std::vector<RiverGraphVertex> buildRiverGraph(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<const Region> regions) {
    const auto indexedRegions = indexRegions(regions, division.cells.size());
    const auto tolerance = numericalToleranceFor(boundingBox).sharedEdgeLength();
    RiverGraphBuilder builder{boundingBox, tolerance};

    for (const auto &cell : division.cells) {
        const auto cellIndex = static_cast<std::size_t>(cell.id);
        if (cellIndex >= indexedRegions.size() || indexedRegions[cellIndex] == nullptr)
            throw std::logic_error("River generation received an invalid cell ID.");
        if (cell.vertices.size() < 3)
            throw std::logic_error("River generation received a degenerate cell.");

        std::vector<RiverVertexId> vertices;
        vertices.reserve(cell.vertices.size());
        for (const auto position : cell.vertices)
            vertices.push_back(builder.addVertex(position, *indexedRegions[cellIndex]));

        for (std::size_t index = 0; index < vertices.size(); ++index)
            builder.addEdge(vertices[index], vertices[(index + 1) % vertices.size()]);
    }

    return std::move(builder).finish();
}

[[nodiscard]] double deterministicEdgeRandom(std::uint64_t seed,
                                             RiverVertexId from,
                                             RiverVertexId to) noexcept {
    const auto value = mix(seed
                           ^ mix(static_cast<std::uint64_t>(from)
                                 + 0x9e3779b97f4a7c15ULL)
                           ^ mix(static_cast<std::uint64_t>(to)
                                 + 0x243f6a8885a308d3ULL));
    return static_cast<double>(value >> 11) * 0x1.0p-53;
}

void assignDownstreamVertices(std::vector<RiverGraphVertex> &graph,
                              std::uint64_t seed,
                              double randomness) noexcept {
    for (RiverVertexId current = 0; current < graph.size(); ++current) {
        auto &vertex = graph[current];
        if (vertex.boundary || vertex.touchesWater)
            continue;

        auto maximumSlope = 0.0;
        for (const auto neighbor : vertex.neighbors) {
            const auto drop = vertex.elevation() - graph[neighbor].elevation();
            const auto length = (vertex.position - graph[neighbor].position).length();
            if (drop > elevationTolerance && length > 0.0)
                maximumSlope = std::max(maximumSlope, drop / length);
        }
        if (maximumSlope <= 0.0)
            continue;

        auto selectedScore = -std::numeric_limits<double>::infinity();
        for (const auto neighbor : vertex.neighbors) {
            const auto drop = vertex.elevation() - graph[neighbor].elevation();
            const auto length = (vertex.position - graph[neighbor].position).length();
            if (drop <= elevationTolerance || length <= 0.0)
                continue;

            const auto normalizedSlope = (drop / length) / maximumSlope;
            const auto score = normalizedSlope * (1.0 - randomness)
                               + deterministicEdgeRandom(seed, current, neighbor)
                                     * randomness;
            if (score > selectedScore
                || (score == selectedScore && neighbor < vertex.downstream)) {
                vertex.downstream = neighbor;
                selectedScore = score;
            }
        }
    }
}

[[nodiscard]] std::vector<RiverVertexId> selectSources(
    const std::vector<RiverGraphVertex> &graph,
    std::size_t sourceCount,
    double minimumElevation) {
    std::vector<RiverVertexId> candidates;
    for (RiverVertexId vertex = 0; vertex < graph.size(); ++vertex) {
        const auto &candidate = graph[vertex];
        if (candidate.boundary || candidate.touchesWater
            || candidate.elevationSamples < 2
            || candidate.downstream == INVALID_RIVER_VERTEX
            || candidate.elevation() < minimumElevation) {
            continue;
        }

        const auto localMaximum = std::ranges::none_of(
            candidate.neighbors,
            [&](RiverVertexId neighbor) {
                return graph[neighbor].elevation()
                       > candidate.elevation() + elevationTolerance;
            });
        if (localMaximum)
            candidates.push_back(vertex);
    }

    std::ranges::sort(candidates, [&](RiverVertexId left, RiverVertexId right) {
        const auto leftElevation = graph[left].elevation();
        const auto rightElevation = graph[right].elevation();
        if (leftElevation != rightElevation)
            return leftElevation > rightElevation;
        return left < right;
    });
    if (candidates.size() > sourceCount)
        candidates.resize(sourceCount);
    return candidates;
}

[[nodiscard]] std::vector<bool> collectActiveChannels(
    const std::vector<RiverGraphVertex> &graph,
    const std::vector<RiverVertexId> &sources) {
    std::vector<bool> active(graph.size(), false);
    for (const auto source : sources) {
        auto current = source;
        while (!active[current]) {
            active[current] = true;
            const auto downstream = graph[current].downstream;
            if (downstream == INVALID_RIVER_VERTEX)
                break;
            current = downstream;
        }
    }
    return active;
}

[[nodiscard]] std::vector<std::size_t> countActiveUpstreamVertices(
    const std::vector<RiverGraphVertex> &graph,
    const std::vector<bool> &active) {
    std::vector<std::size_t> upstreamCount(graph.size(), 0);
    for (RiverVertexId vertex = 0; vertex < graph.size(); ++vertex) {
        const auto downstream = graph[vertex].downstream;
        if (active[vertex]
            && downstream != INVALID_RIVER_VERTEX
            && active[downstream]) {
            ++upstreamCount[downstream];
        }
    }
    return upstreamCount;
}

[[nodiscard]] std::vector<double> accumulateFlow(
    const std::vector<RiverGraphVertex> &graph,
    const std::vector<RiverVertexId> &sources,
    const std::vector<bool> &active) {
    std::vector<double> flow(graph.size(), 0.0);
    for (const auto source : sources)
        flow[source] += 1.0;

    std::vector<RiverVertexId> ordered;
    ordered.reserve(graph.size());
    for (RiverVertexId vertex = 0; vertex < graph.size(); ++vertex) {
        if (active[vertex])
            ordered.push_back(vertex);
    }
    std::ranges::sort(ordered, [&](RiverVertexId left, RiverVertexId right) {
        const auto leftElevation = graph[left].elevation();
        const auto rightElevation = graph[right].elevation();
        if (leftElevation != rightElevation)
            return leftElevation > rightElevation;
        return left < right;
    });

    for (const auto vertex : ordered) {
        const auto downstream = graph[vertex].downstream;
        if (downstream != INVALID_RIVER_VERTEX && active[downstream])
            flow[downstream] += flow[vertex];
    }
    return flow;
}

struct RiverSegment {
    River river;
    RiverVertexId end;
};

[[nodiscard]] std::vector<River> buildRiverSegments(
    const std::vector<RiverGraphVertex> &graph,
    const std::vector<bool> &active,
    const std::vector<std::size_t> &upstreamCount,
    const std::vector<double> &flow) {
    std::vector<RiverVertexId> starts;
    for (RiverVertexId vertex = 0; vertex < graph.size(); ++vertex) {
        const auto downstream = graph[vertex].downstream;
        if (active[vertex]
            && upstreamCount[vertex] != 1
            && downstream != INVALID_RIVER_VERTEX
            && active[downstream]) {
            starts.push_back(vertex);
        }
    }
    std::ranges::sort(starts, [&](RiverVertexId left, RiverVertexId right) {
        const auto leftElevation = graph[left].elevation();
        const auto rightElevation = graph[right].elevation();
        if (leftElevation != rightElevation)
            return leftElevation > rightElevation;
        return left < right;
    });
    if (starts.size() > static_cast<std::size_t>(INVALID_RIVER_ID))
        throw std::length_error("The generated river network has too many segments.");

    std::vector<RiverId> segmentStartingAt(graph.size(), INVALID_RIVER_ID);
    std::vector<RiverSegment> segments;
    segments.reserve(starts.size());
    for (const auto start : starts) {
        const auto segmentId = static_cast<RiverId>(segments.size());
        segmentStartingAt[start] = segmentId;

        River river;
        auto current = start;
        river.nodes.push_back({graph[current].position, flow[current]});
        do {
            current = graph[current].downstream;
            river.nodes.push_back({graph[current].position, flow[current]});
        } while (upstreamCount[current] == 1
                 && graph[current].downstream != INVALID_RIVER_VERTEX
                 && active[graph[current].downstream]);

        segments.push_back({std::move(river), current});
    }

    for (auto &segment : segments) {
        const auto downstream = segmentStartingAt[segment.end];
        if (downstream != INVALID_RIVER_ID)
            segment.river.downstreamRiver = downstream;
    }

    std::vector<River> rivers;
    rivers.reserve(segments.size());
    for (auto &segment : segments)
        rivers.push_back(std::move(segment.river));
    return rivers;
}

} // namespace

std::vector<River> generateRivers(const BoundingBox &boundingBox,
                                  const WorldDivision &division,
                                  std::span<const Region> regions,
                                  std::uint64_t seed,
                                  std::size_t riverSourceCount,
                                  double minimumSourceElevation,
                                  double randomness) {
    if (riverSourceCount == 0 || division.cells.empty())
        return {};

    auto graph = buildRiverGraph(boundingBox, division, regions);
    assignDownstreamVertices(graph, seed ^ 0x726976657273ULL, randomness);
    const auto sources = selectSources(graph,
                                       riverSourceCount,
                                       minimumSourceElevation);
    if (sources.empty())
        return {};

    const auto active = collectActiveChannels(graph, sources);
    const auto upstreamCount = countActiveUpstreamVertices(graph, active);
    const auto flow = accumulateFlow(graph, sources, active);
    return buildRiverSegments(graph, active, upstreamCount, flow);
}

} // namespace worldgen
