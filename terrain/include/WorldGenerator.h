#pragma once

#include "World.h"

#include <cstddef>
#include <cstdint>

namespace worldgen {

struct WorldGenerationSettings {
    BoundingBox bounds;
    std::uint64_t seed{0};

    std::size_t columns{100};
    std::size_t rows{100};
    double jitter{0.8};

    double seaLevel{0.45};
    Vector2d edgeDecayRatio{0.15, 0.15};
    double edgeStrength{0.55};

    std::uint32_t noiseOctaves{5};
    double noiseFrequency{0.01};
    double noiseLacunarity{2.0};
    double noisePersistence{0.5};

    double equatorTemperature{30.0};
    double poleTemperature{-20.0};
    double vegetationCoefficient{1.0};
    double humidityCoefficient{1.0};

    std::size_t riverSourceCount{12};
    double riverMinimumSourceElevation{0.6};
    double riverRandomness{0.25};
    double riverElevationTolerance{0.03};

    double provinceStartScore{10.0};
    double provinceRiverContribution{5.0};
    double provinceElevationContribution{10.0};
    double provinceDistanceContribution{5.0};
    double provinceBaseCost{1.0};
    std::size_t provinceMinimumRegionCount{3};
};

class WorldGenerator {
public:
    explicit WorldGenerator(WorldGenerationSettings settings);

    [[nodiscard]] const WorldGenerationSettings &settings() const noexcept;
    [[nodiscard]] World generate() const;

private:
    const WorldGenerationSettings m_settings;
};

} // namespace worldgen
