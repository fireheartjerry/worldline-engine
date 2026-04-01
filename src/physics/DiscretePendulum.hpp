#pragma once
#include "Universe.hpp"
#include "../seed/UniverseAxioms.hpp"
#include "../seed/AxiomDeriver.hpp"
#include "../math/Vec2.hpp"
#include <cmath>
#include <cstdint>
#include <string>

// ============================================================================
// DiscretePendulum — pendulum whose configuration space is a discrete grid
// and whose dynamics are an iterated algebraic map (no ODE).
//
// Map construction
// ----------------
// The seed determines four coefficients (a, b, c, d) via the axioms.
// Each tick the angle pair (theta1, theta2) is updated by:
//
//   theta1' = theta1 + b * sin(theta2)   (mod 2π)
//   theta2' = theta2 + d * sin(theta1')  (mod 2π)
//
// This is a generalised Standard Map (Chirikov map) on T² = [0, 2π)².
// With appropriate coefficients it exhibits KAM tori, chaos islands, and
// full ergodicity — a rich palette of visual patterns.
//
// Angular "velocities" for colour encoding are approximated as finite
// differences: omega_i ≈ (theta_i' - theta_i) / dt_visual.
//
// Determinism
// -----------
// The map coefficients are derived entirely from the seed's continuous
// parameters (gravity_exponent and noise_amplitude) and are fixed at
// construction.  The same axioms always produce the same trajectory.
// ============================================================================

class DiscretePendulum : public Universe {
public:
    explicit DiscretePendulum(const Worldline::UniverseAxioms& ax) {
        l1 = 1.0;
        l2 = 1.0;

        // Map coefficients from axioms — gravity_exponent drives the
        // kicking strength (analogous to the non-linearity parameter K
        // in the Standard Map).
        b = ax.gravity_exponent * 0.6;   // range ≈ [0.9, 1.8]
        d = ax.gravity_exponent * 0.4 + ax.noise_amplitude * 0.1;

        theta1 = 2.0;
        theta2 = 1.8;
        prev1  = theta1;
        prev2  = theta2;
    }

    void step(double /*dt*/) override {
        prev1 = theta1;
        prev2 = theta2;

        // Standard-map update on T²
        theta1 = std::fmod(theta1 + b * std::sin(theta2), TWO_PI);
        theta2 = std::fmod(theta2 + d * std::sin(theta1), TWO_PI);

        // Ensure positive modulo
        if (theta1 < 0.0) theta1 += TWO_PI;
        if (theta2 < 0.0) theta2 += TWO_PI;
    }

    RenderState extract() const override {
        // Map torus angles [0, 2π) → pendulum display angles [-π, π)
        const double a1 = theta1 - PI;
        const double a2 = theta2 - PI;

        const Vec2 b1 = {l1 * std::sin(a1), l1 * std::cos(a1)};
        const Vec2 b2 = {b1.x + l2 * std::sin(a2),
                         b1.y + l2 * std::cos(a2)};

        const double omega1_approx = (theta1 - prev1);
        const double omega2_approx = (theta2 - prev2);

        return RenderState{
            {0.0, 0.0},
            b1,
            b2,
            omega2_approx * 5.0,  // scaled for colour range
            0.0,
            0.0,
            false
        };
    }

    std::string describe() const override {
        return "Euclidean · Discrete map (Standard Map T\xC2\xB2) · rigid joint";
    }

private:
    static constexpr double TWO_PI = 6.283185307179586;
    static constexpr double PI     = 3.141592653589793;
    double l1, l2;
    double b, d;                  // map coefficients
    double theta1, theta2;
    double prev1,  prev2;         // previous tick, for finite-difference omega
};
