#include "PerlinNoise.h"
#include "WorldGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using namespace worldgen;

constexpr double tolerance = 1e-12;

void require(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error{message.data()};
}

void requireNear(double actual, double expected, std::string_view message) {
    require(std::abs(actual - expected) <= tolerance, message);
}

bool pointsNear(Vector2d first, Vector2d second, double coordinateTolerance) {
    return (first - second).length() <= coordinateTolerance;
}

bool isCellBoundarySegment(const World &world,
                           Vector2d first,
                           Vector2d second,
                           double coordinateTolerance) {
    for (const auto &cell : world.division().cells) {
        for (std::size_t index = 0; index < cell.vertices.size(); ++index) {
            const auto &start = cell.vertices[index];
            const auto &end = cell.vertices[(index + 1) % cell.vertices.size()];
            if ((pointsNear(first, start, coordinateTolerance)
                 && pointsNear(second, end, coordinateTolerance))
                || (pointsNear(first, end, coordinateTolerance)
                    && pointsNear(second, start, coordinateTolerance))) {
                return true;
            }
        }
    }
    return false;
}

double riverVertexElevation(const World &world,
                            Vector2d vertex,
                            double coordinateTolerance) {
    auto elevation = 0.0;
    auto samples = std::size_t{0};
    for (const auto &cell : world.division().cells) {
        auto containsVertex = false;
        for (const auto &cellVertex : cell.vertices) {
            if (pointsNear(vertex, cellVertex, coordinateTolerance)) {
                containsVertex = true;
                break;
            }
        }
        if (!containsVertex)
            continue;

        elevation += world.regions().at(cell.id).elevation();
        ++samples;
    }
    require(samples > 0, "A river node does not correspond to a cell vertex.");
    return elevation / static_cast<double>(samples);
}

bool riverVertexTouchesWater(const World &world,
                             Vector2d vertex,
                             double coordinateTolerance) {
    for (const auto &cell : world.division().cells) {
        if (!world.regions().at(cell.id).isWater())
            continue;
        for (const auto &cellVertex : cell.vertices) {
            if (pointsNear(vertex, cellVertex, coordinateTolerance))
                return true;
        }
    }
    return false;
}

bool riverVertexTouchesSourceCandidate(const World &world,
                                       Vector2d vertex,
                                       double minimumElevation,
                                       double coordinateTolerance) {
    for (const auto &cell : world.division().cells) {
        const auto &region = world.regions().at(cell.id);
        if (!region.isLand() || region.elevation() < minimumElevation)
            continue;
        for (const auto &cellVertex : cell.vertices) {
            if (pointsNear(vertex, cellVertex, coordinateTolerance))
                return true;
        }
    }
    return false;
}

void requireRiverEdgeLinks(const World &world,
                           RiverId river,
                           Vector2d first,
                           Vector2d second,
                           double coordinateTolerance) {
    auto linkedRegionEdges = std::size_t{0};
    for (const auto &cell : world.division().cells) {
        for (std::size_t edge = 0; edge < cell.vertices.size(); ++edge) {
            const auto &start = cell.vertices[edge];
            const auto &end = cell.vertices[(edge + 1) % cell.vertices.size()];
            if (!((pointsNear(first, start, coordinateTolerance)
                   && pointsNear(second, end, coordinateTolerance))
                  || (pointsNear(first, end, coordinateTolerance)
                      && pointsNear(second, start, coordinateTolerance)))) {
                continue;
            }

            require(world.regions().at(cell.id).riverAtEdge(edge) == river,
                    "A region edge does not reference its river segment.");
            ++linkedRegionEdges;
        }
    }
    require(linkedRegionEdges > 0,
            "A river edge is not associated with any region edge.");
}

void testFractalNoise() {
    const Vector2d position{12.5, -3.75};
    constexpr std::uint64_t seed = 42;
    constexpr double frequency = 0.02;

    const auto oneOctave = noise::fractalNoise(position, seed, 1, frequency);
    const auto expectedOneOctave = noise::perlinNoise(position * frequency,
                                                     seed,
                                                     true);
    requireNear(oneOctave,
                expectedOneOctave,
                "Single-octave fractal noise does not match normalized Perlin noise.");

    constexpr std::uint32_t octaves = 3;
    constexpr double lacunarity = 2.0;
    constexpr double persistence = 0.5;
    auto expected = 0.0;
    auto totalStrength = 0.0;
    auto octaveFrequency = frequency;
    auto strength = 1.0;
    for (std::uint32_t octave = 0; octave < octaves; ++octave) {
        expected += noise::perlinNoise(position * octaveFrequency, seed, true)
                    * strength;
        totalStrength += strength;
        octaveFrequency *= lacunarity;
        strength *= persistence;
    }
    expected /= totalStrength;

    requireNear(noise::fractalNoise(position,
                                    seed,
                                    octaves,
                                    frequency,
                                    lacunarity,
                                    persistence),
                expected,
                "Fractal noise is not normalized by total octave strength.");
    require(std::isnan(noise::fractalNoise(position, seed, 0)),
            "Zero-octave fractal noise should be invalid.");
}

