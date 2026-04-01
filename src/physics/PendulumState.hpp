#pragma once

// Phase-space state of a double pendulum.
// Angles measured from the downward vertical; y-axis points down.
struct PendulumState {
    double theta1 = 0.0;  // rod 1 angle (radians)
    double theta2 = 0.0;  // rod 2 angle (radians)
    double omega1 = 0.0;  // rod 1 angular velocity (rad/s)
    double omega2 = 0.0;  // rod 2 angular velocity (rad/s)

    // Arithmetic required by the generic RK4 integrator.
    PendulumState operator+(const PendulumState& o) const {
        return {theta1 + o.theta1, theta2 + o.theta2,
                omega1 + o.omega1, omega2 + o.omega2};
    }
    PendulumState operator*(double s) const {
        return {theta1 * s, theta2 * s, omega1 * s, omega2 * s};
    }
};
