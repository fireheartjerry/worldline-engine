#pragma once
#include "Universe.hpp"
#include "../seed/UniverseAxioms.hpp"
#include "../seed/AxiomDeriver.hpp"
#include "PendulumState.hpp"
#include "PendulumParams.hpp"
#include "PendulumDerivatives.hpp"
#include "../math/Integrators.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

// ============================================================================
// MemoryPendulum — double pendulum whose equations of motion carry a weighted
// history integral (integro-differential equation).
//
// Physics
// -------
// The standard Lagrangian angular accelerations (α₁, α₂) are modified by a
// memory correction term M that depends on the recent history of (ω₁, ω₂):
//
//   α̃₁(t) = α₁(t)  +  λ · ∫₀^τ  κ(s) · ω₁(t - s) ds
//   α̃₂(t) = α₂(t)  +  λ · ∫₀^τ  κ(s) · ω₂(t - s) ds
//
// where:
//   τ   = memory_depth  (seconds; from axioms)
//   κ(s) = exp(-s / τ)  (exponential kernel — decays towards the past)
//   λ   = memory_decay  (from AxiomDeriver; sets kernel amplitude)
//
// The integral is approximated by a running exponential moving average stored
// in a fixed-size circular buffer of past ω values.
//
// Implementation
// --------------
// History buffer capacity MAX_HISTORY = 1024 samples.  Older samples are
// silently dropped when the buffer is full (no heap reallocation).
// The exponential kernel is evaluated analytically per sample using the
// stored timestamp offsets.
// ============================================================================

class MemoryPendulum : public Universe {
public:
    explicit MemoryPendulum(const Worldline::UniverseAxioms& ax) {
        const Worldline::DerivedPhysicsParams dp =
            Worldline::derive_physics_params(ax);
        params       = dp.pendulum;
        memory_depth = dp.memory_depth;
        memory_decay = dp.memory_decay;

        state.theta1 = 2.0;
        state.theta2 = 1.8;
        state.omega1 = 0.0;
        state.omega2 = 0.0;

        buf_head  = 0;
        buf_count = 0;
        t_total   = 0.0;
    }

    void step(double dt) override {
        // Standard Lagrangian derivatives
        PendulumState deriv = pendulum_derivatives(state, params);

        // Memory correction — weighted sum of past angular velocities
        const double m1_correction = memory_integral(dt, 0);  // channel: omega1
        const double m2_correction = memory_integral(dt, 1);  // channel: omega2

        // Apply correction to accelerations
        deriv.omega1 += m1_correction;
        deriv.omega2 += m2_correction;

        // Record current omegas into the history buffer BEFORE stepping
        push_history(state.omega1, state.omega2, t_total);

        // Euler step (memory term makes fully implicit RK4 expensive)
        state = state + deriv * dt;
        t_total += dt;
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
            0.0,
            0.0,
            false
        };
    }

    std::string describe() const override {
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "Euclidean \xC2\xB7 Memory integro (%.1f s) \xC2\xB7 rigid joint",
                      memory_depth);
        return buf;
    }

private:
    static constexpr std::size_t MAX_HISTORY = 1024;

    struct HistoryEntry {
        double omega1;
        double omega2;
        double time;
    };

    PendulumParams params;
    PendulumState  state;
    double         memory_depth;
    double         memory_decay;
    double         t_total;

    std::array<HistoryEntry, MAX_HISTORY> history{};
    std::size_t buf_head  = 0;
    std::size_t buf_count = 0;

    void push_history(double w1, double w2, double t) {
        history[buf_head] = {w1, w2, t};
        buf_head = (buf_head + 1) % MAX_HISTORY;
        if (buf_count < MAX_HISTORY) ++buf_count;
    }

    // Approximate  λ · ∫₀^τ exp(-s/τ) · omega_channel(t-s) ds
    // by summing over the circular buffer entries.
    double memory_integral(double /*dt*/, int channel) const {
        if (buf_count == 0 || memory_depth <= 0.0) return 0.0;

        double sum    = 0.0;
        double weight = 0.0;

        for (std::size_t i = 0; i < buf_count; ++i) {
            // Index from newest to oldest
            const std::size_t idx =
                (buf_head + MAX_HISTORY - 1 - i) % MAX_HISTORY;
            const HistoryEntry& e = history[idx];
            const double lag = t_total - e.time;
            if (lag > memory_depth) break;

            const double k = std::exp(-lag * memory_decay);
            const double w = (channel == 0) ? e.omega1 : e.omega2;
            sum    += k * w;
            weight += k;
        }

        if (weight < 1e-12) return 0.0;
        // Normalise and scale — memory_decay controls the kernel amplitude.
        return memory_decay * 0.05 * sum / weight;
    }
};
