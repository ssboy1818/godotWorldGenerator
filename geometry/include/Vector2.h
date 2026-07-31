#pragma once

#include <cmath>
#include <tuple>

template <class T>
class Vector2 {
public:
    T x{0};
    T y{0};

public:
    Vector2() noexcept;
    Vector2(T x_, T y_) noexcept;

    Vector2(const Vector2<T> &other) noexcept;

    Vector2(Vector2 &&other) = default;

    ~Vector2() noexcept = default;

    Vector2<T> &operator=(const Vector2<T> &other) noexcept;

    Vector2<T> &operator=(Vector2 &&other) = default;

    Vector2<T> operator+(Vector2<T> other) const noexcept;
    Vector2<T> operator-(Vector2<T> other) const noexcept;
    Vector2<T> operator*(Vector2<T> other) const noexcept;
    Vector2<T> operator/(Vector2<T> other) const noexcept;

    Vector2<T> operator+(T value) const noexcept;
    Vector2<T> operator-(T value) const noexcept;
    Vector2<T> operator*(T value) const noexcept;
    Vector2<T> operator/(T value) const noexcept;

    Vector2<T> &operator+=(Vector2<T> other) noexcept;
    Vector2<T> &operator-=(Vector2<T> other) noexcept;
    Vector2<T> &operator*=(Vector2<T> other) noexcept;
    Vector2<T> &operator/=(Vector2<T> other) noexcept;

    Vector2<T> &operator+=(T value) noexcept;
    Vector2<T> &operator-=(T value) noexcept;
    Vector2<T> &operator*=(T value) noexcept;
    Vector2<T> &operator/=(T value) noexcept;

    bool operator<(Vector2<T> other) const noexcept;
    bool operator>(Vector2<T> other) const noexcept;
    bool operator==(Vector2<T> other) const noexcept;

    explicit operator Vector2<int>() const noexcept;
    explicit operator Vector2<float>() const noexcept;
    explicit operator Vector2<double>() const noexcept;

    [[nodiscard]] double length() const noexcept;
    [[nodiscard]] double squaredLength() const noexcept;
};

using Vector2d = Vector2<double>;
using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;


// Implementation


template<class T>
inline Vector2<T>::Vector2() noexcept = default;

template<class T>
inline Vector2<T>::Vector2(T x_, T y_) noexcept
    : x(x_), y(y_) {}

template<class T>
inline Vector2<T>::Vector2(const Vector2<T> &other) noexcept
    : x(other.x), y(other.y) {}

template<class T>
inline Vector2<T> &Vector2<T>::operator=(const Vector2<T> &other) noexcept {
    if (this == &other)
        return *this;

    x = other.x;
    y = other.y;

    return *this;
}

template<class T>
inline Vector2<T> Vector2<T>::operator+(Vector2<T> other) const noexcept {
    return {x + other.x, y + other.y};
}

template<class T>
inline Vector2<T> Vector2<T>::operator-(Vector2<T> other) const noexcept {
    return {x - other.x, y - other.y};
}

template<class T>
inline Vector2<T> Vector2<T>::operator*(Vector2<T> other) const noexcept {
    return {x * other.x, y * other.y};
}

template<class T>
inline Vector2<T> Vector2<T>::operator/(Vector2<T> other) const noexcept {
    return {x / other.x, y / other.y};
}

template<class T>
inline Vector2<T> Vector2<T>::operator+(T value) const noexcept {
    return {x + value, y + value};
}

template<class T>
inline Vector2<T> Vector2<T>::operator-(T value) const noexcept {
    return {x - value, y - value};
}

template<class T>
inline Vector2<T> Vector2<T>::operator*(T value) const noexcept {
    return {x * value, y * value};
}

template<class T>
inline Vector2<T> Vector2<T>::operator/(T value) const noexcept {
    return {x / value, y / value};
}

template<class T>
inline Vector2<T> &Vector2<T>::operator+=(Vector2<T> other) noexcept {
    x += other.x;
    y += other.y;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator-=(Vector2<T> other) noexcept {
    x -= other.x;
    y -= other.y;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator*=(Vector2<T> other) noexcept {
    x *= other.x;
    y *= other.y;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator/=(Vector2<T> other) noexcept {
    x /= other.x;
    y /= other.y;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator+=(T value) noexcept {
    x += value;
    y += value;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator-=(T value) noexcept {
    x -= value;
    y -= value;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator*=(T value) noexcept {
    x *= value;
    y *= value;

    return *this;
}

template<class T>
inline Vector2<T> &Vector2<T>::operator/=(T value) noexcept {
    x /= value;
    y /= value;

    return *this;
}


template<class T>
inline bool Vector2<T>::operator<(Vector2<T> other) const noexcept {
    return std::tie(x, y) < std::tie(other.x, other.y);
}

template<class T>
inline bool Vector2<T>::operator>(Vector2<T> other) const noexcept {
    return other < *this;
}

template<class T>
inline bool Vector2<T>::operator==(Vector2<T> other) const noexcept {
    return std::tie(x, y) == std::tie(other.x, other.y);
}

template<class T>
inline Vector2<T>::operator Vector2<int>() const noexcept {
    return {static_cast<int>(x), static_cast<int>(y)};
}

template<class T>
inline Vector2<T>::operator Vector2<float>() const noexcept {
    return {static_cast<float>(x), static_cast<float>(y)};
}

template<class T>
inline Vector2<T>::operator Vector2<double>() const noexcept {
    return {static_cast<double>(x), static_cast<double>(y)};
}

template<class T>
inline double Vector2<T>::length() const noexcept {
    return std::hypot(x, y);
}

template<class T>
inline double Vector2<T>::squaredLength() const noexcept {
    return x*x + y*y;
}
