#include "ClimateGenerator.h"
#include "Id.h"
#include "JitteredGridSiteGenerator.h"
#include "Landform.h"
#include "LandType.h"
#include "PerlinNoise.h"
#include "ProvinceGenerator.h"
#include "WorldGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <ranges>
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

double maximumLandRelief(const World &world) {
    auto maximum = 0.0;
    for (const auto &region : world.regions()) {
        if (region.isLand()) {
            maximum = std::max(maximum,
                               region.elevation() - region.seaLevel());
        }
    }
    return maximum;
}

double normalizedLandElevation(const Region &region, double maximumRelief) {
    if (maximumRelief == 0.0)
        return 0.0;
    return std::clamp((region.elevation() - region.seaLevel())
                          / maximumRelief,
                      0.0,
                      1.0);
}

bool pointsNear(Vector2d first, Vector2d second, double coordinateTolerance) {
    return (first - second).length() <= coordinateTolerance;
}

Vector2d polygonCentroid(const std::vector<Vector2d> &vertices) {
    auto twiceArea = 0.0;
    Vector2d weightedCentroid;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const auto &current = vertices[index];
        const auto &next = vertices[(index + 1) % vertices.size()];
        const auto cross = current.x * next.y - current.y * next.x;
        twiceArea += cross;
        weightedCentroid += (current + next) * cross;
    }
    require(twiceArea != 0.0, "A test polygon has zero area.");
    return weightedCentroid / (3.0 * twiceArea);
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
        require(province.remainingScore() >= 0.0
                    && province.remainingScore()
                           <= settings.provinceStartScore + EPS,
                "A province stored an invalid remaining score.");

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
            require(!assigned.at(actualRegion),
                    "A land region belongs to multiple provinces.");
            if (!claimed.empty()) {
                require(std::ranges::any_of(
                            world.division().cells.at(actualRegion).neighbors,
                            [&](CellId neighbor) {
                                return std::ranges::find(claimed, neighbor)
                                       != claimed.end();
                            }),
                        "A province's stored growth order is not contiguous.");
            }
            assigned[actualRegion] = true;
            ++assignedCount;
            claimed.push_back(actualRegion);
        }
    }

    require(assignedCount == landRegionCount,
            "Provinces do not assign every land region exactly once.");

    for (std::size_t region = 0; region < world.regions().size(); ++region) {
        if (!world.regions()[region].isLand())
            continue;
        for (const auto neighbor : world.division().cells[region].neighbors) {
            if (neighbor <= region || !world.regions()[neighbor].isLand())
                continue;
            sawRiverFrontier = sawRiverFrontier
                               || sharedBoundaryHasRiver(
                                   world,
                                   static_cast<RegionId>(region),
                                   neighbor,
                                   coordinateTolerance);
        }
    }
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

