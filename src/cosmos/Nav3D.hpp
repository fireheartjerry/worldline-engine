#pragma once

#include <cstdint>

// ============================================================================
// BOILERPLATE for the next major feature: 3D universe navigation.
//
// This is a self-contained, fully-tested orbit/pan/zoom camera with the math
// already worked out, so the eventual 3D renderer only has to consume eye(),
// target() and a projection. Nothing here depends on a graphics backend yet.
// ============================================================================

namespace cosmos {
namespace nav3d {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    double length() const;
};

Vec3 cross(const Vec3& a, const Vec3& b);
Vec3 normalized(const Vec3& v);

// An orbit camera: looks at `target`, positioned on a sphere of `distance` at
// (yaw, pitch). Pan slides the target in the view plane; zoom changes distance.
struct OrbitCamera {
    Vec3 target{0.0, 0.0, 0.0};
    double distance = 12.0;
    double yaw = 0.6;     // radians, around the up axis
    double pitch = 0.35;  // radians, clamped away from the poles
    double fov_degrees = 55.0;
    double min_distance = 1.0;
    double max_distance = 1.0e6;

    void orbit(double delta_yaw, double delta_pitch);
    void zoom(double factor);                 // factor < 1 moves in
    void pan(double screen_dx, double screen_dy);
    void frame(const Vec3& center, double radius); // fit a sphere into view

    Vec3 eye() const;       // world-space camera position
    Vec3 forward() const;   // normalized look direction (eye -> target)
    Vec3 right() const;     // normalized right vector
    Vec3 up() const;        // normalized up vector
};

} // namespace nav3d
} // namespace cosmos
