#include "ClimateGenerator.h"
#include "Id.h"
#include "PerlinNoise.h"
#include "ProvinceGenerator.h"
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

long double quantizedProvinceCost(double cost) {
    return std::floor(static_cast<long double>(cost)
                      / static_cast<long double>(EPS));
}

double normalizedDistance(const BoundingBox &bounds,
                          Vector2d first,
                          Vector2d second) {
    const auto deltaX = static_cast<long double>(first.x)
                        - static_cast<long double>(second.x);
    const auto deltaY = static_cast<long double>(first.y)
                        - static_cast<long double>(second.y);
    const auto width = static_cast<long double>(bounds.max.x)
                       - static_cast<long double>(bounds.min.x);
    const auto height = static_cast<long double>(bounds.max.y)
                        - static_cast<long double>(bounds.min.y);
    return static_cast<double>(std::hypot(deltaX, deltaY)
                               / std::hypot(width, height));
}

bool pointsNear(Vector2d first, Vector2d second, double coordinateTolerance) {
    return (first - second).length() <= coordinateTolerance;
}

bool segmentsShareBoundary(Vector2d firstStart,
                           Vector2d firstEnd,
                           Vector2d secondStart,
                           Vector2d secondEnd,
                           double coordinateTolerance) {
    const auto firstDirection = firstEnd - firstStart;
    const auto secondDirection = secondEnd - secondStart;
    const auto firstLength = firstDirection.length();
    const auto secondLength = secondDirection.length();
    if (firstLength <= coordinateTolerance
        || secondLength <= coordinateTolerance) {
        return false;
    }

    const Vector2d unit{firstDirection.x / firstLength,
                        firstDirection.y / firstLength};
    const auto perpendicularDistance = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return std::abs(relative.x * unit.y - relative.y * unit.x);
    };
    if (perpendicularDistance(secondStart) > coordinateTolerance
        || perpendicularDistance(secondEnd) > coordinateTolerance) {
        return false;
    }

    const auto project = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return relative.x * unit.x + relative.y * unit.y;
    };
    auto secondMinimum = project(secondStart);
    auto secondMaximum = project(secondEnd);
    if (secondMinimum > secondMaximum)
        std::swap(secondMinimum, secondMaximum);
    const auto overlap = std::min(firstLength, secondMaximum)
                         - std::max(0.0, secondMinimum);
    return overlap > coordinateTolerance;
}

bool sharedBoundaryHasRiver(const World &world,
                            RegionId first,
                            RegionId second,
                            double coordinateTolerance) {
    const auto edgeTouchesCell = [&](RegionId riverRegion, RegionId otherRegion) {
        const auto &riverCell = world.division().cells.at(riverRegion);
        const auto &region = world.regions().at(riverRegion);
        const auto &otherCell = world.division().cells.at(otherRegion);
        for (std::size_t edge = 0; edge < riverCell.vertices.size(); ++edge) {
            if (!region.hasRiverAtEdge(edge))
                continue;
            const auto next = (edge + 1) % riverCell.vertices.size();
            for (std::size_t otherEdge = 0;
                 otherEdge < otherCell.vertices.size();
                 ++otherEdge) {
                const auto otherNext = (otherEdge + 1)
                                       % otherCell.vertices.size();
                if (segmentsShareBoundary(riverCell.vertices[edge],
                                          riverCell.vertices[next],
                                          otherCell.vertices[otherEdge],
                                          otherCell.vertices[otherNext],
                                          coordinateTolerance)) {
                    return true;
                }
            }
        }
        return false;
    };
    return edgeTouchesCell(first, second) || edgeTouchesCell(second, first);
}

