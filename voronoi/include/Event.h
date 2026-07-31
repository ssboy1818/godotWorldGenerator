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

private:
    EventType m_type;
};

class SiteEvent : public Event {
public:
    SiteEvent() noexcept : Event(EventType::Site) {};

    SiteId site() const noexcept {
        return m_id;
    };

private:
    SiteId m_id;
};

class CircleEvent : public Event {
public:
    CircleEvent(Vector2d focusLeft,
                Vector2d focusCenter,
                Vector2d focusRight) noexcept
        : m_circle(focusLeft, focusCenter, focusRight), Event(EventType::Circle) {};

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