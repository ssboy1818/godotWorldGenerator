#include "RiverGenerator.h"

#include "NumericalPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace worldgen {

namespace {

using RiverVertexId = std::size_t;
constexpr auto INVALID_RIVER_VERTEX = std::numeric_limits<RiverVertexId>::max();
constexpr double comparisonTolerance = 1e-12;

constexpr std::uint64_t mix(std::uint64_t value) noexcept {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

[[nodiscard]] double deterministicRandom(std::uint64_t seed,
                                         std::uint64_t first,
                                         std::uint64_t second) noexcept {
    const auto value = mix(seed
                           ^ mix(first + 0x9e3779b97f4a7c15ULL)
                           ^ mix(second + 0x243f6a8885a308d3ULL));
    return static_cast<double>(value >> 11) * 0x1.0p-53;
}

struct RiverGraphVertex {
    Vector2d position;
    std::vector<RiverVertexId> neighbors;
    double elevationSum{0.0};
    std::size_t elevationSamples{0};
    double distanceToWater{std::numeric_limits<double>::infinity()};
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

struct RiverGraphEdge {
    RiverVertexId first;
    RiverVertexId second;

    bool operator==(const RiverGraphEdge &) const noexcept = default;
};

struct RiverGraphEdgeHash {
    [[nodiscard]] std::size_t operator()(RiverGraphEdge edge) const noexcept {
        return static_cast<std::size_t>(
            mix(static_cast<std::uint64_t>(edge.first))
            ^ mix(static_cast<std::uint64_t>(edge.second)));
    }
};

struct RegionEdgeReference {
    CellId cell;
    std::size_t edge;
};

struct RiverGraph {
    std::vector<RiverGraphVertex> vertices;
    std::vector<std::vector<RiverVertexId>> cellVertices;
    std::unordered_map<
        RiverGraphEdge,
        std::vector<RegionEdgeReference>,
        RiverGraphEdgeHash> regionEdges;
};

[[nodiscard]] RiverGraphEdge graphEdge(RiverVertexId first,
                                       RiverVertexId second) noexcept {
    if (first > second)
        std::swap(first, second);
    return {first, second};
}

class RiverGraphBuilder {
public:
    RiverGraphBuilder(const BoundingBox &boundingBox,
                      double tolerance,
                      std::size_t cellCount)
        : m_boundingBox(boundingBox),
          m_tolerance(tolerance) {
        m_graph.cellVertices.resize(cellCount);
    }

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
                    if (!pointsAlmostEqual(m_graph.vertices[vertex].position,
                                           position,
                                           m_tolerance)) {
                        continue;
                    }
                    addRegionSample(m_graph.vertices[vertex], region);
                    return vertex;
                }
            }
        }

        const auto vertex = m_graph.vertices.size();
        m_graph.vertices.push_back({
            .position = position,
            .boundary = onBoundary(position),
        });
        addRegionSample(m_graph.vertices.back(), region);
        m_buckets[key].push_back(vertex);
        return vertex;
    }

    void addCell(CellId cell, std::vector<RiverVertexId> vertices) {
        const auto cellIndex = static_cast<std::size_t>(cell);
        if (cellIndex >= m_graph.cellVertices.size()
            || !m_graph.cellVertices[cellIndex].empty()) {
            throw std::logic_error("River generation received duplicate cell IDs.");
        }
        m_graph.cellVertices[cellIndex] = std::move(vertices);
    }

    void addEdge(RiverVertexId first,
                 RiverVertexId second,
                 CellId cell,
                 std::size_t edge) {
        if (first == second)
            return;
        m_graph.vertices[first].neighbors.push_back(second);
        m_graph.vertices[second].neighbors.push_back(first);
        m_graph.regionEdges[graphEdge(first, second)].push_back({cell, edge});
    }

    [[nodiscard]] RiverGraph finish() && {
        for (auto &vertex : m_graph.vertices) {
            std::ranges::sort(vertex.neighbors);
            const auto duplicates = std::ranges::unique(vertex.neighbors);
            vertex.neighbors.erase(duplicates.begin(), duplicates.end());
        }
        return std::move(m_graph);
    }

private:
    const BoundingBox &m_boundingBox;
    double m_tolerance;
    RiverGraph m_graph;
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

[[nodiscard]] std::vector<Region *> indexRegions(std::span<Region> regions,
                                                  std::size_t cellCount) {
    if (regions.size() != cellCount)
        throw std::logic_error("River generation requires one region per cell.");

    std::vector<Region *> indexed(cellCount, nullptr);
    for (auto &region : regions) {
        const auto cell = static_cast<std::size_t>(region.cell());
        if (cell >= cellCount || indexed[cell] != nullptr)
            throw std::logic_error("River generation received invalid region cell IDs.");
        indexed[cell] = &region;
    }
    return indexed;
}

