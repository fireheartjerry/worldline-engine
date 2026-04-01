#pragma once
#include "Universe.hpp"
#include "../seed/UniverseAxioms.hpp"
#include "../seed/AxiomDeriver.hpp"
#include "../math/Vec2.hpp"
#include <cmath>
#include <string>

// ============================================================================
// HyperbolicPendulum — double pendulum living on the Poincaré disk (H²).
//
// Geometry
// --------
// The Poincaré disk model represents H² as the open unit disk D = {z : |z| < 1}
// with metric  ds² = 4(dx² + dy²) / (1 - x² - y²)².
// The curvature is K = -1 (or K = space_curvature via the axiom).
//
// Arms are hyperbolic geodesics — circular arcs that meet the boundary at
// right angles.  A "joint angle" θ parametrises how far along the geodesic
// from the current bob the next bob sits.
//
// State
// -----
// We keep the same four generalised coordinates (theta1, theta2, omega1, omega2)
// as the Euclidean case, but interpret them in the hyperbolic metric:
//   - theta_i is the geodesic angle (arc length = l_i in the hyperbolic metric)
//   - The effective restoring force uses sinh instead of sin for the
//     hyperbolic sine rule.
//
// The equations of motion are the Euler-Lagrange equations for the Lagrangian:
//   L = KE_hyperbolic - PE_hyperbolic
// where the kinetic energy uses the hyperbolic metric tensor evaluated at the
// current position, and the potential is a height function on the disk.
//
// Implementation note (Phase 3)
// -------------------
// This is a first-order approximation: the arms are treated as straight
// segments in the embedding, with hyperbolic length corrected by a cosh
// factor.  A full geodesic RK4 on the tangent bundle is deferred to Phase 4.
// ============================================================================

class HyperbolicPendulum : public Universe {
public:
    explicit HyperbolicPendulum(const Worldline::UniverseAxioms& ax) {
        const Worldline::DerivedPhysicsParams dp =
            Worldline::derive_physics_params(ax);
        l1  = dp.pendulum.l1;
        l2  = dp.pendulum.l2;
        m1  = dp.pendulum.m1;
        m2  = dp.pendulum.m2;
        g   = dp.pendulum.g;
        k   = ax.space_curvature != 0.0 ? ax.space_curvature : -1.0;

        theta1 = 2.0;  omega1 = 0.0;
        theta2 = 1.8;  omega2 = 0.0;
    }

    void step(double dt) override {
        // Lagrangian equations with hyperbolic corrections.
        // The sinh factor accounts for the expanding volume element of H².
        const double delta     = theta1 - theta2;
        const double sin_d     = std::sin(delta);
        const double cos_d     = std::cos(delta);

        // Effective lengths in hyperbolic metric (cosh correction)
        const double hl1 = l1 * std::cosh(l1 * 0.5);
        const double hl2 = l2 * std::cosh(l2 * 0.5);

        const double denom = 2.0 * m1 + m2 - m2 * std::cos(2.0 * delta);

        const double alpha1 = (
            -g * (2.0 * m1 + m2) * std::sinh(hl1 * std::sin(theta1)) / hl1
            - m2 * g * std::sin(theta1 - 2.0 * theta2)
            - 2.0 * sin_d * m2 * (omega2 * omega2 * l2 + omega1 * omega1 * l1 * cos_d)
        ) / (l1 * denom);

        const double alpha2 = (
            2.0 * sin_d * (
                omega1 * omega1 * l1 * (m1 + m2)
                + g * (m1 + m2) * std::cosh(hl2 * std::cos(theta1))
                + omega2 * omega2 * l2 * m2 * cos_d
            )
        ) / (l2 * denom);

        // Simple Euler step for Phase 3; RK4 on the tangent bundle in Phase 4.
        theta1 += omega1 * dt;
        theta2 += omega2 * dt;
        omega1 += alpha1 * dt;
        omega2 += alpha2 * dt;
    }

    RenderState extract() const override {
        // Map from hyperbolic angles to Poincaré disk coordinates,
        // then to the flat world-space the Renderer expects.
        // r_disk = tanh(l / 2) maps hyperbolic distance to disk radius.
        const double r1 = std::tanh(l1 * 0.5);
        const Vec2 b1 = {r1 * std::sin(theta1), r1 * std::cos(theta1)};

        const double r2 = std::tanh(l2 * 0.5);
        const Vec2 b2 = {b1.x + r2 * std::sin(theta2),
                         b1.y + r2 * std::cos(theta2)};

        // Scale out of the disk — multiply by a constant so the Renderer's
        // SCALE factor produces a sensible screen size.
        constexpr double DISK_SCALE = 1.8;
        return RenderState{
            {0.0, 0.0},
            b1 * DISK_SCALE,
            b2 * DISK_SCALE,
            omega2,
            0.0,    // energy not trivially defined for hyperbolic metric
            0.0,
            false
        };
    }

    std::string describe() const override {
        return "Hyperbolic (Poincare disk) · Lagrangian · rigid joint";
    }

private:
    double l1, l2, m1, m2, g, k;
    double theta1, theta2, omega1, omega2;
};
