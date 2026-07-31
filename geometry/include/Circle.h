#pragma once

#include "Vector2.h"
#include "Id.h"

#include <array>
#include <stdexcept>

namespace {

double det2x2(double a, double b, double c, double d) {
    return a * d - b * c;
}

} // namespace

class Circle {
public:
    Circle(Vector2d p1, Vector2d p2, Vector2d p3);

    [[nodiscard]] Vector2d center() const noexcept;

    [[nodiscard]] double radius() const noexcept;

private:
    std::array<Vector2d, 3> m_vertices;
    Vector2d m_center;

private:
    void calculateCenter();
};

inline Circle::Circle(Vector2d p1, Vector2d p2, Vector2d p3)
    : m_vertices({p1, p2, p3}) {
    calculateCenter();
}

inline Vector2d Circle::center() const noexcept {
    return m_center;
}

inline double Circle::radius() const noexcept {
    return (m_center - m_vertices[0]).length();
}

inline void Circle::calculateCenter() {
    auto &p1 = m_vertices[0];
    auto &p2 = m_vertices[1];
    auto &p3 = m_vertices[2];

    double sq1 = p1.squaredLength();
    double sq2 = p2.squaredLength();
    double sq3 = p3.squaredLength();

    double D = det2x2(p1.x - p2.x, p1.y - p2.y, p2.x - p3.x, p2.y - p3.y);

    if (std::abs(D) < EPS)
        throw std::invalid_argument("Точки лежат на одной прямой. Окружность построить невозможно.");

    double Dx = det2x2(sq1 - sq2, p1.y - p2.y, sq2 - sq3, p2.y - p3.y);
    double Dy = det2x2(p1.x - p2.x, sq1 - sq2, p2.x - p3.x, sq2 - sq3);

    m_center.x = Dx / (2 * D);
    m_center.y = Dy / (2 * D);
}
