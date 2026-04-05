#pragma once
#include "PendulumParams.hpp"
#include "../math/Vec2.hpp"
#include <cmath>

inline bool drag_active(const DragCoefficients& drag) {
    return drag.linear > 1e-12 || drag.quadratic > 1e-12;
}

inline bool drag_active(const DirectionalDragCoefficients& drag) {
    return drag_active(drag.axial) || drag_active(drag.normal);
}

inline bool joint_resistance_active(const JointResistance& resistance) {
    return resistance.viscous > 1e-12
        || resistance.quadratic > 1e-12
        || resistance.coulomb > 1e-12;
}

inline Vec2 drag_force(Vec2 velocity, const DragCoefficients& drag) {
    const double speed_sq = velocity.length_sq();
    if (speed_sq < 1e-12) {
        return {};
    }

    const double speed = std::sqrt(speed_sq);
    const double coeff = drag.linear + drag.quadratic * speed;
    if (coeff <= 0.0) {
        return {};
    }

    return velocity * (-coeff);
}

inline Vec2 anisotropic_drag_force(Vec2 velocity,
                                   Vec2 axis_hint,
                                   const DirectionalDragCoefficients& drag) {
    if (!drag_active(drag)) {
        return {};
    }

    const double axis_length = axis_hint.length();
    if (axis_length < 1e-12) {
        return drag_force(velocity, drag.normal);
    }

    const Vec2 axis = axis_hint / axis_length;
    const Vec2 parallel = axis * velocity.dot(axis);
    const Vec2 normal = velocity - parallel;
    return drag_force(parallel, drag.axial) + drag_force(normal, drag.normal);
}

inline double smooth_sign(double x, double smoothing = 1e-3) {
    return x / std::sqrt(x * x + smoothing * smoothing);
}

inline double joint_resistive_torque(double angular_velocity,
                                     const JointResistance& resistance,
                                     double smoothing = 1e-3) {
    const double abs_omega = std::abs(angular_velocity);
    const double magnitude =
        resistance.viscous * abs_omega
        + resistance.quadratic * abs_omega * abs_omega
        + resistance.coulomb;
    if (magnitude <= 0.0) {
        return 0.0;
    }
    return -smooth_sign(angular_velocity, smoothing) * magnitude;
}

template<typename Fn>
inline void integrate_unit_interval_gauss5(Fn&& fn) {
    constexpr double nodes[5] = {
        0.04691007703066802,
        0.23076534494715845,
        0.5,
        0.7692346550528415,
        0.9530899229693319
    };
    constexpr double weights[5] = {
        0.11846344252809454,
        0.23931433524968324,
        0.28444444444444444,
        0.23931433524968324,
        0.11846344252809454
    };

    for (int i = 0; i < 5; ++i) {
        fn(nodes[i], weights[i]);
    }
}
