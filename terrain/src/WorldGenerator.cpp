#include "WorldGenerator.h"

#include "Fortune.h"
#include "JitteredGridSiteGenerator.h"
#include "PerlinNoise.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

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
    if (settings.noiseOctaves == 0)
        throw std::invalid_argument("Noise must have at least one octave.");
    if (!std::isfinite(settings.noiseFrequency) || settings.noiseFrequency <= 0.0)
        throw std::invalid_argument("Noise frequency must be positive.");
    if (!std::isfinite(settings.noiseLacunarity) || settings.noiseLacunarity <= 0.0)
        throw std::invalid_argument("Noise lacunarity must be positive.");
    if (!std::isfinite(settings.noisePersistence) || settings.noisePersistence < 0.0)
        throw std::invalid_argument("Noise persistence must be non-negative.");
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

    std::vector<Region> regions;
    regions.reserve(division.cells.size());
    for (const auto &cell : division.cells) {
        const auto elevation = noise::noise(boundingBox, cell.sitePosition, decayRadius,
                                            m_settings.seed,
                                            m_settings.noiseOctaves,
                                            m_settings.noiseFrequency,
                                            m_settings.noiseLacunarity,
                                            m_settings.noisePersistence);
        regions.emplace_back(cell.id,
                             elevation,
                             m_settings.seaLevel);
    }

    return World{boundingBox, std::move(division), std::move(regions)};
}

} // namespace worldgen
