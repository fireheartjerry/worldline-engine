#pragma once
// Fixed-timestep ecosystem simulation DRIVER — the engine-side of the live sim
// (no UI / no raylib). It decouples the population dynamics from the frame rate:
// frame time is accumulated and consumed in fixed sim-steps of size H, so the
// simulation advances by the same number of deterministic steps regardless of
// how the wall-clock frames are sized (FPS-independent). Between sim-steps the
// previous and current states are linearly interpolated for smooth rendering,
// and a per-species ring buffer records the observable population history.
//
// Everything here is a pure, deterministic function of (community, frame_dt
// sequence): identical inputs -> bit-identical state. The accumulator is clamped
// and the substep count is capped, so a long pause (or a single huge frame_dt)
// can never trigger a "spiral of death".

#include "cosmos/Ecosystem.hpp" // eco::Community, eco::Species, step_community
#include "cosmos/Phenology.hpp" // pheno::* (seasonal helpers, optional)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cosmos {
namespace ecosim {

// Fixed-size O(1) ring buffer of population samples. Oldest sample is at(0).
struct PopRing {
    static constexpr int N = 256;
    std::array<float, 256> v{};
    int head = 0;   // index of the next write (one past the newest)
    int filled = 0; // number of valid samples in [0, N]

    void push(float x) {
        v[head] = x;
        head = (head + 1) % N;
        if (filled < N) ++filled;
    }

    // i = 0 is the oldest retained sample, i = filled-1 the newest.
    float at(int i) const {
        if (filled <= 0) return 0.0f;
        if (i < 0) i = 0;
        if (i >= filled) i = filled - 1;
        // When not yet full, samples live in [0, filled); when full, the oldest
        // sample is at `head` (the slot about to be overwritten).
        const int base = (filled < N) ? 0 : head;
        return v[(base + i) % N];
    }
};

// The simulation's fixed-timestep clock. H is the fixed sim-step (seconds of
// sim time); the accumulator banks unconsumed frame time across frames.
struct SimClock {
    double H = 1.0 / 120.0; // fixed sim-step
    double accumulator = 0.0;
    double sim_time = 0.0;
    int    max_substeps = 6; // cap per advance() to avoid the spiral of death
};

// The live simulation: a community, its clock, observable history (one ring per
// species), the snapshot of the previous sim-step (for render interpolation),
// and the seasonal forcing parameters.
struct LiveSim {
    eco::Community      community;
    SimClock            clock;
    std::vector<PopRing> history;     // one PopRing per species
    std::vector<double>  x_prev;      // species.x at the start of the latest step
    double               season_period = 30.0; // sim-time units per season cycle
    double               season_amp    = 0.3;   // seasonal carrying-capacity swing
};

// Move the community in and size the per-species history / snapshot buffers.
inline void init_live(LiveSim& sim, eco::Community comm) {
    sim.community = std::move(comm);
    const std::size_t S = sim.community.species.size();
    sim.history.assign(S, PopRing{});
    sim.x_prev.assign(S, 0.0);
    for (std::size_t i = 0; i < S; ++i) sim.x_prev[i] = sim.community.species[i].x;
    sim.clock.accumulator = 0.0;
    sim.clock.sim_time = 0.0;
}

// Advance the fixed-timestep loop by a (variable) frame duration. Returns the
// number of fixed sim-steps actually taken. FPS-independent and deterministic:
// the same total frame time, however it is chunked, produces the same number of
// sim-steps up to a single H remainder banked in the accumulator.
inline int advance(LiveSim& sim, double frame_dt) {
    SimClock& c = sim.clock;
    const double H = (c.H > 1.0e-12) ? c.H : 1.0e-12;

    // Clamp the incoming frame time so a long stall can't bank unbounded work.
    if (!(frame_dt > 0.0)) frame_dt = 0.0;
    const double max_frame = H * static_cast<double>(c.max_substeps);
    if (frame_dt > max_frame) frame_dt = max_frame;
    c.accumulator += frame_dt;

    const std::size_t S = sim.community.species.size();
    int substeps = 0;
    while (c.accumulator >= H && substeps < c.max_substeps) {
        // Snapshot the pre-step state for render interpolation.
        for (std::size_t i = 0; i < S; ++i) sim.x_prev[i] = sim.community.species[i].x;

        // Bounded seasonal forcing: a sinusoid around 1.0, kept strictly positive.
        const double period = (sim.season_period > 1.0e-9) ? sim.season_period : 1.0e-9;
        const double amp = clampd(sim.season_amp, 0.0, 0.95);
        double season_factor = 1.0 + amp * std::sin(kTwoPi * c.sim_time / period);
        season_factor = clampd(season_factor, 1.0e-3, 2.0);

        eco::step_community(sim.community, H, season_factor);

        // Record the observable population history (live state / equilibrium).
        for (std::size_t i = 0; i < S; ++i) {
            sim.history[i].push(static_cast<float>(sim.community.species[i].x));
        }

        c.sim_time += H;
        c.accumulator -= H;
        ++substeps;
    }
    return substeps;
}

// Render-interpolation fraction in [0,1): how far we are into the next sim-step.
inline double interp_alpha(const LiveSim& sim) {
    const double H = (sim.clock.H > 1.0e-12) ? sim.clock.H : 1.0e-12;
    double a = sim.clock.accumulator / H;
    return clampd(a, 0.0, 1.0);
}

// Interpolated population for smooth rendering: lerp(prev, current, alpha).
inline double render_x(const LiveSim& sim, int species_index) {
    if (species_index < 0 ||
        species_index >= static_cast<int>(sim.community.species.size())) {
        return 0.0;
    }
    const std::size_t i = static_cast<std::size_t>(species_index);
    return lerp(sim.x_prev[i], sim.community.species[i].x, interp_alpha(sim));
}

// Sprite-radius pulse fed the interpolated population so it never flickers:
// sqrt(render_x / xeq), clamped to a gentle [0.45, 2.2] band.
inline double smoothed_pulse(double render_x_value, double xeq) {
    const double eps = 1.0e-12;
    const double denom = (xeq > eps) ? xeq : eps;
    double r = render_x_value / denom;
    if (!(r > 0.0)) r = 0.0;
    return clampd(std::sqrt(r), 0.45, 2.2);
}

} // namespace ecosim
} // namespace cosmos