void testLloydRelaxation() {
    const WorldGenerationSettings settings{
        .bounds = {{-20.0, 10.0}, {130.0, 110.0}},
        .seed = 8675309,
        .columns = 5,
        .rows = 4,
        .jitter = 0.95,
        .seaLevel = 1.0,
        .edgeStrength = 0.0,
        .riverSourceCount = 0,
    };
    require(settings.lloydRelaxationIterations == 0,
            "Lloyd relaxation must be disabled by default.");

    const auto unrelaxed = WorldGenerator{settings}.generate();
    const JitteredGridSiteGenerator initialSiteGenerator{
        settings.columns,
        settings.rows,
        settings.jitter,
        settings.seed,
    };
    const auto initialSites = initialSiteGenerator.generateSites(settings.bounds);
    require(initialSites.size() == unrelaxed.division().cells.size(),
            "The unrelaxed world changed the site count.");
    for (std::size_t index = 0; index < initialSites.size(); ++index) {
        require(unrelaxed.division().cells[index].sitePosition
                    == initialSites[index].position,
                "Zero Lloyd iterations changed an initial site.");
    }

    auto relaxedSettings = settings;
    relaxedSettings.lloydRelaxationIterations = 1;
    const auto relaxed = WorldGenerator{relaxedSettings}.generate();
    const auto repeated = WorldGenerator{relaxedSettings}.generate();
    require(relaxed.division().cells.size() == unrelaxed.division().cells.size(),
            "Lloyd relaxation changed the cell count.");

    auto movedSite = false;
    constexpr auto centroidTolerance = 1e-9;
    for (std::size_t index = 0;
         index < relaxed.division().cells.size();
         ++index) {
        const auto expected = polygonCentroid(
            unrelaxed.division().cells[index].vertices);
        const auto &cell = relaxed.division().cells[index];
        const auto &repeatedCell = repeated.division().cells[index];
        require(pointsNear(cell.sitePosition, expected, centroidTolerance),
                "A Lloyd-relaxed site is not its previous cell centroid.");
        require(relaxedSettings.bounds.contains(cell.sitePosition),
                "Lloyd relaxation moved a site outside the world bounds.");
        require(cell.sitePosition == repeatedCell.sitePosition,
                "Lloyd-relaxed sites are not deterministic.");
        require(cell.vertices == repeatedCell.vertices,
                "Lloyd-relaxed polygons are not deterministic.");
        movedSite = movedSite
                    || !pointsNear(cell.sitePosition,
                                   initialSites[index].position,
                                   centroidTolerance);
    }
    require(movedSite, "The Lloyd relaxation fixture did not move any sites.");

    auto twiceRelaxedSettings = relaxedSettings;
    twiceRelaxedSettings.lloydRelaxationIterations = 2;
    const auto twiceRelaxed = WorldGenerator{twiceRelaxedSettings}.generate();
    for (std::size_t index = 0;
         index < twiceRelaxed.division().cells.size();
         ++index) {
        const auto expected = polygonCentroid(
            relaxed.division().cells[index].vertices);
        require(pointsNear(twiceRelaxed.division().cells[index].sitePosition,
                           expected,
                           centroidTolerance),
                "Successive Lloyd iterations did not use the previous diagram.");
    }
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

void testWaterConnectivityTypes() {
    const WorldGenerationSettings settings{
        .bounds = {{0.0, 0.0}, {2048.0, 2048.0}},
    };
    const auto world = WorldGenerator{settings}.generate();
    std::vector<bool> expectedSea(world.regions().size(), false);
    std::vector<CellId> pending;
    constexpr auto boundaryTolerance = 1e-7;
    for (const auto &cell : world.division().cells) {
        const auto touchesBoundary = std::ranges::any_of(
            cell.vertices,
            [&](Vector2d vertex) {
                return std::abs(vertex.x - settings.bounds.min.x)
                           <= boundaryTolerance
                    || std::abs(vertex.x - settings.bounds.max.x)
                           <= boundaryTolerance
                    || std::abs(vertex.y - settings.bounds.min.y)
                           <= boundaryTolerance
                    || std::abs(vertex.y - settings.bounds.max.y)
                           <= boundaryTolerance;
            });
        if (world.regions()[cell.id].isWater() && touchesBoundary) {
            expectedSea[cell.id] = true;
            pending.push_back(cell.id);
        }
    }
    for (std::size_t next = 0; next < pending.size(); ++next) {
        for (const auto neighbor : world.division().cells[pending[next]].neighbors) {
            if (expectedSea[neighbor]
                || !world.regions()[neighbor].isWater()) {
                continue;
            }
            expectedSea[neighbor] = true;
            pending.push_back(neighbor);
        }
    }

    auto seaCount = std::size_t{0};
    auto lakeCount = std::size_t{0};
    for (const auto &region : world.regions()) {
        if (region.isLand()) {
            require(region.type() == RegionType::Land,
                    "A land region has a water region type.");
            continue;
        }
        require(region.isSea() == expectedSea[region.id()],
                "Water connectivity produced an incorrect sea/lake type.");
        require(region.isLake() != region.isSea(),
                "A water region is not exactly one of sea or lake.");
        seaCount += region.isSea();
        lakeCount += region.isLake();
    }
    require(seaCount > 0, "The default world generated no boundary sea.");
    require(lakeCount > 0, "The water-type fixture generated no inland lake.");
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
        .oceanHumidityCoefficient = 0.0,
        .riverSourceCount = 0,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto repeated = WorldGenerator{settings}.generate();
    const auto maximumRelief = maximumLandRelief(world);
    const climate::ClimateGenerator climateGenerator{{
        .bounds = settings.bounds,
        .seed = settings.seed,
        .equatorTemperature = settings.equatorTemperature,
        .poleTemperature = settings.poleTemperature,
        .vegetationCoefficient = settings.vegetationCoefficient,
        .humidityCoefficient = settings.humidityCoefficient,
        .temperatureNoiseStrength = settings.temperatureNoiseStrength,
        .temperatureNoiseFrequency = settings.temperatureNoiseFrequency,
        .temperatureElevationCooling = settings.temperatureElevationCooling,
        .temperatureHumidityInfluence = settings.temperatureHumidityInfluence,
        .temperatureLatitudeExponent = settings.temperatureLatitudeExponent,
        .noiseOctaves = settings.noiseOctaves,
        .noiseFrequency = settings.noiseFrequency,
        .noiseLacunarity = settings.noiseLacunarity,
        .noisePersistence = settings.noisePersistence,
    }};

    require(world.landClimates().size() == repeated.landClimates().size(),
            "Repeated generation changed the land climate count.");
    std::size_t expectedLandClimate = 0;
    auto waterRegionCount = std::size_t{0};
    for (const auto &region : world.regions()) {
        const auto &repeatedRegion = repeated.regions().at(region.id());
        require(repeatedRegion.landClimateId() == region.landClimateId(),
                "Region-to-land-climate mapping is not deterministic.");
        if (region.isWater()) {
            require(!region.hasLandClimate()
                        && region.landClimateId() == INVALID_LAND_CLIMATE_ID,
                    "A water region references a land climate sample.");
            require(!region.hasLandType(),
                    "A water region has a land type.");
            require(!region.hasLandform(),
                    "A water region has a landform.");
            ++waterRegionCount;
            continue;
        }

        require(region.hasLandClimate(),
                "A land region does not reference a climate sample.");
        require(region.hasLandType(),
                "A land region does not have a land type.");
        require(region.hasLandform(),
                "A land region does not have a landform.");
        require(region.landClimateId() == expectedLandClimate,
                "Land climate IDs are not compact and ordered by region ID.");
        const auto &cell = world.division().cells.at(region.cell());
        auto expected = climateGenerator.sample(cell.sitePosition);
        expected.temperature = climateGenerator.temperature(
            cell.sitePosition,
            normalizedLandElevation(region, maximumRelief),
            expected.humidity);
        const auto &actual = world.landClimates().at(region.landClimateId());
        const auto &repeatedClimate = repeated.landClimates().at(
            repeatedRegion.landClimateId());
        requireNear(actual.temperature,
                    expected.temperature,
                    "A land climate stores an incorrect temperature.");
        requireNear(actual.humidity,
                    expected.humidity,
                    "A land climate stores incorrect humidity.");
        requireNear(actual.vegetation,
                    expected.vegetation,
                    "A land climate stores incorrect vegetation.");
        require(actual.temperature == repeatedClimate.temperature
                    && actual.humidity == repeatedClimate.humidity
                    && actual.vegetation == repeatedClimate.vegetation,
                "Land climate values are not deterministic.");
        require(region.landType()
                    == classifyLandType(normalizedLandElevation(region,
                                                                 maximumRelief),
                                        actual,
                                        settings.landTypeConditions),
                "A region stores an incorrect land type.");
        require(region.landType() == repeatedRegion.landType(),
                "Land types are not deterministic.");
        require(region.landform()
                    == classifyLandform(normalizedLandElevation(region,
                                                                 maximumRelief),
                                        settings.landformConditions),
                "A region stores an incorrect landform.");
        require(region.landform() == repeatedRegion.landform(),
                "Landforms are not deterministic.");
        ++expectedLandClimate;
    }
    require(expectedLandClimate == world.landClimates().size(),
            "The land climate array is not dense.");
    require(expectedLandClimate > 0 && waterRegionCount > 0,
            "The compact climate fixture needs both land and water regions.");
}

void testDefaultLandTypeDiversity() {
    const WorldGenerationSettings settings{
        .bounds = {{0.0, 0.0}, {2048.0, 2048.0}},
    };
    const auto world = WorldGenerator{settings}.generate();
    std::array<std::size_t, 10> counts{};
    std::array<std::size_t, 3> landformCounts{};
    for (const auto &region : world.regions()) {
        if (!region.isLand())
            continue;
        ++counts[static_cast<std::size_t>(region.landType())];
        ++landformCounts[static_cast<std::size_t>(region.landform())];
        if (region.landType() != LandType::Desert)
            continue;

        const auto &sample = world.landClimates().at(region.landClimateId());
        require(sample.temperature
                    >= settings.landTypeConditions.hotTemperature
                    && (sample.humidity
                            <= settings.landTypeConditions.dryHumidity
                        || sample.vegetation
                               <= settings.landTypeConditions.sparseVegetation),
                "A desert violates its final climate conditions.");
    }

    const auto landCount = world.landClimates().size();
    require(landformCounts[static_cast<std::size_t>(Landform::Mountain)] > 0,
            "Default actual-relief normalization produced no mountains.");
    require(landformCounts[static_cast<std::size_t>(Landform::Hill)] > 0,
            "Default actual-relief normalization produced no hills.");
    require(counts[static_cast<std::size_t>(LandType::Grassland)] * 4
                < landCount * 3,
            "Grassland occupies at least three quarters of default land.");
    require(counts[static_cast<std::size_t>(LandType::Desert)] > 0,
            "The default world generated no deserts.");
    require(counts[static_cast<std::size_t>(LandType::Savanna)] > 0,
            "The default world generated no savannas.");
    require(std::ranges::count_if(counts,
                                  [](std::size_t count) {
                                      return count > 0;
                                  })
                >= 7,
            "The default world generated fewer than seven land types.");
}

void testAridClimateProducesDeserts() {
    const WorldGenerationSettings settings{
        .bounds = {{0.0, 0.0}, {2048.0, 2048.0}},
        .seed = 41,
        .columns = 48,
        .rows = 48,
        .humidityCoefficient = 0.4,
        .oceanHumidityCoefficient = 0.0,
        .riverSourceCount = 0,
        .provinceMinimumRegionCount = 1,
    };
    const auto world = WorldGenerator{settings}.generate();
    auto hotRegionCount = std::size_t{0};
    auto desertCount = std::size_t{0};
    auto savannaCount = std::size_t{0};
    for (const auto &region : world.regions()) {
        if (!region.isLand())
            continue;

        const auto &sample = world.landClimates().at(region.landClimateId());
        if (sample.temperature < settings.landTypeConditions.hotTemperature)
            continue;
        ++hotRegionCount;
        desertCount += region.landType() == LandType::Desert;
        savannaCount += region.landType() == LandType::Savanna;
    }

    require(hotRegionCount > 0,
            "The arid-climate fixture generated no tropical land.");
    require(desertCount == hotRegionCount,
            "Dry tropical land collapsed into a non-desert biome.");
    require(savannaCount == 0,
            "Dry tropical land incorrectly collapsed into savanna.");
}

void testLatitudeBiomeBands() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {2048.0, 2048.0}},
        .seed = 17,
        .columns = 64,
        .rows = 64,
        .seaLevel = 0.3,
        .edgeStrength = 0.3,
        .temperatureNoiseStrength = 0.0,
        .temperatureElevationCooling = 0.0,
        .temperatureHumidityInfluence = 0.0,
        .riverSourceCount = 0,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto temperatureBand = [&](const Region &region) {
        const auto temperature = world.landClimates()
                                     .at(region.landClimateId())
                                     .temperature;
        if (temperature <= settings.landTypeConditions.polarTemperature)
            return 0;
        if (temperature <= settings.landTypeConditions.coldTemperature)
            return 1;
        if (temperature < settings.landTypeConditions.hotTemperature)
            return 2;
        return 3;
    };

    std::array<std::size_t, 4> bandCounts{};
    auto adjacentLandPairs = std::size_t{0};
    auto sameBandPairs = std::size_t{0};
    for (const auto &region : world.regions()) {
        if (!region.isLand())
            continue;
        const auto band = temperatureBand(region);
        ++bandCounts[static_cast<std::size_t>(band)];
        const auto type = region.landType();
        if (band == 0) {
            require(type == LandType::Tundra,
                    "A polar region escaped the tundra band.");
        } else if (band == 1) {
            require(type == LandType::Tundra
                        || type == LandType::BorealForest,
                    "A cool region escaped the tundra/boreal band.");
        } else if (band == 2) {
            require(type == LandType::Grassland
                        || type == LandType::TemperateForest
                        || type == LandType::Steppe
                        || type == LandType::Wetland,
                    "A temperate region escaped the temperate biome band.");
        } else {
            require(type == LandType::Desert
                        || type == LandType::Savanna
                        || type == LandType::TropicalForest
                        || type == LandType::Rainforest,
                    "A hot region escaped the tropical biome band.");
        }

        for (const auto neighbor : world.division().cells[region.cell()].neighbors) {
            if (neighbor <= region.cell() || !world.regions()[neighbor].isLand())
                continue;
            ++adjacentLandPairs;
            sameBandPairs += temperatureBand(world.regions()[neighbor]) == band;
        }
    }
    require(std::ranges::none_of(bandCounts,
                                 [](std::size_t count) { return count == 0; }),
            "The latitude fixture did not produce every temperature band.");
    require(adjacentLandPairs > 0
                && sameBandPairs * 5 >= adjacentLandPairs * 4,
            "Latitude temperature bands are not spatially grouped.");
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
    requireNear(settings.temperatureNoiseStrength,
                8.0,
                "The default temperature noise strength must be eight.");
    requireNear(settings.temperatureNoiseFrequency,
                0.003,
                "The default temperature noise frequency must be 0.003.");
    requireNear(settings.temperatureElevationCooling,
                20.0,
                "The default elevation cooling must be 20.");
    requireNear(settings.temperatureHumidityInfluence,
                4.0,
                "The default temperature humidity influence must be four.");
    requireNear(settings.temperatureLatitudeExponent,
                1.0,
                "The default temperature latitude exponent must be one.");
    requireNear(settings.oceanHumidityCoefficient,
                0.2,
                "The default ocean humidity coefficient must be 0.2.");
    requireNear(settings.oceanHumidityDistanceRatio,
                0.12,
                "The default ocean humidity distance ratio must be 0.12.");

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

    settings.humidityCoefficient = 1.0;
    settings.temperatureNoiseStrength = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative temperature noise strength was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.temperatureNoiseStrength = 8.0;
    settings.temperatureNoiseFrequency = 0.0;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A zero temperature noise frequency was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.temperatureNoiseFrequency = 0.003;
    settings.temperatureElevationCooling = 100.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "Elevation cooling above 100 was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.temperatureElevationCooling = 20.0;
    settings.temperatureHumidityInfluence =
        std::numeric_limits<double>::infinity();
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "Non-finite temperature humidity influence was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.temperatureHumidityInfluence = 4.0;
    settings.temperatureLatitudeExponent = 0.0;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A zero temperature latitude exponent was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.temperatureLatitudeExponent = 1.0;
    settings.oceanHumidityCoefficient = 1.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "An ocean humidity coefficient above one was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.oceanHumidityCoefficient = 0.2;
    settings.oceanHumidityDistanceRatio = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative ocean humidity distance was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.oceanHumidityDistanceRatio = 0.12;
    settings.landTypeConditions.dryHumidity =
        settings.landTypeConditions.wetHumidity;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "Overlapping land type humidity conditions were accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.landTypeConditions = {};
    settings.landTypeConditions.polarTemperature =
        settings.landTypeConditions.coldTemperature;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "Overlapping land type temperature bands were accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.landTypeConditions = {};
    settings.landformConditions.hillElevation =
        settings.landformConditions.mountainElevation;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "Overlapping landform elevations were accepted.");
    } catch (const std::invalid_argument &) {
    }
}