void testSmoothEdgeDecay() {
    const BoundingBox bounds{{0.0, 0.0}, {100.0, 100.0}};
    const Vector2d radius{20.0, 20.0};

    requireNear(noise::edgeDecay(bounds, radius, {0.0, 50.0}),
                1.0,
                "Edge decay must be one on the boundary.");
    requireNear(noise::edgeDecay(bounds, radius, {20.0, 50.0}),
                0.0,
                "Edge decay must reach zero at the decay radius.");
    requireNear(noise::edgeDecay(bounds, radius, {5.0, 50.0}),
                0.84375,
                "Edge decay does not use a smoothstep transition.");
    requireNear(noise::edgeDecay(bounds, radius, {50.0, 50.0}),
                0.0,
                "Edge decay must remain zero in the interior.");
}

void testEffectiveSeaLevel() {
    WorldGenerationSettings settings{
        .bounds = {{0.0, 0.0}, {90.0, 90.0}},
        .seed = 7,
        .columns = 3,
        .rows = 3,
        .jitter = 0.0,
        .seaLevel = 0.45,
        .edgeDecayRatio = {0.4, 0.4},
        .edgeStrength = 0.55,
        .noiseOctaves = 3,
        .noiseFrequency = 0.02,
        .noiseLacunarity = 2.0,
        .noisePersistence = 0.5,
    };
    const auto world = WorldGenerator{settings}.generate();
    const Vector2d decayRadius{
        90.0 * settings.edgeDecayRatio.x,
        90.0 * settings.edgeDecayRatio.y,
    };

    require(world.division().cells.size() == world.regions().size(),
            "Every generated cell must have a region.");
    for (const auto &region : world.regions()) {
        const auto &cell = world.division().cells.at(region.cell());
        const auto elevation = noise::fractalNoise(cell.sitePosition,
                                                   settings.seed,
                                                   settings.noiseOctaves,
                                                   settings.noiseFrequency,
                                                   settings.noiseLacunarity,
                                                   settings.noisePersistence);
        const auto effectiveSeaLevel = settings.seaLevel
                                       + noise::edgeDecay(settings.bounds,
                                                          decayRadius,
                                                          cell.sitePosition)
                                             * settings.edgeStrength;

        requireNear(region.elevation(),
                    elevation,
                    "Edge decay must not alter generated elevation.");
        require(region.isWater() == (elevation < effectiveSeaLevel),
                "Region classification does not use the effective sea level.");
    }
}

void testEdgeStrengthValidation() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {10.0, 10.0}},
    };
    requireNear(settings.edgeStrength,
                0.55,
                "The default edge strength must be 0.55.");

    settings.edgeStrength = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative edge strength was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.edgeStrength = 1.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "An edge strength above one was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.edgeStrength = std::numeric_limits<double>::infinity();
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A non-finite edge strength was accepted.");
    } catch (const std::invalid_argument &) {
    }
}

