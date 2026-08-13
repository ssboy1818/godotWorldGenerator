#include "WorldGenerator.h"

#include "ClimateGenerator.h"
#include "Fortune.h"
#include "JitteredGridSiteGenerator.h"
#include "Landform.h"
#include "LandType.h"
#include "NumericalPolicy.h"
#include "PerlinNoise.h"
#include "ProvinceGenerator.h"
#include "RiverGenerator.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace worldgen {

namespace {

void validateSettings(const WorldGenerationSettings &settings) {
    if (settings.columns == 0 || settings.rows == 0) {
        throw std::invalid_argument("A site grid must have at least one row and column.");
    }
    if (settings.columns > std::numeric_limits<std::size_t>::max() / settings.rows)
        throw std::invalid_argument("The site grid dimensions are too large.");
    if (!std::isfinite(settings.jitter)
        || settings.jitter < 0.0 || settings.jitter > 1.0) {
        throw std::invalid_argument("Grid jitter must be between zero and one.");
    }
    if (!std::isfinite(settings.seaLevel)
        || settings.seaLevel < 0.0 || settings.seaLevel > 1.0) {
        throw std::invalid_argument("Sea level must be between zero and one.");
    }
    if (!std::isfinite(settings.edgeDecayRatio.x)
        || !std::isfinite(settings.edgeDecayRatio.y)
        || settings.edgeDecayRatio.x <= 0.0
        || settings.edgeDecayRatio.y <= 0.0) {
        throw std::invalid_argument("Edge decay ratios must be positive.");
    }
    if (!std::isfinite(settings.edgeStrength)
        || settings.edgeStrength < 0.0 || settings.edgeStrength > 1.0) {
        throw std::invalid_argument("Edge strength must be between zero and one.");
    }
    if (settings.noiseOctaves == 0)
        throw std::invalid_argument("Noise must have at least one octave.");
    if (!std::isfinite(settings.noiseFrequency) || settings.noiseFrequency <= 0.0)
        throw std::invalid_argument("Noise frequency must be positive.");
    if (!std::isfinite(settings.noiseLacunarity) || settings.noiseLacunarity <= 0.0)
        throw std::invalid_argument("Noise lacunarity must be positive.");
    if (!std::isfinite(settings.noisePersistence) || settings.noisePersistence < 0.0)
        throw std::invalid_argument("Noise persistence must be non-negative.");
    const auto validTemperature = [](double value) {
        return std::isfinite(value) && value >= -50.0 && value <= 50.0;
    };
    if (!validTemperature(settings.equatorTemperature)
        || !validTemperature(settings.poleTemperature)) {
        throw std::invalid_argument(
            "Equator and pole temperatures must be between -50 and 50.");
    }
    if (settings.poleTemperature > settings.equatorTemperature) {
        throw std::invalid_argument(
            "Pole temperature cannot exceed equator temperature.");
    }
    const auto validClimateCoefficient = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 2.0;
    };
    if (!validClimateCoefficient(settings.vegetationCoefficient)) {
        throw std::invalid_argument(
            "Vegetation coefficient must be between zero and two.");
    }
    if (!validClimateCoefficient(settings.humidityCoefficient)) {
        throw std::invalid_argument(
            "Humidity coefficient must be between zero and two.");
    }
    const auto validTemperatureModifier = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 100.0;
    };
    if (!validTemperatureModifier(settings.temperatureNoiseStrength)) {
        throw std::invalid_argument(
            "Temperature noise strength must be between zero and 100.");
    }
    if (!std::isfinite(settings.temperatureNoiseFrequency)
        || settings.temperatureNoiseFrequency <= 0.0) {
        throw std::invalid_argument(
            "Temperature noise frequency must be positive.");
    }
    if (!validTemperatureModifier(settings.temperatureElevationCooling)) {
        throw std::invalid_argument(
            "Temperature elevation cooling must be between zero and 100.");
    }
    if (!validTemperatureModifier(settings.temperatureHumidityInfluence)) {
        throw std::invalid_argument(
            "Temperature humidity influence must be between zero and 100.");
    }
    if (!std::isfinite(settings.temperatureLatitudeExponent)
        || settings.temperatureLatitudeExponent <= 0.0
        || settings.temperatureLatitudeExponent > 10.0) {
        throw std::invalid_argument(
            "Temperature latitude exponent must be between zero and ten.");
    }
    const auto validOceanHumiditySetting = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    if (!validOceanHumiditySetting(settings.oceanHumidityCoefficient)) {
        throw std::invalid_argument(
            "Ocean humidity coefficient must be between zero and one.");
    }
    if (!validOceanHumiditySetting(settings.oceanHumidityDistanceRatio)) {
        throw std::invalid_argument(
            "Ocean humidity distance ratio must be between zero and one.");
    }
    validateLandTypeConditions(settings.landTypeConditions);
    validateLandformConditions(settings.landformConditions);
    if (!std::isfinite(settings.riverMinimumSourceElevation)
        || settings.riverMinimumSourceElevation < 0.0
        || settings.riverMinimumSourceElevation > 1.0) {
        throw std::invalid_argument(
            "Minimum river source elevation must be between zero and one.");
    }
    if (!std::isfinite(settings.riverRandomness)
        || settings.riverRandomness < 0.0 || settings.riverRandomness > 1.0) {
        throw std::invalid_argument("River randomness must be between zero and one.");
    }
    if (!std::isfinite(settings.riverElevationTolerance)
        || settings.riverElevationTolerance < 0.0
        || settings.riverElevationTolerance > 1.0) {
        throw std::invalid_argument(
            "River elevation tolerance must be between zero and one.");
    }
    const auto validRiverClimateCoefficient = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    if (!validRiverClimateCoefficient(settings.riverHumidityCoefficient)) {
        throw std::invalid_argument(
            "River humidity coefficient must be between zero and one.");
    }
    if (!validRiverClimateCoefficient(settings.riverVegetationCoefficient)) {
        throw std::invalid_argument(
            "River vegetation coefficient must be between zero and one.");
    }
    if (!std::isfinite(settings.provinceStartScore)
        || settings.provinceStartScore < 0.0) {
        throw std::invalid_argument(
            "Province start score must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceRiverContribution)
        || settings.provinceRiverContribution < 0.0) {
        throw std::invalid_argument(
            "Province river contribution must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceElevationContribution)
        || settings.provinceElevationContribution < 0.0) {
        throw std::invalid_argument(
            "Province elevation contribution must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceDistanceContribution)
        || settings.provinceDistanceContribution < 0.0) {
        throw std::invalid_argument(
            "Province distance contribution must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceShortBorderContribution)
        || settings.provinceShortBorderContribution < 0.0) {
        throw std::invalid_argument(
            "Province short-border contribution must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceLandTypeContribution)
        || settings.provinceLandTypeContribution < 0.0) {
        throw std::invalid_argument(
            "Province land-type contribution must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceBaseCost)
        || settings.provinceBaseCost < 0.0) {
        throw std::invalid_argument(
            "Province base cost must be finite and non-negative.");
    }
    if (!std::isfinite(settings.provinceBaseCost
                       + settings.provinceRiverContribution
                       + settings.provinceElevationContribution
                       + settings.provinceDistanceContribution
                       + settings.provinceShortBorderContribution
                       + settings.provinceLandTypeContribution)) {
        throw std::invalid_argument(
            "The maximum province claim cost must be finite.");
    }
    if (settings.provinceMinimumRegionCount == 0) {
        throw std::invalid_argument(
            "The minimum province region count must be positive.");
    }
    if (settings.provinceMaximumRegionCount != 0
        && settings.provinceMaximumRegionCount
               < settings.provinceMinimumRegionCount) {
        throw std::invalid_argument(
            "The maximum province region count must be zero or at least the minimum.");
    }
}

void applyRiverClimateInfluence(
    std::span<const Region> regions,
    std::span<climate::ClimateSample> landClimates,
    std::span<const River> rivers,
    double humidityCoefficient,
    double vegetationCoefficient) {
    if (humidityCoefficient == 0.0 && vegetationCoefficient == 0.0)
        return;

    for (const auto &region : regions) {
        if (!region.isLand())
            continue;

        auto strongestRiver = 0.0;
        for (const auto riverId : region.edgeRivers()) {
            if (riverId == INVALID_RIVER_ID)
                continue;
            if (riverId >= rivers.size() || rivers[riverId].nodes.empty()) {
                throw std::logic_error(
                    "A region references an invalid river segment.");
            }
            strongestRiver = std::max(strongestRiver,
                                      rivers[riverId].nodes.front().strength);
        }
        if (strongestRiver == 0.0)
            continue;

        if (region.landClimateId() >= landClimates.size()) {
            throw std::logic_error(
                "A land region references an invalid climate sample.");
        }
        auto &sample = landClimates[region.landClimateId()];
        sample.humidity = std::clamp(
            sample.humidity + strongestRiver * humidityCoefficient,
            0.0,
            1.0);
        sample.vegetation = std::clamp(
            sample.vegetation + strongestRiver * vegetationCoefficient,
            0.0,
            1.0);
    }
}

[[nodiscard]] bool cellTouchesBoundary(const BoundingBox &boundingBox,
                                       const Cell &cell) noexcept {
    const auto tolerance = numericalToleranceFor(boundingBox).sharedEdgeLength();
    return std::ranges::any_of(cell.vertices, [&](Vector2d vertex) {
        return almostEqual(vertex.x, boundingBox.min.x, tolerance)
            || almostEqual(vertex.x, boundingBox.max.x, tolerance)
            || almostEqual(vertex.y, boundingBox.min.y, tolerance)
            || almostEqual(vertex.y, boundingBox.max.y, tolerance);
    });
}

[[nodiscard]] std::vector<bool> findSeaCells(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<const Region> regions) {
    if (division.cells.size() != regions.size()) {
        throw std::logic_error(
            "Water classification requires one region per cell.");
    }

    std::vector<bool> sea(division.cells.size(), false);
    std::queue<CellId> pending;
    for (const auto &cell : division.cells) {
        if (cell.id >= regions.size() || regions[cell.id].cell() != cell.id) {
            throw std::logic_error(
                "Water classification received invalid cell or region IDs.");
        }
        if (regions[cell.id].isWater()
            && cellTouchesBoundary(boundingBox, cell)) {
            sea[cell.id] = true;
            pending.push(cell.id);
        }
    }

    while (!pending.empty()) {
        const auto cell = pending.front();
        pending.pop();
        for (const auto neighbor : division.cells[cell].neighbors) {
            if (neighbor >= regions.size()) {
                throw std::logic_error(
                    "A sea cell references an invalid neighbor.");
            }
            if (sea[neighbor] || !regions[neighbor].isWater())
                continue;
            sea[neighbor] = true;
            pending.push(neighbor);
        }
    }
    return sea;
}

void classifyWaterRegions(const BoundingBox &boundingBox,
                          const WorldDivision &division,
                          std::span<Region> regions) {
    const auto sea = findSeaCells(boundingBox, division, regions);
    for (std::size_t cell = 0; cell < sea.size(); ++cell) {
        if (sea[cell])
            regions[cell].markAsSea();
    }
}

void applyOceanHumidityInfluence(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<const Region> regions,
    std::span<climate::ClimateSample> landClimates,
    double coefficient,
    double distanceRatio) {
    if (coefficient == 0.0 || distanceRatio == 0.0)
        return;

    const auto diagonal = (boundingBox.max - boundingBox.min).length();
    const auto maximumDistance = diagonal * distanceRatio;
    std::vector<double> distance(
        division.cells.size(),
        std::numeric_limits<double>::infinity());
    using QueueEntry = std::pair<double, CellId>;
    std::priority_queue<QueueEntry,
                        std::vector<QueueEntry>,
                        std::greater<>> pending;

    const auto seed = [&](CellId cell) {
        if (distance[cell] == 0.0)
            return;
        distance[cell] = 0.0;
        pending.emplace(0.0, cell);
    };
    for (CellId cell = 0; cell < regions.size(); ++cell) {
        if (!regions[cell].isSea())
            continue;
        seed(cell);
        for (const auto neighbor : division.cells[cell].neighbors) {
            if (neighbor >= regions.size()) {
                throw std::logic_error(
                    "A sea cell references an invalid neighbor.");
            }
            if (regions[neighbor].isLand())
                seed(neighbor);
        }
    }

    while (!pending.empty()) {
        const auto [currentDistance, cell] = pending.top();
        pending.pop();
        if (currentDistance > distance[cell])
            continue;

        for (const auto neighbor : division.cells[cell].neighbors) {
            if (neighbor >= division.cells.size()) {
                throw std::logic_error(
                    "Ocean humidity encountered an invalid neighbor.");
            }
            const auto step = (division.cells[cell].sitePosition
                               - division.cells[neighbor].sitePosition).length();
            const auto candidate = currentDistance + step;
            if (candidate >= distance[neighbor]
                || candidate > maximumDistance) {
                continue;
            }
            distance[neighbor] = candidate;
            pending.emplace(candidate, neighbor);
        }
    }

    for (const auto &region : regions) {
        if (!region.isLand() || distance[region.cell()] > maximumDistance)
            continue;
        if (region.landClimateId() >= landClimates.size()) {
            throw std::logic_error(
                "A land region references an invalid climate sample.");
        }
        const auto normalizedDistance = std::clamp(
            distance[region.cell()] / maximumDistance,
            0.0,
            1.0);
        const auto smoothDistance = normalizedDistance * normalizedDistance
                                    * (3.0 - 2.0 * normalizedDistance);
        auto &sample = landClimates[region.landClimateId()];
        sample.humidity = std::clamp(
            sample.humidity + coefficient * (1.0 - smoothDistance),
            0.0,
            1.0);
    }
}

[[nodiscard]] std::vector<double> normalizeLandElevations(
    std::span<const Region> regions,
    std::size_t landClimateCount) {
    std::vector<double> normalizedElevations(landClimateCount, 0.0);
    std::vector<bool> populated(landClimateCount, false);
    auto maximumRelief = 0.0;
    for (const auto &region : regions) {
        if (!region.isLand())
            continue;
        if (region.landClimateId() >= normalizedElevations.size()
            || populated[region.landClimateId()]) {
            throw std::logic_error(
                "Elevation normalization received invalid land climate IDs.");
        }
        const auto relief = region.elevation() - region.seaLevel();
        if (!std::isfinite(relief) || relief < 0.0) {
            throw std::logic_error(
                "Elevation normalization received invalid land relief.");
        }
        maximumRelief = std::max(maximumRelief, relief);
        populated[region.landClimateId()] = true;
    }
    if (std::ranges::find(populated, false) != populated.end()) {
        throw std::logic_error(
            "Elevation normalization did not receive every land climate.");
    }
    if (maximumRelief == 0.0)
        return normalizedElevations;

    for (const auto &region : regions) {
        if (!region.isLand())
            continue;
        normalizedElevations[region.landClimateId()] = std::clamp(
            (region.elevation() - region.seaLevel()) / maximumRelief,
            0.0,
            1.0);
    }
    return normalizedElevations;
}

void finalizeLandTemperatures(
    const WorldDivision &division,
    std::span<const Region> regions,
    std::span<climate::ClimateSample> landClimates,
    std::span<const double> normalizedElevations,
    const climate::ClimateGenerator &climateGenerator) {
    if (normalizedElevations.size() != landClimates.size()) {
        throw std::logic_error(
            "Temperature finalization received inconsistent elevation data.");
    }
    for (const auto &region : regions) {
        if (!region.isLand())
            continue;
        if (region.cell() >= division.cells.size()
            || region.landClimateId() >= landClimates.size()) {
            throw std::logic_error(
                "Temperature finalization received invalid region data.");
        }

        auto &sample = landClimates[region.landClimateId()];
        sample.temperature = climateGenerator.temperature(
            division.cells[region.cell()].sitePosition,
            normalizedElevations[region.landClimateId()],
            sample.humidity);
    }
}

void classifyLandRegions(std::span<Region> regions,
                         std::span<const climate::ClimateSample> landClimates,
                         std::span<const double> normalizedElevations,
                         const LandTypeConditions &conditions) {
    if (normalizedElevations.size() != landClimates.size()) {
        throw std::logic_error(
            "Land classification received inconsistent elevation data.");
    }
    for (auto &region : regions) {
        if (!region.isLand())
            continue;
        if (region.landClimateId() >= landClimates.size()) {
            throw std::logic_error(
                "A land region references an invalid climate sample.");
        }
        region.setLandType(classifyLandType(
            normalizedElevations[region.landClimateId()],
            landClimates[region.landClimateId()],
            conditions));
    }
}

void classifyLandforms(std::span<Region> regions,
                       std::span<const double> normalizedElevations,
                       const LandformConditions &conditions) {
    for (auto &region : regions) {
        if (!region.isLand())
            continue;
        if (region.landClimateId() >= normalizedElevations.size()) {
            throw std::logic_error(
                "Landform classification received invalid region data.");
        }
        region.setLandform(classifyLandform(
            normalizedElevations[region.landClimateId()],
            conditions));
    }
}

} // namespace

WorldGenerator::WorldGenerator(WorldGenerationSettings settings)
    : m_settings(std::move(settings)) {
    validateSettings(m_settings);
}

const WorldGenerationSettings &WorldGenerator::settings() const noexcept {
    return m_settings;
}

World WorldGenerator::generate() const {
    const auto &boundingBox = m_settings.bounds;
    const JitteredGridSiteGenerator siteGenerator{
        m_settings.columns,
        m_settings.rows,
        m_settings.jitter,
        m_settings.seed,
    };
    auto sites = siteGenerator.generateSites(boundingBox);

    Fortune fortune;
    auto division = fortune.generate(sites, boundingBox);

    const Vector2d decayRadius{
        (boundingBox.max.x - boundingBox.min.x) * m_settings.edgeDecayRatio.x,
        (boundingBox.max.y - boundingBox.min.y) * m_settings.edgeDecayRatio.y,
    };
    const climate::ClimateGenerator climateGenerator{{
        .bounds = boundingBox,
        .seed = m_settings.seed,
        .equatorTemperature = m_settings.equatorTemperature,
        .poleTemperature = m_settings.poleTemperature,
        .vegetationCoefficient = m_settings.vegetationCoefficient,
        .humidityCoefficient = m_settings.humidityCoefficient,
        .temperatureNoiseStrength = m_settings.temperatureNoiseStrength,
        .temperatureNoiseFrequency = m_settings.temperatureNoiseFrequency,
        .temperatureElevationCooling = m_settings.temperatureElevationCooling,
        .temperatureHumidityInfluence = m_settings.temperatureHumidityInfluence,
        .temperatureLatitudeExponent = m_settings.temperatureLatitudeExponent,
        .noiseOctaves = m_settings.noiseOctaves,
        .noiseFrequency = m_settings.noiseFrequency,
        .noiseLacunarity = m_settings.noiseLacunarity,
        .noisePersistence = m_settings.noisePersistence,
    }};

    std::vector<Region> regions;
    regions.reserve(division.cells.size());
    std::vector<climate::ClimateSample> landClimates;
    std::vector<CellId> riverCandidateCells;
    riverCandidateCells.reserve(division.cells.size());
    for (const auto &cell : division.cells) {
        const auto elevation = noise::fractalNoise(cell.sitePosition,
                                                   m_settings.seed,
                                                   m_settings.noiseOctaves,
                                                   m_settings.noiseFrequency,
                                                   m_settings.noiseLacunarity,
                                                   m_settings.noisePersistence);
        const auto edgeAmount = noise::edgeDecay(boundingBox,
                                                 decayRadius,
                                                 cell.sitePosition);
        const auto effectiveSeaLevel = m_settings.seaLevel
                                       + edgeAmount * m_settings.edgeStrength;
        auto landClimateId = INVALID_LAND_CLIMATE_ID;
        if (elevation >= effectiveSeaLevel) {
            if (landClimates.size() >= INVALID_LAND_CLIMATE_ID) {
                throw std::length_error(
                    "The generated world has too many land climate samples.");
            }
            landClimateId = static_cast<LandClimateId>(landClimates.size());
            landClimates.push_back({
                .temperature = 0.0,
                .humidity = climateGenerator.humidity(cell.sitePosition),
                .vegetation = climateGenerator.vegetation(cell.sitePosition),
            });
        }
        regions.emplace_back(cell.id,
                             elevation,
                             effectiveSeaLevel,
                             cell.vertices.size(),
                             landClimateId);
        if (regions.back().isLand()
            && elevation >= m_settings.riverMinimumSourceElevation) {
            riverCandidateCells.push_back(cell.id);
        }
    }

    classifyWaterRegions(boundingBox, division, regions);

    auto rivers = generateRivers(boundingBox,
                                 division,
                                 regions,
                                 riverCandidateCells,
                                 m_settings.seed,
                                 m_settings.riverSourceCount,
                                 m_settings.riverRandomness,
                                 m_settings.riverElevationTolerance);

    applyRiverClimateInfluence(regions,
                               landClimates,
                               rivers,
                               m_settings.riverHumidityCoefficient,
                               m_settings.riverVegetationCoefficient);
    applyOceanHumidityInfluence(boundingBox,
                                division,
                                regions,
                                landClimates,
                                m_settings.oceanHumidityCoefficient,
                                m_settings.oceanHumidityDistanceRatio);
    const auto normalizedLandElevations = normalizeLandElevations(
        regions,
        landClimates.size());
    classifyLandforms(regions,
                      normalizedLandElevations,
                      m_settings.landformConditions);
    finalizeLandTemperatures(division,
                             regions,
                             landClimates,
                             normalizedLandElevations,
                             climateGenerator);
    classifyLandRegions(regions,
                        landClimates,
                        normalizedLandElevations,
                        m_settings.landTypeConditions);

    auto provinces = generateProvinces(
        boundingBox,
        division,
        regions,
        m_settings.provinceStartScore,
        m_settings.provinceRiverContribution,
        m_settings.provinceElevationContribution,
        m_settings.provinceDistanceContribution,
        m_settings.provinceBaseCost,
        m_settings.provinceMinimumRegionCount,
        m_settings.provinceShortBorderContribution,
        m_settings.provinceLandTypeContribution,
        m_settings.provinceMaximumRegionCount);

    return World{boundingBox,
                 std::move(division),
                 std::move(regions),
                 std::move(landClimates),
                 std::move(rivers),
                 std::move(provinces)};
}

} // namespace worldgen
