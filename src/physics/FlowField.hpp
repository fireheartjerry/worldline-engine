#pragma once
#include "PendulumParams.hpp"
#include "../math/Vec2.hpp"
#include <cmath>

inline bool flow_field_active(const FlowFieldParams& flow) {
    return std::abs(flow.wind_x) > 1e-12
        || std::abs(flow.wind_y) > 1e-12
        || std::abs(flow.shear_x) > 1e-12
        || std::abs(flow.shear_y) > 1e-12
        || std::abs(flow.swirl) > 1e-12
        || std::abs(flow.gust_x) > 1e-12
        || std::abs(flow.gust_y) > 1e-12;
}

inline double gust_angular_frequency(const FlowFieldParams& flow) {
    constexpr double tau = 6.28318530717958647692;
    return tau * std::max(0.0, flow.gust_frequency);
}

inline Vec2 base_flow_velocity_at(Vec2 position, const FlowFieldParams& flow) {
    return {
        flow.wind_x + flow.shear_x * position.y - flow.swirl * position.y,
        flow.wind_y + flow.shear_y * position.x + flow.swirl * position.x
    };
}

inline Vec2 flow_velocity_at(Vec2 position,
                             const FlowFieldParams& flow,
                             double time_seconds = 0.0) {
    const Vec2 base = base_flow_velocity_at(position, flow);
    const double phase = gust_angular_frequency(flow) * time_seconds;
    const double gust = std::sin(phase);
    return {
        base.x + flow.gust_x * gust,
        base.y + flow.gust_y * gust
    };
}

inline Vec2 flow_acceleration_at(Vec2 position,
                                 const FlowFieldParams& flow,
                                 double time_seconds = 0.0) {
    const Vec2 velocity = flow_velocity_at(position, flow, time_seconds);
    const double omega = gust_angular_frequency(flow);
    const double gust_derivative = omega * std::cos(omega * time_seconds);
    return {
        flow.gust_x * gust_derivative
            + velocity.y * (flow.shear_x - flow.swirl),
        flow.gust_y * gust_derivative
            + velocity.x * (flow.shear_y + flow.swirl)
    };
}