bool requireProvinceGrowth(const World &world,
                           const WorldGenerationSettings &settings,
                           const World *repeated = nullptr) {
    const auto regionCount = world.regions().size();
    const auto landRegionCount = static_cast<std::size_t>(std::ranges::count_if(
        world.regions(),
        [](const Region &region) { return region.isLand(); }));
    require(!world.provinces().empty() || landRegionCount == 0,
            "A world with land has no provinces.");
    if (repeated != nullptr) {
        require(world.provinces().size() == repeated->provinces().size(),
                "Province counts are not deterministic.");
    }

    constexpr double coordinateTolerance = 1e-7;
    std::vector<bool> assigned(regionCount, false);
    for (const auto &region : world.regions()) {
        if (!region.isWater())
            continue;
        assigned[region.id()] = true;
        require(!region.hasProvince(),
                "A water region stores a province ID.");
        if (repeated != nullptr) {
            require(!repeated->regions().at(region.id()).hasProvince(),
                    "Water-region province ownership is not deterministic.");
        }
    }
    auto assignedCount = std::size_t{0};
    auto sawRiverFrontier = false;
    for (std::size_t provinceIndex = 0;
         provinceIndex < world.provinces().size();
         ++provinceIndex) {
        const auto &province = world.provinces()[provinceIndex];
        require(!province.regionIds().empty(), "A province has no regions.");
        require(province.seedRegion() == province.regionIds().front(),
                "A province seed is not its first region.");

        const auto firstUnassigned = std::ranges::find(assigned, false);
        require(firstUnassigned != assigned.end(),
                "A province was created after all regions were assigned.");
        require(province.seedRegion()
                    == static_cast<RegionId>(firstUnassigned - assigned.begin()),
                "A province did not use the lowest unassigned region as its seed.");

        if (repeated != nullptr) {
            const auto &repeatedProvince = repeated->provinces()[provinceIndex];
            require(province.seedRegion() == repeatedProvince.seedRegion(),
                    "Province seeds are not deterministic.");
            require(province.regionIds() == repeatedProvince.regionIds(),
                    "Province memberships are not deterministic.");
            require(province.remainingScore()
                        == repeatedProvince.remainingScore(),
                    "Province remaining scores are not deterministic.");
        }

        auto remainingScore = settings.provinceStartScore;
        const auto provinceCenter = world.division()
                                        .cells.at(province.seedRegion())
                                        .sitePosition;
        std::vector<RegionId> claimed;
        claimed.reserve(province.regionIds().size());
        for (const auto actualRegion : province.regionIds()) {
            require(world.regions().at(actualRegion).isLand(),
                    "A province contains a water region.");
            require(world.regions().at(actualRegion).provinceId()
                        == provinceIndex,
                    "A region stores an incorrect province ID.");
            if (repeated != nullptr) {
                require(repeated->regions().at(actualRegion).provinceId()
                            == provinceIndex,
                        "Region province IDs are not deterministic.");
            }
            if (claimed.empty()) {
                require(actualRegion == province.seedRegion(),
                        "A province claim order does not begin with its seed.");
                require(!assigned.at(actualRegion),
                        "A province seed was already assigned.");
                assigned[actualRegion] = true;
                ++assignedCount;
                claimed.push_back(actualRegion);
                continue;
            }

            auto bestCost = std::numeric_limits<double>::infinity();
            auto bestCostOrder = std::numeric_limits<long double>::infinity();
            auto bestRegion = INVALID_REGION_ID;
            auto bestSource = INVALID_REGION_ID;
            for (const auto source : claimed) {
                const auto &sourceCell = world.division().cells.at(source);
                for (const auto neighbor : sourceCell.neighbors) {
                    if (assigned.at(neighbor))
                        continue;
                    const auto crossesRiver = sharedBoundaryHasRiver(
                        world,
                        source,
                        static_cast<RegionId>(neighbor),
                        coordinateTolerance);
                    sawRiverFrontier = sawRiverFrontier || crossesRiver;
                    const auto elevationDifference = std::abs(
                        world.regions().at(source).elevation()
                        - world.regions().at(neighbor).elevation());
                    const auto cost = settings.provinceBaseCost
                                      + settings.provinceElevationContribution
                                            * elevationDifference
                                      + settings.provinceDistanceContribution
                                            * normalizedDistance(
                                                settings.bounds,
                                                provinceCenter,
                                                world.division()
                                                    .cells.at(neighbor)
                                                    .sitePosition)
                                      + (crossesRiver
                                             ? settings.provinceRiverContribution
                                             : 0.0);
                    const auto costOrder = quantizedProvinceCost(cost);
                    const auto neighborRegion = static_cast<RegionId>(neighbor);
                    if (costOrder < bestCostOrder
                        || (costOrder == bestCostOrder
                            && (neighborRegion < bestRegion
                                || (neighborRegion == bestRegion
                                    && source < bestSource)))) {
                        bestCost = cost;
                        bestCostOrder = costOrder;
                        bestRegion = neighborRegion;
                        bestSource = source;
                    }
                }
            }

            require(bestRegion != INVALID_REGION_ID,
                    "A province claimed a non-neighboring region.");
            require(actualRegion == bestRegion,
                    "A province did not claim its cheapest frontier region.");
            require(bestCost <= remainingScore + EPS,
                    "A province claimed a region it could not afford.");
            remainingScore = std::max(0.0, remainingScore - bestCost);
            assigned[actualRegion] = true;
            ++assignedCount;
            claimed.push_back(actualRegion);
        }

        auto cheapestRemaining = std::numeric_limits<double>::infinity();
        for (const auto source : claimed) {
            for (const auto neighbor : world.division().cells.at(source).neighbors) {
                if (assigned.at(neighbor))
                    continue;
                const auto crossesRiver = sharedBoundaryHasRiver(
                    world,
                    source,
                    static_cast<RegionId>(neighbor),
                    coordinateTolerance);
                sawRiverFrontier = sawRiverFrontier || crossesRiver;
                const auto elevationDifference = std::abs(
                    world.regions().at(source).elevation()
                    - world.regions().at(neighbor).elevation());
                cheapestRemaining = std::min(
                    cheapestRemaining,
                    settings.provinceBaseCost
                        + settings.provinceElevationContribution
                              * elevationDifference
                        + settings.provinceDistanceContribution
                              * normalizedDistance(
                                  settings.bounds,
                                  provinceCenter,
                                  world.division().cells.at(neighbor).sitePosition)
                        + (crossesRiver
                               ? settings.provinceRiverContribution
                               : 0.0));
            }
        }
        require(cheapestRemaining > remainingScore + EPS,
                "A province stopped with an affordable frontier region.");
        requireNear(province.remainingScore(),
                    remainingScore,
                    "A province stored an incorrect remaining score.");
    }

    require(assignedCount == landRegionCount,
            "Provinces do not assign every land region exactly once.");
    return sawRiverFrontier;
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

void testRegionClimate() {
    const WorldGenerationSettings settings{
        .bounds = {{0.0, 0.0}, {120.0, 100.0}},
        .seed = 211,
        .columns = 4,
        .rows = 4,
        .jitter = 0.0,
        .noiseOctaves = 4,
        .noiseFrequency = 0.017,
        .noiseLacunarity = 2.0,
        .noisePersistence = 0.5,
        .equatorTemperature = 40.0,
        .poleTemperature = -30.0,
        .vegetationCoefficient = 1.4,
        .humidityCoefficient = 0.8,
        .riverSourceCount = 0,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto repeated = WorldGenerator{settings}.generate();
    const climate::ClimateGenerator climateGenerator{{
        .bounds = settings.bounds,
        .seed = settings.seed,
        .equatorTemperature = settings.equatorTemperature,
        .poleTemperature = settings.poleTemperature,
        .vegetationCoefficient = settings.vegetationCoefficient,
        .humidityCoefficient = settings.humidityCoefficient,
        .noiseOctaves = settings.noiseOctaves,
        .noiseFrequency = settings.noiseFrequency,
        .noiseLacunarity = settings.noiseLacunarity,
        .noisePersistence = settings.noisePersistence,
    }};

    for (const auto &region : world.regions()) {
        const auto &cell = world.division().cells.at(region.cell());
        const auto expected = climateGenerator.sample(cell.sitePosition);
        requireNear(region.temperature(),
                    expected.temperature,
                    "A region stores an incorrect temperature.");
        requireNear(region.humidity(),
                    expected.humidity,
                    "A region stores incorrect humidity.");
        requireNear(region.vegetation(),
                    expected.vegetation,
                    "A region stores incorrect vegetation.");
        require(region.temperature()
                    == repeated.regions().at(region.id()).temperature()
                    && region.humidity()
                           == repeated.regions().at(region.id()).humidity()
                    && region.vegetation()
                           == repeated.regions().at(region.id()).vegetation(),
                "Region climate values are not deterministic.");
    }

    requireNear(world.regions().at(0).temperature(),
                world.regions().at(12).temperature(),
                "Top and bottom temperatures are not symmetric.");
    require(world.regions().at(4).temperature()
                > world.regions().at(0).temperature(),
            "Regions do not become warmer toward the equator.");
}

void testClimateSettingsValidation() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {10.0, 10.0}},
    };
    requireNear(settings.equatorTemperature,
                30.0,
                "The default equator temperature must be 30.");
    requireNear(settings.poleTemperature,
                -20.0,
                "The default pole temperature must be -20.");
    requireNear(settings.vegetationCoefficient,
                1.0,
                "The default vegetation coefficient must be one.");
    requireNear(settings.humidityCoefficient,
                1.0,
                "The default humidity coefficient must be one.");

    settings.equatorTemperature = 50.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "An equator temperature above 50 was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.equatorTemperature = 30.0;
    settings.poleTemperature = 31.0;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A pole warmer than the equator was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.poleTemperature = -20.0;
    settings.vegetationCoefficient = 2.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A vegetation coefficient above two was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.vegetationCoefficient = 1.0;
    settings.humidityCoefficient = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative humidity coefficient was accepted.");
    } catch (const std::invalid_argument &) {
    }
}

