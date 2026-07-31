#include "WorldGenerator.h"

#include "Fortune.h"
#include "PerlinNoise.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

void validateSettings(const WorldGenerationSettings &settings) {
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
}

} // namespace

WorldGenerator::WorldGenerator(
    std::unique_ptr<SiteGenerator> siteGenerator,
    WorldGenerationSettings settings)
    : m_siteGenerator(std::move(siteGenerator)),
      m_settings(settings) {
    if (!m_siteGenerator)
        throw std::invalid_argument("A world generator needs a site generator.");
    validateSettings(m_settings);
}

void WorldGenerator::setSiteGenerator(std::unique_ptr<SiteGenerator> siteGenerator) {
    if (!siteGenerator)
        throw std::invalid_argument("A world generator needs a site generator.");
    m_siteGenerator = std::move(siteGenerator);
}

const SiteGenerator &WorldGenerator::siteGenerator() const noexcept {
    return *m_siteGenerator;
}

World WorldGenerator::generate(const BoundingBox &boundingBox) const {
    auto sites = m_siteGenerator->generateSites(boundingBox);

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
                                            5, 0.01);
        regions.emplace_back(cell.id,
                             elevation,
                             m_settings.seaLevel);
    }

    return World{boundingBox, std::move(division), std::move(regions)};
}