void testOceanHumidity() {
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
        .humidityCoefficient = 0.0,
        .oceanHumidityCoefficient = 0.4,
        .oceanHumidityDistanceRatio = 0.2,
        .riverSourceCount = 0,
        .provinceMinimumRegionCount = 1,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto repeated = WorldGenerator{settings}.generate();
    auto disabledSettings = settings;
    disabledSettings.oceanHumidityCoefficient = 0.0;
    const auto disabled = WorldGenerator{disabledSettings}.generate();

    auto boostedLandCount = std::size_t{0};
    auto coastalLandCount = std::size_t{0};
    auto decayedLandCount = std::size_t{0};
    auto cooledLandCount = std::size_t{0};
    for (const auto &region : world.regions()) {
        if (!region.isLand())
            continue;

        const auto &sample = world.landClimates().at(region.landClimateId());
        const auto &repeatedSample = repeated.landClimates().at(
            repeated.regions().at(region.id()).landClimateId());
        const auto &disabledSample = disabled.landClimates().at(
            disabled.regions().at(region.id()).landClimateId());
        requireNear(disabledSample.humidity,
                    0.0,
                    "Disabling ocean humidity did not preserve base humidity.");
        requireNear(sample.vegetation,
                    disabledSample.vegetation,
                    "Ocean influence changed vegetation.");
        require(sample.humidity >= 0.0
                    && sample.humidity <= settings.oceanHumidityCoefficient,
                "Ocean humidity is outside its configured contribution range.");
        require(sample.humidity == repeatedSample.humidity,
                "Ocean humidity is not deterministic.");
        require(sample.temperature == repeatedSample.temperature,
                "Ocean-adjusted temperature is not deterministic.");
        require(sample.temperature <= disabledSample.temperature + tolerance,
                "Ocean humidity warmed a land region.");

        if (sample.humidity <= 0.0)
            continue;
        ++boostedLandCount;
        if (sample.temperature < disabledSample.temperature - tolerance)
            ++cooledLandCount;
        if (std::abs(sample.humidity
                     - settings.oceanHumidityCoefficient) <= tolerance) {
            ++coastalLandCount;
        } else {
            ++decayedLandCount;
        }
    }
    require(boostedLandCount > 0,
            "Ocean did not increase humidity in any land region.");
    require(coastalLandCount > 0,
            "Ocean-adjacent land did not receive the full humidity contribution.");
    require(decayedLandCount > 0,
            "Ocean humidity did not decay into inland regions.");
    require(cooledLandCount > 0,
            "Ocean humidity did not affect final land temperature.");
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
        .provinceLandTypeContribution = 0.0,
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
    settings.provinceSeedMinimumDistance = 1000.0;
    const auto freeWorld = WorldGenerator{settings}.generate();
    require(freeWorld.provinces().size() == 1,
            "Free claims did not combine a connected world into one province.");
    requireProvinceGrowth(freeWorld, settings);

    settings.provinceMinimumRegionCount = 1;
    settings.provinceMaximumRegionCount = 3;
    const auto cappedWorld = WorldGenerator{settings}.generate();
    const auto repeatedCappedWorld = WorldGenerator{settings}.generate();
    require(cappedWorld.provinces().size() > 1,
            "The maximum region count did not split a free connected world.");
    requireProvinceGrowth(cappedWorld, settings, &repeatedCappedWorld);
    require(std::ranges::all_of(
                cappedWorld.provinces(),
                [](const Province &province) {
                    return province.regionIds().size() <= 3;
                }),
            "Province growth exceeded the maximum region count.");

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

void testProvinceFarthestPointSeedsAndConcurrentGrowth() {
    const auto generateLine = [](std::size_t regionCount,
                                 double seedMinimumDistance,
                                 double elevationContribution = 0.0,
                                 std::size_t elevationSplit = 0,
                                 std::uint64_t generationSeed = 2) {
        const BoundingBox bounds{
            {0.0, 0.0},
            {static_cast<double>(regionCount), 1.0},
        };
        WorldDivision division;
        division.cells.reserve(regionCount);
        for (CellId id = 0; id < regionCount; ++id) {
            std::vector<CellId> neighbors;
            if (id > 0)
                neighbors.push_back(id - 1);
            if (static_cast<std::size_t>(id) + 1 < regionCount)
                neighbors.push_back(id + 1);
            division.cells.push_back({
                .id = id,
                .sitePosition = {static_cast<double>(id) + 0.5, 0.5},
                .neighbors = std::move(neighbors),
            });
        }

        std::vector<Region> regions;
        regions.reserve(regionCount);
        for (CellId id = 0; id < regionCount; ++id) {
            regions.emplace_back(
                id,
                static_cast<std::size_t>(id) >= elevationSplit ? 1.0 : 0.0,
                0.0,
                0,
                static_cast<LandClimateId>(id));
        }
        return generateProvinces(bounds,
                                 division,
                                 regions,
                                 100.0,
                                 0.0,
                                 elevationContribution,
                                 0.0,
                                 1.0,
                                 1,
                                 0.0,
                                 0.0,
                                 0,
                                 seedMinimumDistance,
                                 generationSeed);
    };

    const auto sparse = generateLine(9, 100.0);
    require(sparse.size() == 1,
            "A seed spacing larger than a land component created extra seeds.");
    const auto otherWorldSeed = generateLine(9, 100.0, 0.0, 0, 3);
    require(otherWorldSeed.front().seedRegion()
                != sparse.front().seedRegion(),
            "Province seed selection did not respond to the world seed.");

    const auto distributed = generateLine(9, 2.5);
    require(distributed.size() == 3,
            "Farthest-point sampling did not cover a line with three seeds.");
    std::vector<RegionId> distributedSeeds;
    for (const auto &province : distributed)
        distributedSeeds.push_back(province.seedRegion());
    std::ranges::sort(distributedSeeds);
    require(distributedSeeds == std::vector<RegionId>{0, 4, 8},
            "Farthest-point sampling did not choose the expected separated seeds.");
    for (RegionId region = 0; region < 9; ++region) {
        const auto closestSeed = std::ranges::min(
            distributedSeeds
            | std::views::transform([&](RegionId seed) {
                  return std::abs(static_cast<int>(seed)
                                  - static_cast<int>(region));
              }));
        require(closestSeed <= 2,
                "Farthest-point seeds did not cover every line region.");
    }

    const auto flat = generateLine(6, 5.0, 0.0, 3);
    const auto dividedByRelief = generateLine(6, 5.0, 10.0, 3);
    require(flat.size() == 1,
            "A flat line unexpectedly crossed its seed-distance threshold.");
    require(dividedByRelief.size() == 2,
            "A steep terrain barrier did not create seeds on both sides.");

    const auto concurrent = generateLine(5, 3.0);
    require(concurrent.size() == 2,
            "The concurrent-growth fixture did not create two provinces.");
    require(concurrent[0].regionIds()
                == std::vector<RegionId>{0, 1, 2}
                && concurrent[1].regionIds()
                       == std::vector<RegionId>{4, 3},
            "Province fronts did not grow concurrently by accumulated distance.");

    const BoundingBox islandBounds{{0.0, 0.0}, {5.0, 1.0}};
    const WorldDivision islandDivision{
        .cells = {
            {.id = 0, .sitePosition = {0.5, 0.5}, .neighbors = {1}},
            {.id = 1, .sitePosition = {1.5, 0.5}, .neighbors = {0, 2}},
            {.id = 2, .sitePosition = {2.5, 0.5}, .neighbors = {1, 3}},
            {.id = 3, .sitePosition = {3.5, 0.5}, .neighbors = {2, 4}},
            {.id = 4, .sitePosition = {4.5, 0.5}, .neighbors = {3}},
        },
    };
    std::vector<Region> islandRegions;
    islandRegions.emplace_back(0, 1.0, 0.0, 0, 0);
    islandRegions.emplace_back(1, 1.0, 0.0, 0, 1);
    islandRegions.emplace_back(2, 0.0, 1.0, 0, INVALID_LAND_CLIMATE_ID);
    islandRegions.emplace_back(3, 1.0, 0.0, 0, 2);
    islandRegions.emplace_back(4, 1.0, 0.0, 0, 3);
    const auto islandProvinces = generateProvinces(islandBounds,
                                                   islandDivision,
                                                   islandRegions,
                                                   100.0,
                                                   0.0,
                                                   0.0,
                                                   0.0,
                                                   1.0,
                                                   1,
                                                   0.0,
                                                   0.0,
                                                   0,
                                                   100.0,
                                                   2);
    require(islandProvinces.size() == 2,
            "Disconnected land components did not each receive a seed.");
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
        regions.emplace_back(0, 0.0, 0.0, 0, 0);
        regions.emplace_back(1, firstCost, 0.0, 0, 1);
        regions.emplace_back(2, secondCost, 0.0, 0, 2);
        return generateProvinces(bounds,
                                 division,
                                 regions,
                                 10.0 * EPS,
                                 0.0,
                                 1.0,
                                 0.0,
                                 0.0,
                                 1,
                                 0.0,
                                 0.0,
                                 0,
                                 1000.0,
                                 2);
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
                                 static_cast<LandClimateId>(cell));
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
                                 shortBorderContribution,
                                 0.0,
                                 0,
                                 1000.0,
                                 2);
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