void testProvinceBudgets() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {150.0, 120.0}},
        .seed = 417,
        .columns = 5,
        .rows = 4,
        .jitter = 0.0,
        .seaLevel = 0.0,
        .edgeStrength = 0.0,
        .riverSourceCount = 0,
        .provinceStartScore = 2.5,
        .provinceRiverContribution = 0.0,
        .provinceElevationContribution = 0.0,
        .provinceDistanceContribution = 0.0,
        .provinceShortBorderContribution = 0.0,
        .provinceBaseCost = 1.0,
        .provinceMinimumRegionCount = 1,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto repeated = WorldGenerator{settings}.generate();

    requireProvinceGrowth(world, settings, &repeated);
    for (const auto &province : world.provinces()) {
        require(province.regionIds().size() <= 3,
                "A province exceeded its fixed base-cost budget.");
    }

    auto distanceSettings = settings;
    distanceSettings.provinceDistanceContribution = 10.0;
    const auto distanceWorld = WorldGenerator{distanceSettings}.generate();
    require(distanceWorld.provinces().size() == distanceWorld.regions().size(),
            "Province distance cost did not limit growth from the seed center.");
    requireProvinceGrowth(distanceWorld, distanceSettings);

    settings.provinceStartScore = 0.0;
    const auto noBudgetWorld = WorldGenerator{settings}.generate();
    require(noBudgetWorld.provinces().size() == noBudgetWorld.regions().size(),
            "Zero-score provinces claimed regions with a positive base cost.");
    requireProvinceGrowth(noBudgetWorld, settings);

    settings.provinceBaseCost = 0.0;
    const auto freeWorld = WorldGenerator{settings}.generate();
    require(freeWorld.provinces().size() == 1,
            "Free claims did not combine a connected world into one province.");
    requireProvinceGrowth(freeWorld, settings);

    settings.seaLevel = 1.0;
    settings.edgeStrength = 1.0;
    const auto waterWorld = WorldGenerator{settings}.generate();
    require(std::ranges::all_of(waterWorld.regions(),
                                &Region::isWater),
            "The water-only province fixture contains land.");
    require(waterWorld.provinces().empty(),
            "A water-only world generated provinces.");
    requireProvinceGrowth(waterWorld, settings);
}