[[nodiscard]] RiverGraph buildRiverGraph(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    const std::vector<Region *> &regions) {
    const auto tolerance = numericalToleranceFor(boundingBox).sharedEdgeLength();
    RiverGraphBuilder builder{boundingBox, tolerance, division.cells.size()};

    for (const auto &cell : division.cells) {
        const auto cellIndex = static_cast<std::size_t>(cell.id);
        if (cellIndex >= regions.size() || regions[cellIndex] == nullptr)
            throw std::logic_error("River generation received an invalid cell ID.");
        if (cell.vertices.size() < 3)
            throw std::logic_error("River generation received a degenerate cell.");

        std::vector<RiverVertexId> vertices;
        vertices.reserve(cell.vertices.size());
        for (const auto position : cell.vertices)
            vertices.push_back(builder.addVertex(position, *regions[cellIndex]));

        for (std::size_t edge = 0; edge < vertices.size(); ++edge) {
            builder.addEdge(vertices[edge],
                            vertices[(edge + 1) % vertices.size()],
                            cell.id,
                            edge);
        }
        builder.addCell(cell.id, std::move(vertices));
    }

    return std::move(builder).finish();
}

[[nodiscard]] double routingStepCost(const RiverGraphVertex &from,
                                     const RiverGraphVertex &to,
                                     double elevationTolerance,
                                     double randomness,
                                     std::uint64_t seed,
                                     RiverVertexId fromId,
                                     RiverVertexId toId) noexcept {
    const auto length = (to.position - from.position).length();
    const auto rise = std::max(0.0, to.elevation() - from.elevation());
    const auto drop = std::max(0.0, from.elevation() - to.elevation());
    const auto elevationScale = std::max(elevationTolerance, comparisonTolerance);
    const auto risePenalty = 1.0 + 4.0 * rise / elevationScale;
    const auto descentRatio = std::min(drop / elevationScale, 1.0);
    const auto descentFactor = 1.0 / (1.0 + 0.25 * descentRatio);
    const auto randomValue = deterministicRandom(seed, fromId, toId);
    const auto randomFactor = 1.0 + randomness * (randomValue - 0.5) * 0.5;
    return length * risePenalty * descentFactor * randomFactor;
}

void routeVerticesToWater(RiverGraph &graph,
                          std::uint64_t seed,
                          double randomness,
                          double elevationTolerance) {
    using QueueEntry = std::pair<double, RiverVertexId>;
    std::priority_queue<
        QueueEntry,
        std::vector<QueueEntry>,
        std::greater<>> queue;

    for (RiverVertexId vertex = 0; vertex < graph.vertices.size(); ++vertex) {
        if (!graph.vertices[vertex].touchesWater)
            continue;
        graph.vertices[vertex].distanceToWater = 0.0;
        queue.emplace(0.0, vertex);
    }

    while (!queue.empty()) {
        const auto [distance, next] = queue.top();
        queue.pop();
        if (distance > graph.vertices[next].distanceToWater)
            continue;

        for (const auto previous : graph.vertices[next].neighbors) {
            auto &from = graph.vertices[previous];
            if (from.touchesWater)
                continue;
            if (graph.vertices[next].elevation()
                > from.elevation() + elevationTolerance + comparisonTolerance) {
                continue;
            }

            const auto step = routingStepCost(from,
                                              graph.vertices[next],
                                              elevationTolerance,
                                              randomness,
                                              seed,
                                              previous,
                                              next);
            const auto candidateDistance = distance + step;
            if (candidateDistance < from.distanceToWater
                || (candidateDistance == from.distanceToWater
                    && next < from.downstream)) {
                from.distanceToWater = candidateDistance;
                from.downstream = next;
                queue.emplace(candidateDistance, previous);
            }
        }
    }
}

struct SelectedSources {
    std::vector<RiverVertexId> vertices;
    std::vector<bool> activeChannels;
};

