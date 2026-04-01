#pragma once
#include "Universe.hpp"
#include "../seed/UniverseAxioms.hpp"
#include "../seed/AxiomDeriver.hpp"
#include "PendulumState.hpp"
#include "PendulumParams.hpp"
#include "PendulumDerivatives.hpp"
#include "../math/Integrators.hpp"
#include <cmath>
#include <cstdint>
#include <string>

// ============================================================================
// StochasticPendulum — Lagrangian double pendulum with additive Langevin noise.
//
// Equation of motion
// ------------------
//   dω₁ = α₁(θ, ω) dt + σ dW₁
//   dω₂ = α₂(θ, ω) dt + σ dW₂
//
// where σ = noise_amplitude from the axioms and dWᵢ are independent Wiener
// increments approximated as σ * √dt * ξ with ξ ~ N(0,1).
//
// The normal distribution is approximated via the Box-Muller transform seeded
// from the axiom seed for full determinism.  No std::mt19937 is used; the
// entire PRNG is a self-contained xorshift64 seeded at construction time.
//
// Energy is NOT conserved (noise_amplitude > 0 injects energy stochastically).
// Conservation flags are updated by derive_framework accordingly.
//
// Itô convention is used: the noise is applied AFTER the deterministic step,
// not inside the RK4 stages.  This matches the standard Euler-Maruyama scheme
// and is sufficient for visual purposes.
// ============================================================================

class StochasticPendulum : public Universe {
public:
    explicit StochasticPendulum(const Worldline::UniverseAxioms& ax) {
        const Worldline::DerivedPhysicsParams dp =
            Worldline::derive_physics_params(ax);
        params    = dp.pendulum;
        sigma     = dp.noise_amplitude;
        rng_state = ax.seed ^ 0xDEADBEEFCAFEBABEull;
        if (rng_state == 0) rng_state = 1;

        state.theta1 = 2.0;
        state.theta2 = 1.8;
        state.omega1 = 0.0;
        state.omega2 = 0.0;
    }

    void step(double dt) override {
        // Deterministic RK4 step
        state = Integrators::rk4(state, params, dt, pendulum_derivatives);

        // Euler-Maruyama noise injection (Itô)
        const double sqrt_dt = std::sqrt(dt);
        state.omega1 += sigma * sqrt_dt * normal();
        state.omega2 += sigma * sqrt_dt * normal();
    }

    RenderState extract() const override {
        const Vec2 b1 = {
            params.l1 * std::sin(state.theta1),
            params.l1 * std::cos(state.theta1)
        };
        const Vec2 b2 = {
            b1.x + params.l2 * std::sin(state.theta2),
            b1.y + params.l2 * std::cos(state.theta2)
        };
        return RenderState{
            {0.0, 0.0},
            b1,
            b2,
            state.omega2,
            0.0,   // energy not conserved; omit
            0.0,
            false
        };
    }

    std::string describe() const override {
        return "Euclidean · Stochastic Langevin · rigid joint";
    }

private:
    PendulumParams params;
    PendulumState  state;
    double         sigma;        // noise std-dev per √s
    uint64_t       rng_state;   // xorshift64 state

    // xorshift64 — period 2⁶⁴-1, fully deterministic
    uint64_t xorshift64() {
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 7;
        rng_state ^= rng_state << 17;
        return rng_state;
    }

    // Box-Muller transform — returns one N(0,1) sample per call,
    // caches the pair to avoid wasted samples.
    double normal() {
        if (has_spare) {
            has_spare = false;
            return spare;
        }
        constexpr double TWO_PI = 6.283185307179586;
        // Map two uint64 values to (0,1]
        const double u1 = (xorshift64() >> 11) * (1.0 / (1ull << 53));
        const double u2 = (xorshift64() >> 11) * (1.0 / (1ull << 53));
        const double r  = std::sqrt(-2.0 * std::log(u1 + 1e-300));
        const double t  = TWO_PI * u2;
        spare      = r * std::cos(t);
        has_spare  = true;
        return       r * std::sin(t);
    }

    double spare     = 0.0;
    bool   has_spare = false;
};