void testProvinceCostOrdering() {
    const BoundingBox bounds{{0.0, 0.0}, {10.0, 10.0}};
    const WorldDivision division{
        .cells = {
            {.id = 0, .sitePosition = {5.0, 5.0}, .neighbors = {1, 2}},
            {.id = 1, .sitePosition = {4.0, 5.0}, .neighbors = {0}},
            {.id = 2, .sitePosition = {6.0, 5.0}, .neighbors = {0}},
        },
    };
    const auto generate = [&](double firstCost, double secondCost) {
        std::vector<Region> regions;
        regions.emplace_back(0, 0.0, 0.0, 0, 0.0, 0.0, 0.0);
        regions.emplace_back(1, firstCost, 0.0, 0, 0.0, 0.0, 0.0);
        regions.emplace_back(2, secondCost, 0.0, 0, 0.0, 0.0, 0.0);
        return generateProvinces(bounds,
                                 division,
                                 regions,
                                 10.0 * EPS,
                                 0.0,
                                 1.0,
                                 0.0,
                                 0.0,
                                 1,
                                 0.0);
    };

    const auto epsilonTied = generate(0.75 * EPS, 0.25 * EPS);
    require(epsilonTied.size() == 1,
            "EPS-equivalent claims did not form one province.");
    require(epsilonTied.front().regionIds()
                == std::vector<RegionId>{0, 1, 2},
            "EPS-equivalent claim costs did not use the region-ID tie-breaker.");

    const auto distinct = generate(1.25 * EPS, 0.25 * EPS);
    require(distinct.size() == 1,
            "Distinct claim costs did not form one province.");
    require(distinct.front().regionIds()
                == std::vector<RegionId>{0, 2, 1},
            "Province growth did not choose the cheapest frontier claim.");
}