[[nodiscard]] SelectedSources selectSources(
    const RiverGraph &graph,
    const std::vector<Region *> &regions,
    std::span<const CellId> candidateCells,
    std::size_t sourceCount,
    std::uint64_t seed) {
    std::vector<CellId> shuffledCandidates{candidateCells.begin(),
                                           candidateCells.end()};
    std::ranges::sort(shuffledCandidates, [&](CellId left, CellId right) {
        const auto leftOrder = deterministicRandom(seed,
                                                   left,
                                                   0x736f75726365ULL);
        const auto rightOrder = deterministicRandom(seed,
                                                    right,
                                                    0x736f75726365ULL);
        if (leftOrder != rightOrder)
            return leftOrder < rightOrder;
        return left < right;
    });

    SelectedSources selected{
        .activeChannels = std::vector<bool>(graph.vertices.size(), false),
    };
    std::vector<bool> sourceVertices(graph.vertices.size(), false);
    selected.vertices.reserve(std::min(sourceCount, shuffledCandidates.size()));

    for (const auto candidateCell : shuffledCandidates) {
        if (selected.vertices.size() == sourceCount)
            break;

        const auto cell = static_cast<std::size_t>(candidateCell);
        if (cell >= graph.cellVertices.size()
            || cell >= regions.size()
            || regions[cell] == nullptr
            || !regions[cell]->isLand()) {
            throw std::logic_error("River generation received an invalid source candidate.");
        }

        auto source = INVALID_RIVER_VERTEX;
        for (const auto vertex : graph.cellVertices[cell]) {
            const auto &candidate = graph.vertices[vertex];
            if (candidate.boundary || candidate.touchesWater
                || candidate.downstream == INVALID_RIVER_VERTEX
                || selected.activeChannels[vertex]) {
                continue;
            }
            if (source == INVALID_RIVER_VERTEX
                || candidate.elevation() > graph.vertices[source].elevation()
                || (candidate.elevation() == graph.vertices[source].elevation()
                    && vertex < source)) {
                source = vertex;
            }
        }
        if (source == INVALID_RIVER_VERTEX)
            continue;

        auto current = source;
        auto reachesWater = false;
        auto entersExistingSource = false;
        while (true) {
            if (sourceVertices[current]) {
                entersExistingSource = true;
                break;
            }
            if (selected.activeChannels[current]) {
                reachesWater = true;
                break;
            }
            if (graph.vertices[current].touchesWater) {
                reachesWater = true;
                break;
            }
            const auto downstream = graph.vertices[current].downstream;
            if (downstream == INVALID_RIVER_VERTEX)
                break;
            current = downstream;
        }
        if (!reachesWater || entersExistingSource)
            continue;

        selected.vertices.push_back(source);
        sourceVertices[source] = true;
        current = source;
        while (!selected.activeChannels[current]) {
            selected.activeChannels[current] = true;
            if (graph.vertices[current].touchesWater)
                break;
            current = graph.vertices[current].downstream;
        }
    }

    return selected;
}

[[nodiscard]] std::vector<std::size_t> countActiveUpstreamVertices(
    const RiverGraph &graph,
    const std::vector<bool> &active) {
    std::vector<std::size_t> upstreamCount(graph.vertices.size(), 0);
    for (RiverVertexId vertex = 0; vertex < graph.vertices.size(); ++vertex) {
        const auto downstream = graph.vertices[vertex].downstream;
        if (active[vertex]
            && downstream != INVALID_RIVER_VERTEX
            && active[downstream]) {
            ++upstreamCount[downstream];
        }
    }
    return upstreamCount;
}

[[nodiscard]] std::vector<double> accumulateFlow(
    const RiverGraph &graph,
    const std::vector<RiverVertexId> &sources,
    const std::vector<bool> &active) {
    std::vector<double> flow(graph.vertices.size(), 0.0);
    for (const auto source : sources)
        flow[source] += 1.0;

    std::vector<RiverVertexId> ordered;
    ordered.reserve(graph.vertices.size());
    for (RiverVertexId vertex = 0; vertex < graph.vertices.size(); ++vertex) {
        if (active[vertex])
            ordered.push_back(vertex);
    }
    std::ranges::sort(ordered, [&](RiverVertexId left, RiverVertexId right) {
        const auto leftDistance = graph.vertices[left].distanceToWater;
        const auto rightDistance = graph.vertices[right].distanceToWater;
        if (leftDistance != rightDistance)
            return leftDistance > rightDistance;
        return left < right;
    });

    for (const auto vertex : ordered) {
        const auto downstream = graph.vertices[vertex].downstream;
        if (downstream != INVALID_RIVER_VERTEX && active[downstream])
            flow[downstream] += flow[vertex];
    }
    return flow;
}

struct RiverSegment {
    River river;
    std::vector<RiverVertexId> graphVertices;
};

