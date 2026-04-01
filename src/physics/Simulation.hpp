#pragma once
#include "PendulumState.hpp"
#include "PendulumParams.hpp"
#include "PendulumDerivatives.hpp"
#include "../math/Integrators.hpp"
#include "../math/Vec2.hpp"
#include "../seed/UniverseAxioms.hpp"
#include "../seed/AxiomDeriver.hpp"
#include <cmath>
#include <string>

// Wraps state + params and advances them via Störmer-Verlet (leapfrog).
// The double pendulum is a conservative Hamiltonian system; Störmer-Verlet
// preserves a modified Hamiltonian exactly, giving far better long-term
// energy conservation than RK4 at the same step size.
// RK4 remains available in Integrators.hpp for non-conservative exotic types.
// No raylib dependency — all coordinates are in physics units (metres).
class Simulation {
public:
    PendulumState state;
    PendulumParams params;

    Simulation(PendulumState initial, PendulumParams p)
        : state(initial), params(p) {}

    void step(double dt) {
        // Störmer-Verlet (velocity Verlet / leapfrog) — symplectic integrator.
        // Specifying the four non-deducible type params explicitly; the four
        // function-type params (GetPos…Accel) are deduced from the arguments.
        state = Integrators::velocity_verlet<
            PendulumState, PendulumPos, PendulumVel, PendulumParams>(
            state, params, dt,
            pendulum_get_pos, pendulum_get_vel,
            pendulum_combine, pendulum_accel);
    }

    // World-space position of bob 1.  Pivot is the origin; +y is down.
    Vec2 bob1_pos() const {
        return {
            params.l1 * std::sin(state.theta1),
            params.l1 * std::cos(state.theta1)
        };
    }

    // World-space position of bob 2 (tip of the chain).
    Vec2 bob2_pos() const {
        const Vec2 b1 = bob1_pos();
        return {
            b1.x + params.l2 * std::sin(state.theta2),
            b1.y + params.l2 * std::cos(state.theta2)
        };
    }
};

// ============================================================================
// StandardPendulumUniverse — wraps Simulation to satisfy the Universe contract.
// Defined here (not in Universe.hpp) to avoid circular includes.
// ============================================================================

class StandardPendulumUniverse {
public:
    explicit StandardPendulumUniverse(const Worldline::UniverseAxioms& ax) {
        const Worldline::DerivedPhysicsParams dp =
            Worldline::derive_physics_params(ax);
        sim = Simulation(default_initial(), dp.pendulum);
    }

    void step(double dt) { sim.step(dt); }

    // Fills a RenderState — called by Universe::create's returned object.
    // (Universe.hpp will use this through its inline create() implementation.)
    Vec2   bob1()             const { return sim.bob1_pos(); }
    Vec2   bob2()             const { return sim.bob2_pos(); }
    double angular_velocity() const { return sim.state.omega2; }
    double energy()           const {
        // Total mechanical energy (kinetic + potential) for the standard
        // Lagrangian double pendulum.
        const auto& s = sim.state;
        const auto& p = sim.params;
        const double v1sq = p.l1 * p.l1 * s.omega1 * s.omega1;
        const double v2sq = p.l2 * p.l2 * s.omega2 * s.omega2
            + v1sq
            + 2.0 * p.l1 * p.l2 * s.omega1 * s.omega2
              * std::cos(s.theta1 - s.theta2);
        const double KE = 0.5 * p.m1 * v1sq + 0.5 * p.m2 * v2sq;
        const double PE = -(p.m1 + p.m2) * p.g * p.l1 * std::cos(s.theta1)
                          - p.m2 * p.g * p.l2 * std::cos(s.theta2);
        return KE + PE;
    }
    std::string describe() const { return "Euclidean · Lagrangian · rigid joint"; }

private:
    Simulation sim{{}, {}};

    static PendulumState default_initial() {
        return {2.0, 1.8, 0.0, 0.0};
    }
};
