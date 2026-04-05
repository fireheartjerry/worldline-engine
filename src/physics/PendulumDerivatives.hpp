#pragma once
#include "PendulumState.hpp"
#include "PendulumParams.hpp"
#include "FlowField.hpp"
#include "ResistiveForces.hpp"
#include "../math/Vec2.hpp"
#include <cmath>

// Equations of motion derived from the Lagrangian of a standard planar
// double pendulum.  No raylib dependency — pure math.
//
// Rigid mode supports optional connector mass by treating each connector as
// a uniform slender rod.
inline Vec2 tangent_from_angle(double length, double theta) {
    return {
        length * std::cos(theta),
        -length * std::sin(theta)
    };
}

struct GeneralizedForces {
    double q1 = 0.0;
    double q2 = 0.0;

    void add_point_force(Vec2 force, Vec2 jacobian_theta1, Vec2 jacobian_theta2) {
        q1 += force.dot(jacobian_theta1);
        q2 += force.dot(jacobian_theta2);
    }
};

struct RigidDragSample {
    Vec2 position{};
    Vec2 velocity{};
    Vec2 j1{};
    Vec2 j2{};
    Vec2 axis{};
};

inline GeneralizedForces rigid_resistive_forces(const PendulumState& s,
                                                const PendulumParams& p) {
    GeneralizedForces forces;

    const Vec2 r1 = {
        p.l1 * std::sin(s.theta1),
        p.l1 * std::cos(s.theta1)
    };
    const Vec2 r2 = {
        p.l2 * std::sin(s.theta2),
        p.l2 * std::cos(s.theta2)
    };
    const Vec2 t1 = tangent_from_angle(p.l1, s.theta1);
    const Vec2 t2 = tangent_from_angle(p.l2, s.theta2);
    const Vec2 bob1_vel = t1 * s.omega1;
    const Vec2 bob2_vel = bob1_vel + t2 * s.omega2;
    const Vec2 fluid1 = flow_velocity_at(r1, p.flow_field);
    const Vec2 fluid2 = flow_velocity_at(r1 + r2, p.flow_field);

    forces.add_point_force(drag_force(bob1_vel - fluid1, p.bob1_drag), t1, {});
    forces.add_point_force(drag_force(bob2_vel - fluid2, p.bob2_drag), t1, t2);

    auto accumulate_segment_drag =
        [&](double length, const DirectionalDragCoefficients& drag, auto jacobian) {
            if (length <= 1e-9 || !drag_active(drag)) {
                return;
            }

            integrate_unit_interval_gauss5([&](double s_unit, double weight) {
                const auto sample = jacobian(s_unit);
                const Vec2 drag_density =
                    anisotropic_drag_force(
                        sample.velocity - flow_velocity_at(sample.position, p.flow_field),
                        sample.axis,
                        drag);
                const double line_weight = weight * length;
                forces.q1 += line_weight * drag_density.dot(sample.j1);
                forces.q2 += line_weight * drag_density.dot(sample.j2);
            });
        };

    accumulate_segment_drag(p.l1, p.connector1_drag,
        [&](double s_unit) {
            const Vec2 position = r1 * s_unit;
            const Vec2 j1 = t1 * s_unit;
            return RigidDragSample{position, j1 * s.omega1, j1, {}, r1};
        });

    accumulate_segment_drag(p.l2, p.connector2_drag,
        [&](double s_unit) {
            const Vec2 position = r1 + r2 * s_unit;
            const Vec2 j1 = t1;
            const Vec2 j2 = t2 * s_unit;
            return RigidDragSample{position, j1 * s.omega1 + j2 * s.omega2, j1, j2, r2};
        });

    const double pivot_tau = joint_resistive_torque(s.omega1, p.pivot_resistance);
    forces.q1 += pivot_tau;

    const double elbow_tau = joint_resistive_torque(s.omega2 - s.omega1, p.elbow_resistance);
    forces.q1 -= elbow_tau;
    forces.q2 += elbow_tau;

    return forces;
}

inline PendulumState pendulum_derivatives(const PendulumState& s,
                                          const PendulumParams& p) {
    const double connector1_mass =
        (p.connector_mode == ConnectorMode::RIGID) ? p.connector1_mass : 0.0;
    const double connector2_mass =
        (p.connector_mode == ConnectorMode::RIGID) ? p.connector2_mass : 0.0;

    const double delta     = s.theta1 - s.theta2;
    const double cos_delta = std::cos(delta);
    const double sin_delta = std::sin(delta);

    const double inertia11 = p.l1 * p.l1 *
        (p.m1 + p.m2 + connector2_mass + connector1_mass / 3.0);
    const double inertia22 = p.l2 * p.l2 *
        (p.m2 + connector2_mass / 3.0);
    const double coupling = p.l1 * p.l2 *
        (p.m2 + connector2_mass / 2.0);
    const double gravity1 = p.g * p.l1 *
        (p.m1 + p.m2 + connector2_mass + connector1_mass / 2.0);
    const double gravity2 = p.g * p.l2 *
        (p.m2 + connector2_mass / 2.0);

    const double m12 = coupling * cos_delta;
    double rhs1 =
        -coupling * sin_delta * s.omega2 * s.omega2
        -gravity1 * std::sin(s.theta1);
    double rhs2 =
        coupling * sin_delta * s.omega1 * s.omega1
        -gravity2 * std::sin(s.theta2);

    const GeneralizedForces resistance = rigid_resistive_forces(s, p);
    rhs1 += resistance.q1;
    rhs2 += resistance.q2;

    const double det = inertia11 * inertia22 - m12 * m12;
    const double safe_det = (std::abs(det) < 1e-9) ? (det >= 0.0 ? 1e-9 : -1e-9) : det;

    const double alpha1 = (rhs1 * inertia22 - m12 * rhs2) / safe_det;
    const double alpha2 = (inertia11 * rhs2 - m12 * rhs1) / safe_det;

    // [dtheta1/dt, dtheta2/dt, domega1/dt, domega2/dt]
    return {s.omega1, s.omega2, alpha1, alpha2};
}
