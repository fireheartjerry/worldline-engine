#pragma once
#include "PendulumState.hpp"
#include "PendulumParams.hpp"
#include <cmath>

// Equations of motion derived from the Lagrangian of a standard planar
// double pendulum.  No raylib dependency — pure math.
//
// Phase 3 will read params.gravity_exponent to warp the effective g field.
inline PendulumState pendulum_derivatives(const PendulumState& s,
                                          const PendulumParams& p) {
    const double delta     = s.theta1 - s.theta2;
    const double cos_delta = std::cos(delta);
    const double sin_delta = std::sin(delta);

    // Common denominator (identical for both angular accelerations)
    const double denom = 2.0 * p.m1 + p.m2 - p.m2 * std::cos(2.0 * delta);

    const double alpha1 = (
        -p.g * (2.0 * p.m1 + p.m2) * std::sin(s.theta1)
        - p.m2 * p.g * std::sin(s.theta1 - 2.0 * s.theta2)
        - 2.0 * sin_delta * p.m2 *
          (s.omega2 * s.omega2 * p.l2 +
           s.omega1 * s.omega1 * p.l1 * cos_delta)
    ) / (p.l1 * denom);

    const double alpha2 = (
        2.0 * sin_delta * (
            s.omega1 * s.omega1 * p.l1 * (p.m1 + p.m2)
          + p.g * (p.m1 + p.m2) * std::cos(s.theta1)
          + s.omega2 * s.omega2 * p.l2 * p.m2 * cos_delta
        )
    ) / (p.l2 * denom);

    // [dtheta1/dt, dtheta2/dt, domega1/dt, domega2/dt]
    return {s.omega1, s.omega2, alpha1, alpha2};
}

// ============================================================================
// Split types for the generic velocity_verlet integrator in Integrators.hpp.
//
// PendulumPos  = (theta1, theta2) — generalised coordinates
// PendulumVel  = (omega1, omega2) — generalised velocities
//
// Both carry the arithmetic operators that velocity_verlet requires.
// ============================================================================
struct PendulumPos {
    double theta1 = 0.0;
    double theta2 = 0.0;

    PendulumPos operator+(const PendulumPos& o) const { return {theta1+o.theta1, theta2+o.theta2}; }
    PendulumPos operator*(double s)             const { return {theta1*s, theta2*s}; }

    // Cross-type addition: pos + vel*dt = new pos.
    // Required by Integrators::velocity_verlet for the drift step q_new = q + v_h * dt.
    // v_h * dt yields a PendulumVel whose omega fields hold the scaled increments.
    PendulumPos operator+(const struct PendulumVel& v) const;
};

struct PendulumVel {
    double omega1 = 0.0;
    double omega2 = 0.0;

    PendulumVel operator+(const PendulumVel& o) const { return {omega1+o.omega1, omega2+o.omega2}; }
    PendulumVel operator*(double s)             const { return {omega1*s, omega2*s}; }
};

// Out-of-line definition so PendulumVel is fully defined before use.
inline PendulumPos PendulumPos::operator+(const PendulumVel& v) const {
    return {theta1 + v.omega1, theta2 + v.omega2};
}

// Free functions consumed by Integrators::velocity_verlet<>
inline PendulumPos pendulum_get_pos(const PendulumState& s) {
    return {s.theta1, s.theta2};
}
inline PendulumVel pendulum_get_vel(const PendulumState& s) {
    return {s.omega1, s.omega2};
}
inline PendulumState pendulum_combine(const PendulumPos& q, const PendulumVel& v) {
    return {q.theta1, q.theta2, v.omega1, v.omega2};
}
// accel(q, v, params) → PendulumVel  (the d²q/dt² part of the Lagrangian)
inline PendulumVel pendulum_accel(const PendulumPos& q,
                                   const PendulumVel& v,
                                   const PendulumParams& p) {
    const PendulumState tmp{q.theta1, q.theta2, v.omega1, v.omega2};
    const PendulumState d = pendulum_derivatives(tmp, p);
    return {d.omega1, d.omega2};  // alpha1, alpha2
}
