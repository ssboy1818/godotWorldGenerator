#pragma once

#include "Vector2.h"

using SiteId = ssize_t;
inline constexpr SiteId INVALID_SITE_ID = -1;

namespace {

inline static SiteId newId() noexcept {
    static SiteId id_ = 0;
    return id_++;
}

} // namespace

class Site {
public:
    SiteId id{newId()};
    Vector2d position;

public:
    Site() noexcept = default;
    Site(double x, double y) noexcept;
    Site(Vector2d position_) noexcept;

    Site(const Site &other) noexcept;

    Site(Site &&other) noexcept;

    ~Site() noexcept = default;

    Site &operator=(const Site &other) noexcept;

    Site &operator=(Site &&other) noexcept;

    bool operator<(const Site &other) const noexcept;
    bool operator>(const Site &other) const noexcept;
    bool operator==(const Site &other) const noexcept;
};

// Implementation

inline Site::Site(double x, double y) noexcept
    : position(x, y) {}

inline Site::Site(Vector2d position_) noexcept
    : position(position_) {}

inline Site::Site(const Site &other) noexcept
    : position(other.position) {}

inline Site::Site(Site &&other) noexcept
    : id(other.id), position(other.position) {
    other.id = INVALID_SITE_ID;
    other.position = {0, 0};
}

inline Site &Site::operator=(const Site &other) noexcept {
    if (this == &other)
        return *this;

    position = other.position;
    return *this;
}

inline Site &Site::operator=(Site &&other) noexcept {
    if (this == &other)
        return *this;

    id = other.id;
    position = other.position;

    other.id = INVALID_SITE_ID;
    other.position = {0, 0};

    return *this;
}

inline bool Site::operator<(const Site &other) const noexcept {
    if ((position.x < other.position.x) ||
        (position.x == other.position.x && position.y < other.position.y))
        return true;
    return false;
}

inline bool Site::operator>(const Site &other) const noexcept {
    if ((position.x > other.position.x) ||
        (position.x == other.position.x && position.y > other.position.y))
        return true;
    return false;
}

inline bool Site::operator==(const Site &other) const noexcept {
    return position.x == other.position.x
        && position.y == other.position.y;
}