void testProvinceShortBorderPenalty() {
    const BoundingBox bounds{{0.0, 0.0}, {5.0, 4.0}};
    const WorldDivision division{
        .cells = {
            {
                .id = 0,
                .sitePosition = {2.0, 2.0},
                .vertices = {{0.0, 0.0},
                             {4.0, 0.0},
                             {4.0, 4.0},
                             {0.0, 4.0}},
                .neighbors = {1, 2},
            },
            {
                .id = 1,
                .sitePosition = {4.5, 0.5},
                .vertices = {{4.0, 0.0},
                             {5.0, 0.0},
                             {5.0, 1.0},
                             {4.0, 1.0}},
                .neighbors = {0},
            },
            {
                .id = 2,
                .sitePosition = {4.5, 2.5},
                .vertices = {{4.0, 1.0},
                             {5.0, 1.0},
                             {5.0, 4.0},
                             {4.0, 4.0}},
                .neighbors = {0},
            },
        },
    };
    const auto generate = [&](double shortBorderContribution) {
        std::vector<Region> regions;
        for (CellId cell = 0; cell < division.cells.size(); ++cell) {
            regions.emplace_back(cell,
                                 0.0,
                                 0.0,
                                 division.cells[cell].vertices.size(),
                                 0.0,
                                 0.0,
                                 0.0);
        }
        return generateProvinces(bounds,
                                 division,
                                 regions,
                                 10.0,
                                 0.0,
                                 0.0,
                                 0.0,
                                 0.0,
                                 1,
                                 shortBorderContribution);
    };

    const auto disabled = generate(0.0);
    require(disabled.front().regionIds()
                == std::vector<RegionId>{0, 1, 2},
            "Disabled short-border cost changed province claim ordering.");

    const auto enabled = generate(5.0);
    require(enabled.size() == 1,
            "Short-border cost unexpectedly split an affordable province.");
    require(enabled.front().regionIds()
                == std::vector<RegionId>{0, 2, 1},
            "A longer shared border was not preferred over a short border.");
}

void testSmallProvinceMerging() {
    const BoundingBox bounds{{0.0, 0.0}, {10.0, 10.0}};
    const WorldDivision splitDivision{
        .cells = {
            {.id = 0, .sitePosition = {0.0, 0.0}, .neighbors = {1}},
            {.id = 1, .sitePosition = {1.0, 0.0}, .neighbors = {0, 2}},
            {.id = 2, .sitePosition = {2.0, 0.0}, .neighbors = {1, 3}},
            {.id = 3, .sitePosition = {3.0, 0.0}, .neighbors = {2, 4}},
            {.id = 4, .sitePosition = {4.0, 0.0}, .neighbors = {3, 5}},
            {.id = 5, .sitePosition = {5.0, 0.0}, .neighbors = {4, 6}},
            {.id = 6, .sitePosition = {6.0, 0.0}, .neighbors = {5, 7}},
            {.id = 7, .sitePosition = {7.0, 0.0}, .neighbors = {6}},
        },
    };
    std::vector<Region> splitRegions;
    for (CellId cell = 0; cell < splitDivision.cells.size(); ++cell) {
        const auto elevation = cell >= 5 ? 1.0 : 0.0;
        splitRegions.emplace_back(cell,
                                  elevation,
                                  0.0,
                                  0,
                                  0.0,
                                  0.0,
                                  0.0);
    }

    const auto splitProvinces = generateProvinces(bounds,
                                                  splitDivision,
                                                  splitRegions,
                                                  2.0,
                                                  0.0,
                                                  3.0,
                                                  0.0,
                                                  1.0,
                                                  3,
                                                  0.0);
    require(splitProvinces.size() == 2,
            "An undersized province with neighbors was not deleted.");
    require(splitProvinces[0].regionIds()
                == std::vector<RegionId>{0, 1, 2, 3},
            "A small-province region did not join its neighboring province.");
    require(splitProvinces[1].regionIds()
                == std::vector<RegionId>{5, 6, 7, 4},
            "Small-province regions were not reassigned independently.");
    for (RegionId region = 0; region < splitRegions.size(); ++region) {
        const auto expectedProvince = region >= 4 ? ProvinceId{1}
                                                  : ProvinceId{0};
        require(splitRegions[region].provinceId() == expectedProvince,
                "A merged region retained its deleted province ID.");
    }

    const WorldDivision allSmallDivision{
        .cells = {
            {.id = 0, .sitePosition = {0.0, 0.0}, .neighbors = {1}},
            {.id = 1, .sitePosition = {1.0, 0.0}, .neighbors = {0, 2}},
            {.id = 2, .sitePosition = {2.0, 0.0}, .neighbors = {1, 3}},
            {.id = 3, .sitePosition = {3.0, 0.0}, .neighbors = {2, 4}},
            {.id = 4, .sitePosition = {4.0, 0.0}, .neighbors = {3}},
        },
    };
    std::vector<Region> allSmallRegions;
    for (CellId cell = 0; cell < allSmallDivision.cells.size(); ++cell)
        allSmallRegions.emplace_back(cell, 0.0, 0.0, 0, 0.0, 0.0, 0.0);
    const auto combined = generateProvinces(bounds,
                                            allSmallDivision,
                                            allSmallRegions,
                                            0.0,
                                            0.0,
                                            0.0,
                                            0.0,
                                            1.0,
                                            3,
                                            0.0);
    require(combined.size() == 1,
            "A connected group of small provinces did not retain one target.");
    require(combined.front().regionIds()
                == std::vector<RegionId>{0, 1, 2, 3, 4},
            "Small provinces without a large neighbor were not combined deterministically.");
    require(std::ranges::all_of(allSmallRegions,
                                [](const Region &region) {
                                    return region.provinceId() == 0;
                                }),
            "Combined small provinces did not update every region owner.");

    const WorldDivision isolatedDivision{
        .cells = {
            {.id = 0, .sitePosition = {0.0, 0.0}, .neighbors = {1}},
            {.id = 1, .sitePosition = {1.0, 0.0}, .neighbors = {0}},
        },
    };
    std::vector<Region> isolatedRegions;
    isolatedRegions.emplace_back(0, 0.0, 0.0, 0, 0.0, 0.0, 0.0);
    isolatedRegions.emplace_back(1, 0.0, 0.0, 0, 0.0, 0.0, 0.0);
    const auto isolated = generateProvinces(bounds,
                                            isolatedDivision,
                                            isolatedRegions,
                                            1.0,
                                            0.0,
                                            0.0,
                                            0.0,
                                            1.0,
                                            3,
                                            0.0);
    require(isolated.size() == 1 && isolated.front().regionIds().size() == 2,
            "An undersized province without another province was deleted.");
}

