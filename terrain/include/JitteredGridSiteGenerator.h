#pragma once

#include "SiteGenerator.h"

#include <cstddef>
#include <cstdint>
#include <random>

class JitteredGridSiteGenerator final : public SiteGenerator {
public:
    JitteredGridSiteGenerator(
        std::size_t columns,
        std::size_t rows,
        double jitter = 0.8,
        std::uint64_t seed = std::random_device{}());

    [[nodiscard]] std::vector<Site> generateSites(
        const BoundingBox &boundingBox) const override;

private:
    std::size_t m_columns;
    std::size_t m_rows;
    double m_jitter;
    std::uint64_t m_seed;
};
