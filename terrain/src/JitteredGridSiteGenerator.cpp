#include "JitteredGridSiteGenerator.h"

#include <cmath>
#include <random>
#include <stdexcept>

JitteredGridSiteGenerator::JitteredGridSiteGenerator(
    std::size_t columns,
    std::size_t rows,
    double jitter,
    std::uint64_t seed)
    : m_columns(columns),
      m_rows(rows),
      m_jitter(jitter),
      m_seed(seed) {
    if (m_columns == 0 || m_rows == 0)
        throw std::invalid_argument("A site grid must have at least one row and column.");
    if (!std::isfinite(m_jitter) || m_jitter < 0.0 || m_jitter > 1.0)
        throw std::invalid_argument("Grid jitter must be between zero and one.");
}

std::vector<Site> JitteredGridSiteGenerator::generateSites(
    const BoundingBox &boundingBox) const {
    const auto cellWidth = (boundingBox.max.x - boundingBox.min.x)
                           / static_cast<double>(m_columns);
    const auto cellHeight = (boundingBox.max.y - boundingBox.min.y)
                            / static_cast<double>(m_rows);
    const auto maximumOffsetX = cellWidth * m_jitter * 0.5;
    const auto maximumOffsetY = cellHeight * m_jitter * 0.5;

    std::mt19937_64 random{m_seed};
    std::uniform_real_distribution<double> offsetX{-maximumOffsetX, maximumOffsetX};
    std::uniform_real_distribution<double> offsetY{-maximumOffsetY, maximumOffsetY};

    std::vector<Site> sites;
    sites.reserve(m_columns * m_rows);

    for (std::size_t row = 0; row < m_rows; ++row) {
        for (std::size_t column = 0; column < m_columns; ++column) {
            const Vector2d cellCenter{
                boundingBox.min.x + (static_cast<double>(column) + 0.5) * cellWidth,
                boundingBox.min.y + (static_cast<double>(row) + 0.5) * cellHeight,
            };
            sites.emplace_back(cellCenter.x + offsetX(random),
                               cellCenter.y + offsetY(random));
        }
    }

    return sites;
}
