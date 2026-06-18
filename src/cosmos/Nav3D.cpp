#include "cosmos/Nav3D.hpp"

#include <algorithm>
#include <cmath>

namespace cosmos {
namespace nav3d {

namespace {
constexpr double kHalfPi = 1.57079632679489661923;
}

double Vec3::length() const {
    return std::sqrt(x * x + y * y + z * z);
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalized(const Vec3& v) {
    const double len = v.length();
    return (len > 1.0e-12) ? Vec3{v.x / len, v.y / len, v.z / len} : Vec3{0.0, 0.0, 1.0};
}

void OrbitCamera::orbit(double delta_yaw, double delta_pitch) {
    yaw += delta_yaw;
    // Keep yaw bounded to avoid unbounded drift in long sessions.
    const double two_pi = 2.0 * (kHalfPi * 2.0);
    yaw = std::fmod(std::fmod(yaw, two_pi) + two_pi, two_pi);
    pitch = std::clamp(pitch + delta_pitch, -kHalfPi + 0.05, kHalfPi - 0.05);
}

void OrbitCamera::zoom(double factor) {
    distance = std::clamp(distance * factor, min_distance, max_distance);
}

void OrbitCamera::pan(double screen_dx, double screen_dy) {
    // Slide the target in the camera's view plane, scaled by distance so panning
    // feels consistent at every zoom level.
    const Vec3 r = right();
    const Vec3 u = up();
    const double scale = distance * 0.001;
    target = target + r * (-screen_dx * scale) + u * (screen_dy * scale);
}

void OrbitCamera::frame(const Vec3& center, double radius) {
    target = center;
    const double fov_rad = fov_degrees * 3.14159265358979323846 / 180.0;
    distance = std::clamp(radius / std::tan(std::max(0.05, fov_rad * 0.5)), min_distance,
                          max_distance);
}

Vec3 OrbitCamera::eye() const {
    const double cp = std::cos(pitch);
    const Vec3 offset{std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp};
    return target + offset * distance;
}

Vec3 OrbitCamera::forward() const {
    return normalized(target - eye());
}

Vec3 OrbitCamera::right() const {
    return normalized(cross(forward(), Vec3{0.0, 1.0, 0.0}));
}

Vec3 OrbitCamera::up() const {
    return normalized(cross(right(), forward()));
}

} // namespace nav3d
} // namespace cosmos
