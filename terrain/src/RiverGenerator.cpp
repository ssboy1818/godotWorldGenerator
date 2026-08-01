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

struct RiverGraphVertex {
    Vector2d position;
    std::vector<RiverVertexId> neighbors;
    double elevationSum{0.0};
    std::size_t elevationSamples{0};
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
        auto x = static_cast<std::uint64_t>(key.x);
        auto y = static_cast<std::uint64_t>(key.y);
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        y ^= y >> 27;
        y *= 0x94d049bb133111ebULL;
        return static_cast<std::size_t>(x ^ y);
    }
};

class DeterministicRandom {
public:
    explicit DeterministicRandom(std::uint64_t seed) noexcept
        : m_state(seed) {}

    [[nodiscard]] double unit() noexcept {
        auto value = (m_state += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        value ^= value >> 31;
        return static_cast<double>(value >> 11) * 0x1.0p-53;
    }

private:
    std::uint64_t m_state;
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

[[nodiscard]] std::vector<RiverVertexId> sourceCandidates(
    const std::vector<RiverGraphVertex> &graph,
    double minimumElevation) {
    constexpr double elevationTolerance = 1e-12;
    std::vector<RiverVertexId> candidates;

    for (RiverVertexId vertex = 0; vertex < graph.size(); ++vertex) {
        const auto &candidate = graph[vertex];
        if (candidate.boundary || candidate.touchesWater
            || candidate.elevationSamples < 2
            || candidate.elevation() < minimumElevation) {
            continue;
        }

        auto localMaximum = true;
        auto hasDownhillNeighbor = false;
        for (const auto neighbor : candidate.neighbors) {
            const auto neighborElevation = graph[neighbor].elevation();
            if (neighborElevation > candidate.elevation() + elevationTolerance)
                localMaximum = false;
            if (neighborElevation < candidate.elevation() - elevationTolerance)
                hasDownhillNeighbor = true;
        }
        if (localMaximum && hasDownhillNeighbor)
            candidates.push_back(vertex);
    }

    std::ranges::sort(candidates, [&](RiverVertexId left, RiverVertexId right) {
        const auto leftElevation = graph[left].elevation();
        const auto rightElevation = graph[right].elevation();
        if (leftElevation != rightElevation)
            return leftElevation > rightElevation;
        return left < right;
    });
    return candidates;
}

[[nodiscard]] RiverVertexId chooseDownhillNeighbor(
    RiverVertexId current,
    const std::vector<RiverGraphVertex> &graph,
    const std::vector<bool> &visited,
    double randomness,
    DeterministicRandom &random) noexcept {
    constexpr double elevationTolerance = 1e-12;
    const auto currentElevation = graph[current].elevation();
    auto selected = std::numeric_limits<RiverVertexId>::max();
    auto selectedScore = -std::numeric_limits<double>::infinity();
    auto maximumDrop = 0.0;

    for (const auto neighbor : graph[current].neighbors) {
        if (visited[neighbor])
            continue;
        maximumDrop = std::max(maximumDrop,
                               currentElevation - graph[neighbor].elevation());
    }
    if (maximumDrop <= elevationTolerance)
        return selected;

    for (const auto neighbor : graph[current].neighbors) {
        if (visited[neighbor])
            continue;
        const auto drop = currentElevation - graph[neighbor].elevation();
        if (drop <= elevationTolerance)
            continue;

        const auto descentScore = drop / maximumDrop;
        const auto score = descentScore * (1.0 - randomness)
                           + random.unit() * randomness;
        if (score > selectedScore
            || (score == selectedScore && neighbor < selected)) {
            selected = neighbor;
            selectedScore = score;
        }
    }
    return selected;
}

[[nodiscard]] River traceRiver(RiverVertexId source,
                               const std::vector<RiverGraphVertex> &graph,
                               double randomness,
                               DeterministicRandom &random) {
    std::vector<bool> visited(graph.size(), false);
    River river;
    auto current = source;

    while (!visited[current]) {
        visited[current] = true;
        river.push_back({
            graph[current].position,
            static_cast<double>(river.size() + 1),
        });

        if (current != source
            && (graph[current].boundary || graph[current].touchesWater)) {
            break;
        }

        const auto next = chooseDownhillNeighbor(current,
                                                 graph,
                                                 visited,
                                                 randomness,
                                                 random);
        if (next == std::numeric_limits<RiverVertexId>::max())
            break;
        current = next;
    }

    if (river.size() < 2)
        river.clear();
    return river;
}

} // namespace

std::vector<River> generateRivers(const BoundingBox &boundingBox,
                                  const WorldDivision &division,
                                  std::span<const Region> regions,
                                  std::uint64_t seed,
                                  std::size_t riverCount,
                                  double minimumSourceElevation,
                                  double randomness) {
    if (riverCount == 0 || division.cells.empty())
        return {};

    auto graph = buildRiverGraph(boundingBox, division, regions);
    auto candidates = sourceCandidates(graph, minimumSourceElevation);
    DeterministicRandom random{seed ^ 0x726976657273ULL};
    std::vector<River> rivers;
    rivers.reserve(std::min(riverCount, candidates.size()));

    for (const auto source : candidates) {
        if (rivers.size() == riverCount)
            break;

        auto river = traceRiver(source, graph, randomness, random);
        if (river.empty())
            continue;

        rivers.push_back(std::move(river));
    }

    return rivers;
}

} // namespace worldgen
