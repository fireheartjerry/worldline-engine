#pragma once
#include <cmath>

// Double-precision 2D vector.  Only convert to float at the render boundary.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator-()              const { return {-x, -y}; }
    Vec2 operator*(double s)      const { return {x * s,   y * s};   }
    Vec2 operator/(double s)      const { return {x / s,   y / s};   }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

    double length() const { return std::sqrt(x * x + y * y); }
    double length_sq() const { return x * x + y * y; }
    double dot(const Vec2& o) const { return x * o.x + y * o.y; }
};

inline Vec2 operator*(double s, const Vec2& v) { return v * s; }
