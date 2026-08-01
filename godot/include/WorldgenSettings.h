#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>

namespace worldgen {

class WorldgenSettings final : public godot::Resource {
    GDCLASS(WorldgenSettings, godot::Resource)

public:
    WorldgenSettings() = default;

    void setBounds(const godot::Rect2 &bounds);
    [[nodiscard]] godot::Rect2 bounds() const;

    void setSeed(std::int64_t seed);
    [[nodiscard]] std::int64_t seed() const noexcept;

    void setColumns(std::int64_t columns);
    [[nodiscard]] std::int64_t columns() const noexcept;

    void setRows(std::int64_t rows);
    [[nodiscard]] std::int64_t rows() const noexcept;

    void setJitter(double jitter);
    [[nodiscard]] double jitter() const noexcept;

    void setSeaLevel(double seaLevel);
    [[nodiscard]] double seaLevel() const noexcept;

    void setEdgeDecayRatio(const godot::Vector2 &ratio);
    [[nodiscard]] godot::Vector2 edgeDecayRatio() const;

    void setEdgeStrength(double strength);
    [[nodiscard]] double edgeStrength() const noexcept;

    void setNoiseOctaves(std::int64_t octaves);
    [[nodiscard]] std::int64_t noiseOctaves() const noexcept;

    void setNoiseFrequency(double frequency);
    [[nodiscard]] double noiseFrequency() const noexcept;

    void setNoiseLacunarity(double lacunarity);
    [[nodiscard]] double noiseLacunarity() const noexcept;

    void setNoisePersistence(double persistence);
    [[nodiscard]] double noisePersistence() const noexcept;

    void setRiverSourceCount(std::int64_t count);
    [[nodiscard]] std::int64_t riverSourceCount() const noexcept;

    void setRiverMinimumSourceElevation(double elevation);
    [[nodiscard]] double riverMinimumSourceElevation() const noexcept;

    void setRiverRandomness(double randomness);
    [[nodiscard]] double riverRandomness() const noexcept;

    void setRiverElevationTolerance(double tolerance);
    [[nodiscard]] double riverElevationTolerance() const noexcept;

    void setProvinceStartScore(double score);
    [[nodiscard]] double provinceStartScore() const noexcept;

    void setProvinceRiverContribution(double contribution);
    [[nodiscard]] double provinceRiverContribution() const noexcept;

    void setProvinceElevationContribution(double contribution);
    [[nodiscard]] double provinceElevationContribution() const noexcept;

    void setProvinceBaseCost(double cost);
    [[nodiscard]] double provinceBaseCost() const noexcept;

protected:
    static void _bind_methods();

private:
    godot::Rect2 m_bounds{{0.0, 0.0}, {2048.0, 2048.0}};
    std::int64_t m_seed{0};
    std::int64_t m_columns{100};
    std::int64_t m_rows{100};
    double m_jitter{0.8};
    double m_seaLevel{0.45};
    godot::Vector2 m_edgeDecayRatio{0.15, 0.15};
    double m_edgeStrength{0.55};
    std::int64_t m_noiseOctaves{5};
    double m_noiseFrequency{0.01};
    double m_noiseLacunarity{2.0};
    double m_noisePersistence{0.5};
    std::int64_t m_riverSourceCount{12};
    double m_riverMinimumSourceElevation{0.6};
    double m_riverRandomness{0.25};
    double m_riverElevationTolerance{0.03};
    double m_provinceStartScore{10.0};
    double m_provinceRiverContribution{5.0};
    double m_provinceElevationContribution{10.0};
    double m_provinceBaseCost{1.0};
};

} // namespace worldgen