void testProvinceSettingsValidation() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {10.0, 10.0}},
    };
    requireNear(settings.provinceStartScore,
                10.0,
                "The default province start score must be 10.");
    requireNear(settings.provinceRiverContribution,
                5.0,
                "The default province river contribution must be 5.");
    requireNear(settings.provinceElevationContribution,
                10.0,
                "The default province elevation contribution must be 10.");
    requireNear(settings.provinceDistanceContribution,
                5.0,
                "The default province distance contribution must be 5.");
    requireNear(settings.provinceShortBorderContribution,
                5.0,
                "The default province short-border contribution must be 5.");
    requireNear(settings.provinceBaseCost,
                1.0,
                "The default province base cost must be 1.");
    require(settings.provinceMinimumRegionCount == 3,
            "The default minimum province region count must be 3.");

    settings.provinceStartScore = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province start score was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceStartScore = 10.0;
    settings.provinceRiverContribution = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province river contribution was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceRiverContribution = 5.0;
    settings.provinceElevationContribution = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province elevation contribution was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceElevationContribution = 10.0;
    settings.provinceDistanceContribution = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province distance contribution was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceDistanceContribution = 5.0;
    settings.provinceBaseCost = std::numeric_limits<double>::infinity();
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A non-finite province base cost was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceBaseCost = 1.0;
    settings.provinceMinimumRegionCount = 0;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A zero minimum province region count was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceMinimumRegionCount = 3;
    settings.provinceShortBorderContribution = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province short-border contribution was accepted.");
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
        .provinceShortBorderContribution = 0.0,
        .provinceMinimumRegionCount = 1,
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

    require(requireProvinceGrowth(world, settings, &repeated),
            "The province fixture did not exercise a river frontier cost.");

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
        testRegionClimate();
        testClimateSettingsValidation();
        testProvinceBudgets();
        testProvinceCostOrdering();
        testProvinceShortBorderPenalty();
        testSmallProvinceMerging();
        testProvinceSettingsValidation();
        testRivers();
        testRiverSettingsValidation();
        std::cout << "World generation tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "World generation test failed: " << error.what() << '\n';
        return 1;
    }
}
