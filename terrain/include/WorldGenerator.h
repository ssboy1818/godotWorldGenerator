#pragma once

#include "SiteGenerator.h"
#include "World.h"

#include <memory>

struct WorldGenerationSettings {
    double seaLevel{0.45};
    Vector2d edgeDecayRatio{0.15, 0.15};
};

class WorldGenerator {
public:
    explicit WorldGenerator(
        std::unique_ptr<SiteGenerator> siteGenerator,
        WorldGenerationSettings settings = {});

    void setSiteGenerator(std::unique_ptr<SiteGenerator> siteGenerator);

    [[nodiscard]] const SiteGenerator &siteGenerator() const noexcept;
    [[nodiscard]] World generate(const BoundingBox &boundingBox) const;

private:
    std::unique_ptr<SiteGenerator> m_siteGenerator;
    WorldGenerationSettings m_settings;
};
