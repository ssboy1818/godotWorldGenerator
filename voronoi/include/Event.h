#pragma once

#include "Circle.h"
#include "Id.h"

enum class EventType {
    Circle, Site
};

class Event {
public:
    Event(EventType type) noexcept
        : m_type(type) {};

    EventType type() const noexcept {
        return m_type;
    };

    bool operator<(const Event &other) const noexcept;

private:
    EventType m_type;
};

class SiteEvent : public Event {
public:
    SiteEvent(SiteId site, Vector2d position) noexcept
        : Event(EventType::Site), m_site(site), m_position(position) {};

    SiteId site() const noexcept {
        return m_site;
    };

    Vector2d position() const noexcept {
        return m_position;
    };

private:
    SiteId m_site{INVALID_ID};
    Vector2d m_position;
};

class CircleEvent : public Event {
public:
    CircleEvent(Vector2d focusLeft,
                Vector2d focusCenter,
                Vector2d focusRight) noexcept
        : Event(EventType::Circle), m_circle(focusLeft, focusCenter, focusRight) {};

    bool isValid() const noexcept {
        return m_valid;
    };

    void setInvalid() noexcept {
        m_valid = false;
    };

    Circle &circle() noexcept {
        return m_circle;
    };

    const Circle &circle() const noexcept {
        return m_circle;
    };

private:
    Circle m_circle;
    bool m_valid{true};
};

// Implementation

inline bool Event::operator<(const Event &other) const noexcept  {
    if (m_type == EventType::Circle && other.m_type == EventType::Site)
        return true;
    if (m_type == EventType::Site && other.m_type == EventType::Circle)
        return false;

    if (m_type == EventType::Site) {
        auto lhs = static_cast<const SiteEvent *>(this);
        auto rhs = static_cast<const SiteEvent *>(&other);
        return lhs->position() < rhs->position();
    } else {
        auto lhs = static_cast<const CircleEvent *>(this)->circle().center();
        auto rhs = static_cast<const CircleEvent *>(&other)->circle().center();

        return lhs < rhs;
    }
};
