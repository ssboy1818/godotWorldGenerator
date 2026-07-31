#pragma once

#include "Circle.h"
#include "Id.h"

namespace worldgen {

class Arc;

enum class EventType {
    Circle,
    Site
};

class Event {
public:
    explicit Event(EventType type) noexcept
        : m_type(type) {}

    virtual ~Event() noexcept = default;

    [[nodiscard]] EventType type() const noexcept {
        return m_type;
    }

    [[nodiscard]] virtual Vector2d position() const noexcept = 0;

    bool operator<(const Event &other) const noexcept;

private:
    EventType m_type;
};

class SiteEvent : public Event {
public:
    SiteEvent(SiteId site, Vector2d position) noexcept
        : Event(EventType::Site), m_site(site), m_position(position) {}

    [[nodiscard]] SiteId site() const noexcept {
        return m_site;
    }

    [[nodiscard]] Vector2d position() const noexcept override {
        return m_position;
    }

private:
    SiteId m_site{INVALID_ID};
    Vector2d m_position;
};

class CircleEvent : public Event {
public:
    CircleEvent(Arc *arc,
                SiteId leftFocus,
                SiteId centerFocus,
                SiteId rightFocus,
                Vector2d focusLeft,
                Vector2d focusCenter,
                Vector2d focusRight)
        : Event(EventType::Circle),
          m_arc(arc),
          m_leftFocus(leftFocus),
          m_centerFocus(centerFocus),
          m_rightFocus(rightFocus),
          m_circle(focusLeft, focusCenter, focusRight) {}

    [[nodiscard]] Arc *arc() const noexcept {
        return m_arc;
    }

    [[nodiscard]] SiteId leftFocus() const noexcept {
        return m_leftFocus;
    }

    [[nodiscard]] SiteId centerFocus() const noexcept {
        return m_centerFocus;
    }

    [[nodiscard]] SiteId rightFocus() const noexcept {
        return m_rightFocus;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return m_valid;
    }

    void setInvalid() noexcept {
        m_valid = false;
    }

    [[nodiscard]] Vector2d position() const noexcept override {
        const auto center = m_circle.center();
        return {center.x, center.y - m_circle.radius()};
    }

    [[nodiscard]] Circle &circle() noexcept {
        return m_circle;
    }

    [[nodiscard]] const Circle &circle() const noexcept {
        return m_circle;
    }

private:
    Arc *m_arc{nullptr};
    SiteId m_leftFocus{INVALID_ID};
    SiteId m_centerFocus{INVALID_ID};
    SiteId m_rightFocus{INVALID_ID};
    Circle m_circle;
    bool m_valid{true};
};

inline bool Event::operator<(const Event &other) const noexcept {
    const auto lhs = position();
    const auto rhs = other.position();

    if (lhs.y != rhs.y)
        return lhs.y < rhs.y;
    if (lhs.x != rhs.x)
        return lhs.x < rhs.x;

    return m_type == EventType::Circle && other.m_type == EventType::Site;
}

} // namespace worldgen