void testProvinceLandTypePenalty() {
    const BoundingBox bounds{{0.0, 0.0}, {10.0, 10.0}};
    const WorldDivision division{
        .cells = {
            {.id = 0, .sitePosition = {5.0, 5.0}, .neighbors = {1, 2}},
            {.id = 1, .sitePosition = {4.0, 5.0}, .neighbors = {0, 3}},
            {.id = 2, .sitePosition = {6.0, 5.0}, .neighbors = {0}},
            {.id = 3, .sitePosition = {3.0, 5.0}, .neighbors = {1}},
        },
    };
    const auto generate = [&](double landTypeContribution) {
        std::vector<Region> regions;
        for (CellId cell = 0; cell < division.cells.size(); ++cell) {
            regions.emplace_back(cell,
                                 0.0,
                                 0.0,
                                 0,
                                 static_cast<LandClimateId>(cell));
            regions.back().setLandType(cell == 1
                                           ? LandType::Desert
                                           : LandType::Grassland);
        }
        return generateProvinces(bounds,
                                 division,
                                 regions,
                                 1.0,
                                 0.0,
                                 0.0,
                                 0.0,
                                 0.0,
                                 1,
                                 0.0,
                                 landTypeContribution,
                                 0,
                                 1000.0,
                                 2);
    };

    const auto disabled = generate(0.0);
    require(disabled.size() == 1
                && disabled.front().regionIds()
                       == std::vector<RegionId>{0, 1, 2, 3},
            "Disabled land-type cost changed region-ID claim ordering.");

    const auto enabled = generate(1.0);
    require(enabled.size() == 1,
            "The land-type penalty unexpectedly split an affordable province.");
    require(enabled.front().regionIds()
                == std::vector<RegionId>{0, 2, 1, 3},
            "Province growth did not penalize land types differing from its seed.");
    requireNear(enabled.front().remainingScore(),
                0.0,
                "Land-type penalties were not charged relative to the province seed.");
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
                                  static_cast<LandClimateId>(cell));
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
                == std::vector<RegionId>{2, 3, 4, 1, 0},
            "A small-province region did not join its neighboring province.");
    require(splitProvinces[1].regionIds()
                == std::vector<RegionId>{7, 6, 5},
            "Small-province regions were not reassigned independently.");
    for (RegionId region = 0; region < splitRegions.size(); ++region) {
        const auto expectedProvince = region >= 5 ? ProvinceId{1}
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
        allSmallRegions.emplace_back(cell, 0.0, 0.0, 0, cell);
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
                == std::vector<RegionId>{2, 1, 3, 0, 4},
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
    isolatedRegions.emplace_back(0, 0.0, 0.0, 0, 0);
    isolatedRegions.emplace_back(1, 0.0, 0.0, 0, 1);
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

void testSmallProvinceCheapestReassignment() {
    const BoundingBox bounds{{0.0, 0.0}, {10.0, 10.0}};
    const WorldDivision division{
        .cells = {
            {.id = 0, .sitePosition = {0.0, 0.0}, .neighbors = {1}},
            {.id = 1, .sitePosition = {1.0, 0.0}, .neighbors = {0, 2}},
            {.id = 2, .sitePosition = {2.0, 0.0}, .neighbors = {1, 3}},
            {.id = 3, .sitePosition = {3.0, 0.0}, .neighbors = {2, 4}},
            {.id = 4, .sitePosition = {4.0, 0.0}, .neighbors = {3, 5}},
            {.id = 5, .sitePosition = {5.0, 0.0}, .neighbors = {4, 6}},
            {.id = 6, .sitePosition = {6.0, 0.0}, .neighbors = {5}},
        },
    };
    std::vector<Region> regions;
    for (CellId cell = 0; cell < division.cells.size(); ++cell) {
        const auto elevation = cell <= 2 ? 0.0
                             : cell == 3 ? 0.8
                                         : 1.0;
        regions.emplace_back(cell,
                             elevation,
                             0.0,
                             0,
                             static_cast<LandClimateId>(cell));
    }

    const auto provinces = generateProvinces(bounds,
                                             division,
                                             regions,
                                             2.0,
                                             0.0,
                                             10.0,
                                             0.0,
                                             1.0,
                                             3,
                                             0.0);
    require(provinces.size() == 2,
            "The cheapest-reassignment fixture did not remove its small province.");
    require(provinces[0].regionIds()
                == std::vector<RegionId>{2, 1, 0},
            "An orphaned region joined a more expensive lower-ID province.");
    require(provinces[1].regionIds()
                == std::vector<RegionId>{6, 5, 4, 3},
            "An orphaned region did not join its cheapest neighboring province.");
    require(regions[3].provinceId() == 1,
            "Cheapest reassignment did not update the region's compacted province ID.");
}

void testProvinceCapacitySeedSelection() {
    const BoundingBox bounds{{0.0, 0.0}, {10.0, 10.0}};
    const auto generate = [&](std::size_t regionCount) {
        WorldDivision division;
        division.cells.reserve(regionCount);
        for (CellId id = 0; id < regionCount; ++id) {
            std::vector<CellId> neighbors;
            if (id > 0)
                neighbors.push_back(id - 1);
            if (static_cast<std::size_t>(id) + 1 < regionCount)
                neighbors.push_back(id + 1);
            division.cells.push_back({
                .id = id,
                .sitePosition = {static_cast<double>(id), 0.0},
                .neighbors = std::move(neighbors),
            });
        }

        std::vector<Region> regions;
        regions.reserve(regionCount);
        for (CellId id = 0; id < regionCount; ++id) {
            regions.emplace_back(id,
                                 id <= 3 ? 0.0 : 1.0,
                                 0.0,
                                 0,
                                 static_cast<LandClimateId>(id));
        }
        auto provinces = generateProvinces(bounds,
                                           division,
                                           regions,
                                           2.0,
                                           0.0,
                                           10.0,
                                           0.0,
                                           1.0,
                                           2,
                                           0.0,
                                           0.0,
                                           3);
        return std::pair{std::move(provinces), std::move(regions)};
    };

    const auto [provinces, regions] = generate(6);
    require(provinces.size() == 2,
            "Capacity-aware seed selection did not create two provinces for six regions.");
    require(std::ranges::all_of(regions,
                                [](const Region &region) {
                                    return region.hasProvince();
                                }),
            "Capacity-aware seed selection left a land region unassigned.");

    const auto [fullProvinces, fullRegions] = generate(7);
    require(fullProvinces.size() == 3,
            "Capacity-aware seed selection did not round up the province count.");
    require(std::ranges::all_of(fullRegions,
                                [](const Region &region) {
                                    return region.hasProvince();
                                }),
            "Capacity-aware seed selection lost a land region.");
}

void testCoastalSmallProvinceMerging() {
    const BoundingBox bounds{{0.0, 0.0}, {10.0, 10.0}};
    const WorldDivision division{
        .cells = {
            {.id = 0, .sitePosition = {1.0, 5.0}, .neighbors = {1}},
            {.id = 1, .sitePosition = {3.0, 5.0}, .neighbors = {0, 2, 3}},
            {.id = 2, .sitePosition = {5.0, 5.0}, .neighbors = {1}},
            {.id = 3, .sitePosition = {3.0, 3.0}, .neighbors = {1}},
        },
    };
    std::vector<Region> regions;
    regions.emplace_back(0, 1.0, 0.0, 0, 0);
    regions.emplace_back(1, 1.0, 0.0, 0, 1);
    regions.emplace_back(2, 1.0, 0.0, 0, 2);
    regions.emplace_back(3, 0.0, 1.0, 0, INVALID_LAND_CLIMATE_ID);

    const auto provinces = generateProvinces(bounds,
                                             division,
                                             regions,
                                             0.0,
                                             0.0,
                                             0.0,
                                             0.0,
                                             1.0,
                                             3,
                                             0.0);
    require(provinces.size() == 1,
            "Coastal small provinces did not merge into their land anchor.");
    auto coastalRegionIds = provinces.front().regionIds();
    std::ranges::sort(coastalRegionIds);
    require(coastalRegionIds == std::vector<RegionId>{0, 1, 2},
            "Coastal province merging included water or lost a land region.");
    require(std::ranges::all_of(regions.begin(),
                                regions.begin() + 3,
                                [](const Region &region) {
                                    return region.provinceId() == 0;
                                }),
            "Coastal land regions did not receive the merged province ID.");
    require(regions[3].isWater() && !regions[3].hasProvince(),
            "Coastal province merging assigned a water region.");
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
    requireNear(settings.provinceLandTypeContribution,
                5.0,
                "The default province land-type contribution must be 5.");
    requireNear(settings.provinceShortBorderContribution,
                5.0,
                "The default province short-border contribution must be 5.");
    requireNear(settings.provinceBaseCost,
                1.0,
                "The default province base cost must be 1.");
    requireNear(settings.provinceSeedMinimumDistance,
                4.0,
                "The default province seed minimum distance must be 4.");
    require(settings.provinceMinimumRegionCount == 3,
            "The default minimum province region count must be 3.");
    require(settings.provinceMaximumRegionCount == 0,
            "The default maximum province region count must be unlimited.");

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
    settings.provinceLandTypeContribution = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province land-type contribution was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceLandTypeContribution = 5.0;
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
    settings.provinceMaximumRegionCount = 2;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A maximum province size below the minimum was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceMaximumRegionCount = 0;
    settings.provinceShortBorderContribution = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative province short-border contribution was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.provinceShortBorderContribution = 5.0;
    settings.provinceSeedMinimumDistance = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false,
                "A negative province seed minimum distance was accepted.");
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
        .vegetationCoefficient = 0.0,
        .humidityCoefficient = 0.0,
        .oceanHumidityCoefficient = 0.0,
        .riverSourceCount = 24,
        .riverMinimumSourceElevation = 0.5,
        .riverRandomness = 0.35,
        .riverElevationTolerance = 0.03,
        .riverHumidityCoefficient = 0.08,
        .riverVegetationCoefficient = 0.04,
        .provinceShortBorderContribution = 0.0,
        .provinceMinimumRegionCount = 1,
    };
    const auto world = WorldGenerator{settings}.generate();
    const auto repeated = WorldGenerator{settings}.generate();
    const auto maximumRelief = maximumLandRelief(world);
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

    const climate::ClimateGenerator climateGenerator{{
        .bounds = settings.bounds,
        .seed = settings.seed,
        .equatorTemperature = settings.equatorTemperature,
        .poleTemperature = settings.poleTemperature,
        .vegetationCoefficient = settings.vegetationCoefficient,
        .humidityCoefficient = settings.humidityCoefficient,
        .temperatureNoiseStrength = settings.temperatureNoiseStrength,
        .temperatureNoiseFrequency = settings.temperatureNoiseFrequency,
        .temperatureElevationCooling = settings.temperatureElevationCooling,
        .temperatureHumidityInfluence = settings.temperatureHumidityInfluence,
        .temperatureLatitudeExponent = settings.temperatureLatitudeExponent,
        .noiseOctaves = settings.noiseOctaves,
        .noiseFrequency = settings.noiseFrequency,
        .noiseLacunarity = settings.noiseLacunarity,
        .noisePersistence = settings.noisePersistence,
    }};
    auto riverClimateRegionCount = std::size_t{0};
    auto dryLandRegionCount = std::size_t{0};
    for (const auto &region : world.regions()) {
        for (const auto river : region.edgeRivers()) {
            require(river == INVALID_RIVER_ID || river < world.rivers().size(),
                    "A region edge references an invalid river segment.");
        }
        if (!region.isLand())
            continue;

        auto strongestRiver = 0.0;
        for (const auto river : region.edgeRivers()) {
            if (river == INVALID_RIVER_ID)
                continue;
            strongestRiver = std::max(
                strongestRiver,
                world.rivers().at(river).nodes.front().strength);
        }

        const auto base = climateGenerator.sample(
            world.division().cells.at(region.cell()).sitePosition);
        const auto &actual = world.landClimates().at(region.landClimateId());
        const auto expectedTemperature = climateGenerator.temperature(
            world.division().cells.at(region.cell()).sitePosition,
            normalizedLandElevation(region, maximumRelief),
            actual.humidity);
        requireNear(actual.temperature,
                    expectedTemperature,
                    "River-adjusted humidity did not affect final temperature.");
        requireNear(actual.humidity,
                    std::clamp(base.humidity
                                   + strongestRiver
                                         * settings.riverHumidityCoefficient,
                               0.0,
                               1.0),
                    "River strength did not produce the expected humidity boost.");
        requireNear(actual.vegetation,
                    std::clamp(base.vegetation
                                   + strongestRiver
                                         * settings.riverVegetationCoefficient,
                               0.0,
                               1.0),
                    "River strength did not produce the expected vegetation boost.");

        const auto &repeatedClimate = repeated.landClimates().at(
            repeated.regions().at(region.id()).landClimateId());
        require(actual.temperature == repeatedClimate.temperature
                    && actual.humidity == repeatedClimate.humidity
                    && actual.vegetation == repeatedClimate.vegetation,
                "River climate influence is not deterministic.");
        require(region.hasLandType()
                    && region.landType()
                           == classifyLandType(normalizedLandElevation(
                                                   region,
                                                   maximumRelief),
                                               actual,
                                               settings.landTypeConditions),
                "Land type does not use the river-adjusted climate.");
        if (strongestRiver > 0.0)
            ++riverClimateRegionCount;
        else
            ++dryLandRegionCount;
    }
    require(riverClimateRegionCount > 0,
            "The river fixture did not boost any land-region climate.");
    require(dryLandRegionCount > 0,
            "The river fixture has no land region away from a river.");

    require(requireProvinceGrowth(world, settings, &repeated),
            "The province fixture did not exercise a river frontier cost.");

    auto disabledSettings = settings;
    disabledSettings.riverSourceCount = 0;
    require(WorldGenerator{disabledSettings}.generate().rivers().empty(),
            "A zero river count did not disable river generation.");

    auto disabledClimateSettings = settings;
    disabledClimateSettings.riverHumidityCoefficient = 0.0;
    disabledClimateSettings.riverVegetationCoefficient = 0.0;
    const auto disabledClimateWorld = WorldGenerator{
        disabledClimateSettings}.generate();
    require(!disabledClimateWorld.rivers().empty(),
            "The zero-coefficient fixture did not retain its rivers.");
    for (const auto &sample : disabledClimateWorld.landClimates()) {
        requireNear(sample.humidity,
                    0.0,
                    "Zero did not disable river humidity influence.");
        requireNear(sample.vegetation,
                    0.0,
                    "Zero did not disable river vegetation influence.");
    }
}