void testRivers() {
    const WorldGenerationSettings settings{
        .bounds = {{0.0, 0.0}, {512.0, 512.0}},
        .seed = 93,
        .columns = 16,
        .rows = 16,
        .jitter = 0.8,
        .seaLevel = 0.3,
        .edgeDecayRatio = {0.1, 0.1},
        .edgeStrength = 0.2,
        .noiseOctaves = 5,
        .noiseFrequency = 0.01,
        .noiseLacunarity = 2.0,
        .noisePersistence = 0.5,
        .riverSourceCount = 24,
        .riverMinimumSourceElevation = 0.5,
        .riverRandomness = 0.35,
        .riverElevationTolerance = 0.03,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto repeated = WorldGenerator{settings}.generate();
    constexpr double coordinateTolerance = 1e-7;

    require(!world.rivers().empty(), "River generation did not produce any rivers.");
    require(world.rivers().size() == repeated.rivers().size(),
            "River counts are not deterministic.");

    std::vector<std::size_t> incomingSegments(world.rivers().size(), 0);
    for (const auto &river : world.rivers()) {
        if (river.downstreamRiver != INVALID_RIVER_ID) {
            require(river.downstreamRiver < world.rivers().size(),
                    "A river references an invalid downstream segment.");
            ++incomingSegments[river.downstreamRiver];
        }
    }

    auto headwaterSegments = std::size_t{0};
    auto hasConfluence = false;
    auto hasToleratedRise = false;
    std::vector<std::pair<Vector2d, Vector2d>> channelEdges;

    for (std::size_t riverIndex = 0;
         riverIndex < world.rivers().size();
         ++riverIndex) {
        const auto &river = world.rivers()[riverIndex];
        const auto &repeatedRiver = repeated.rivers()[riverIndex];
        require(river.nodes.size() >= 2, "A river has fewer than two nodes.");
        require(river.nodes.size() == repeatedRiver.nodes.size(),
                "River node counts are not deterministic.");
        require(river.downstreamRiver == repeatedRiver.downstreamRiver,
                "Downstream river links are not deterministic.");

        if (incomingSegments[riverIndex] == 0) {
            ++headwaterSegments;
            require(riverVertexTouchesSourceCandidate(
                        world,
                        river.nodes.front().vertex,
                        settings.riverMinimumSourceElevation,
                        coordinateTolerance),
                    "A river does not start on a high land candidate region.");
            requireNear(river.nodes.front().strength,
                        1.0,
                        "A headwater river must start with unit strength.");
        }

        for (std::size_t nodeIndex = 0;
             nodeIndex < river.nodes.size();
             ++nodeIndex) {
            const auto &node = river.nodes[nodeIndex];
            const auto &repeatedNode = repeatedRiver.nodes[nodeIndex];
            require(node.vertex == repeatedNode.vertex,
                    "River vertices are not deterministic.");
            require(node.strength == repeatedNode.strength,
                    "River strengths are not deterministic.");

            if (nodeIndex == 0)
                continue;
            const auto &previous = river.nodes[nodeIndex - 1];
            require(node.strength >= previous.strength,
                    "River strength decreases downstream.");
            require(isCellBoundarySegment(world,
                                          previous.vertex,
                                          node.vertex,
                                          coordinateTolerance),
                    "A river segment does not follow a cell boundary.");
            const auto previousElevation = riverVertexElevation(
                world,
                previous.vertex,
                coordinateTolerance);
            const auto nodeElevation = riverVertexElevation(world,
                                                            node.vertex,
                                                            coordinateTolerance);
            require(nodeElevation
                        <= previousElevation
                               + settings.riverElevationTolerance
                               + tolerance,
                    "A river exceeds the configured elevation tolerance.");
            hasToleratedRise = hasToleratedRise
                               || nodeElevation > previousElevation + tolerance;
            requireRiverEdgeLinks(world,
                                  static_cast<RiverId>(riverIndex),
                                  previous.vertex,
                                  node.vertex,
                                  coordinateTolerance);

            const auto edge = std::pair{previous.vertex, node.vertex};
            require(std::ranges::find(channelEdges, edge) == channelEdges.end(),
                    "A shared downstream river edge was stored more than once.");
            channelEdges.push_back(edge);
        }

        if (river.downstreamRiver == INVALID_RIVER_ID) {
            require(riverVertexTouchesWater(world,
                                            river.nodes.back().vertex,
                                            coordinateTolerance),
                    "A terminal river segment does not reach water.");
            continue;
        }

        const auto &downstream = world.rivers()[river.downstreamRiver];
        require(river.nodes.back().vertex == downstream.nodes.front().vertex,
                "Linked river segments do not meet at their confluence.");
        require(river.nodes.back().strength == downstream.nodes.front().strength,
                "Linked river segments disagree about confluence strength.");

        auto linked = river.downstreamRiver;
        auto remaining = world.rivers().size();
        while (linked != INVALID_RIVER_ID && remaining > 0) {
            linked = world.rivers()[linked].downstreamRiver;
            --remaining;
        }
        require(remaining > 0 || linked == INVALID_RIVER_ID,
                "Downstream river links contain a cycle.");
    }

    require(headwaterSegments <= settings.riverSourceCount,
            "River generation exceeded the requested headwater count.");
    for (std::size_t downstreamId = 0;
         downstreamId < world.rivers().size();
         ++downstreamId) {
        if (incomingSegments[downstreamId] < 2)
            continue;

        hasConfluence = true;
        auto incomingStrength = 0.0;
        for (const auto &river : world.rivers()) {
            if (river.downstreamRiver == downstreamId) {
                incomingStrength += river.nodes[river.nodes.size() - 2].strength;
            }
        }
        requireNear(world.rivers()[downstreamId].nodes.front().strength,
                    incomingStrength,
                    "Confluence strength is not the sum of incoming rivers.");
    }
    require(hasConfluence, "The river fixture did not exercise a confluence.");
    require(hasToleratedRise,
            "The river fixture did not exercise the elevation tolerance.");

    for (const auto &region : world.regions()) {
        for (const auto river : region.edgeRivers()) {
            require(river == INVALID_RIVER_ID || river < world.rivers().size(),
                    "A region edge references an invalid river segment.");
        }
    }

    auto disabledSettings = settings;
    disabledSettings.riverSourceCount = 0;
    require(WorldGenerator{disabledSettings}.generate().rivers().empty(),
            "A zero river count did not disable river generation.");
}

void testRiverSettingsValidation() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {10.0, 10.0}},
    };

    settings.riverMinimumSourceElevation = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative minimum river source elevation was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.riverMinimumSourceElevation = 0.6;
    settings.riverRandomness = 1.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "River randomness above one was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.riverRandomness = 0.25;
    settings.riverElevationTolerance = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative river elevation tolerance was accepted.");
    } catch (const std::invalid_argument &) {
    }
}

} // namespace

int main() {
    try {
        testFractalNoise();
        testSmoothEdgeDecay();
        testEffectiveSeaLevel();
        testEdgeStrengthValidation();
        testRivers();
        testRiverSettingsValidation();
        std::cout << "World generation tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "World generation test failed: " << error.what() << '\n';
        return 1;
    }
}
