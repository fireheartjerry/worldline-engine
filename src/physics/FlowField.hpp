#pragma once
#include "PendulumParams.hpp"
#include "../math/Vec2.hpp"
#include <cmath>

inline bool flow_field_active(const FlowFieldParams& flow) {
    return std::abs(flow.wind_x) > 1e-12
        || std::abs(flow.wind_y) > 1e-12
        || std::abs(flow.shear_x) > 1e-12
        || std::abs(flow.shear_y) > 1e-12
        || std::abs(flow.swirl) > 1e-12;
}

inline Vec2 flow_velocity_at(Vec2 position, const FlowFieldParams& flow) {
    return {
        flow.wind_x + flow.shear_x * position.y - flow.swirl * position.y,
        flow.wind_y + flow.shear_y * position.x + flow.swirl * position.x
    };
}