void testRiverSettingsValidation() {
    auto settings = WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {10.0, 10.0}},
    };
    requireNear(settings.riverHumidityCoefficient,
                0.05,
                "The default river humidity coefficient must be 0.05.");
    requireNear(settings.riverVegetationCoefficient,
                0.05,
                "The default river vegetation coefficient must be 0.05.");

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

    settings.riverElevationTolerance = 0.03;
    settings.riverHumidityCoefficient = -0.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A negative river humidity coefficient was accepted.");
    } catch (const std::invalid_argument &) {
    }

    settings.riverHumidityCoefficient = 0.05;
    settings.riverVegetationCoefficient = 1.01;
    try {
        static_cast<void>(WorldGenerator{settings});
        require(false, "A river vegetation coefficient above one was accepted.");
    } catch (const std::invalid_argument &) {
    }
}

} // namespace

int main() {
    try {
        testFractalNoise();
        testSmoothEdgeDecay();
        testLloydRelaxation();
        testEffectiveSeaLevel();
        testWaterConnectivityTypes();
        testEdgeStrengthValidation();
        testRegionClimate();
        testDefaultLandTypeDiversity();
        testAridClimateProducesDeserts();
        testLatitudeBiomeBands();
        testClimateSettingsValidation();
        testOceanHumidity();
        testProvinceBudgets();
        testProvinceFarthestPointSeedsAndConcurrentGrowth();
        testProvinceCostOrdering();
        testProvinceShortBorderPenalty();
        testProvinceLandTypePenalty();
        testSmallProvinceMerging();
        testSmallProvinceCheapestReassignment();
        testProvinceCapacitySeedSelection();
        testCoastalSmallProvinceMerging();
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