[[nodiscard]] std::vector<RiverSegment> buildRiverSegments(
    const RiverGraph &graph,
    const std::vector<bool> &active,
    const std::vector<std::size_t> &upstreamCount,
    const std::vector<double> &flow) {
    std::vector<RiverVertexId> starts;
    for (RiverVertexId vertex = 0; vertex < graph.vertices.size(); ++vertex) {
        const auto downstream = graph.vertices[vertex].downstream;
        if (active[vertex]
            && upstreamCount[vertex] != 1
            && downstream != INVALID_RIVER_VERTEX
            && active[downstream]) {
            starts.push_back(vertex);
        }
    }
    std::ranges::sort(starts, [&](RiverVertexId left, RiverVertexId right) {
        const auto leftDistance = graph.vertices[left].distanceToWater;
        const auto rightDistance = graph.vertices[right].distanceToWater;
        if (leftDistance != rightDistance)
            return leftDistance > rightDistance;
        return left < right;
    });
    if (starts.size() > static_cast<std::size_t>(INVALID_RIVER_ID))
        throw std::length_error("The generated river network has too many segments.");

    std::vector<RiverId> segmentStartingAt(graph.vertices.size(), INVALID_RIVER_ID);
    std::vector<RiverSegment> segments;
    segments.reserve(starts.size());
    for (const auto start : starts) {
        const auto segmentId = static_cast<RiverId>(segments.size());
        segmentStartingAt[start] = segmentId;

        RiverSegment segment;
        auto current = start;
        segment.river.nodes.push_back({graph.vertices[current].position,
                                       flow[current]});
        segment.graphVertices.push_back(current);
        do {
            current = graph.vertices[current].downstream;
            segment.river.nodes.push_back({graph.vertices[current].position,
                                           flow[current]});
            segment.graphVertices.push_back(current);
        } while (upstreamCount[current] == 1
                 && graph.vertices[current].downstream != INVALID_RIVER_VERTEX
                 && active[graph.vertices[current].downstream]);

        segments.push_back(std::move(segment));
    }

    for (auto &segment : segments) {
        const auto downstream = segmentStartingAt[segment.graphVertices.back()];
        if (downstream != INVALID_RIVER_ID)
            segment.river.downstreamRiver = downstream;
    }
    return segments;
}

void assignRiversToRegionEdges(
    const RiverGraph &graph,
    const std::vector<RiverSegment> &segments,
    const std::vector<Region *> &regions) {
    for (std::size_t riverIndex = 0; riverIndex < segments.size(); ++riverIndex) {
        const auto &vertices = segments[riverIndex].graphVertices;
        for (std::size_t node = 1; node < vertices.size(); ++node) {
            const auto references = graph.regionEdges.find(
                graphEdge(vertices[node - 1], vertices[node]));
            if (references == graph.regionEdges.end()) {
                throw std::logic_error(
                    "A river graph edge has no corresponding region edge.");
            }

            for (const auto reference : references->second) {
                const auto cell = static_cast<std::size_t>(reference.cell);
                if (cell >= regions.size() || regions[cell] == nullptr)
                    throw std::logic_error("A river edge references an invalid region.");
                regions[cell]->setRiverAtEdge(
                    reference.edge,
                    static_cast<RiverId>(riverIndex));
            }
        }
    }
}

} // namespace

std::vector<River> generateRivers(const BoundingBox &boundingBox,
                                  const WorldDivision &division,
                                  std::span<Region> regions,
                                  std::span<const CellId> candidateCells,
                                  std::uint64_t seed,
                                  std::size_t riverSourceCount,
                                  double randomness,
                                  double elevationTolerance) {
    if (riverSourceCount == 0
        || division.cells.empty()
        || candidateCells.empty()) {
        return {};
    }

    const auto indexedRegions = indexRegions(regions, division.cells.size());
    auto graph = buildRiverGraph(boundingBox, division, indexedRegions);
    routeVerticesToWater(graph,
                         seed ^ 0x726976657273ULL,
                         randomness,
                         elevationTolerance);
    const auto selected = selectSources(graph,
                                        indexedRegions,
                                        candidateCells,
                                        riverSourceCount,
                                        seed ^ 0x736f7572636573ULL);
    if (selected.vertices.empty())
        return {};

    const auto upstreamCount = countActiveUpstreamVertices(
        graph,
        selected.activeChannels);
    const auto flow = accumulateFlow(graph,
                                     selected.vertices,
                                     selected.activeChannels);
    auto segments = buildRiverSegments(graph,
                                       selected.activeChannels,
                                       upstreamCount,
                                       flow);
    assignRiversToRegionEdges(graph, segments, indexedRegions);

    std::vector<River> rivers;
    rivers.reserve(segments.size());
    for (auto &segment : segments)
        rivers.push_back(std::move(segment.river));
    return rivers;
}

} // namespace worldgen
