// Verifies the fixed-timestep ecosystem simulation driver (EcoSim.hpp): the
// PopRing O(1) ring buffer, FPS-independence and determinism of the fixed-step
// loop, render interpolation, the smoothed sprite pulse, bounded long-run
// dynamics under seasonal forcing, and the max-substeps cap (no spiral of
// death). Links worldline_cosmos so eco:: symbols resolve.

#include "cosmos/EcoSim.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;
using namespace cosmos::ecosim;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_ecosim_verification FAILED: " << what << "\n"; ++g_failures; }
}

eco::BiomeParams rainforest() { return {25.0, 3000.0, 2200.0, 5.0e14, false}; }

LiveSim make_sim() {
    LiveSim s;
    init_live(s, eco::generate_community(0xABCDull, rainforest()));
    return s;
}

bool is_finite(double x) { return std::isfinite(x); }

} // namespace

int main() {
    // --- PopRing --------------------------------------------------------------
    {
        PopRing r;
        check(r.filled == 0, "ring starts empty");
        for (int i = 0; i < 300; ++i) r.push(static_cast<float>(i));
        check(r.filled == PopRing::N, "filled caps at N=256");
        // After 300 pushes (0..299), the oldest retained is 300-256 = 44.
        check(r.at(0) == 44.0f, "at(0) is the oldest retained sample");
        check(r.at(r.filled - 1) == 299.0f, "at(filled-1) is the newest sample");
        // Monotone contiguous order across the wrap.
        bool ordered = true;
        for (int i = 1; i < r.filled; ++i) {
            if (r.at(i) != r.at(i - 1) + 1.0f) ordered = false;
        }
        check(ordered, "ring wraps correctly (contiguous oldest->newest order)");

        // Partially-filled ring: oldest is the first pushed.
        PopRing p;
        p.push(7.0f); p.push(8.0f); p.push(9.0f);
        check(p.filled == 3, "partial fill count");
        check(p.at(0) == 7.0f && p.at(2) == 9.0f, "partial-fill ordering");
    }

    // --- FPS independence / determinism --------------------------------------
    {
        const double total = 2.0; // seconds of frame time to deliver
        LiveSim small = make_sim();
        LiveSim big = make_sim();

        // (a) many small frames, (b) fewer large frames — same total time.
        const double small_dt = 1.0 / 240.0;
        const int small_frames = static_cast<int>(std::lround(total / small_dt));
        int small_steps = 0;
        for (int i = 0; i < small_frames; ++i) small_steps += advance(small, small_dt);

        const double big_dt = 1.0 / 30.0;
        const int big_frames = static_cast<int>(std::lround(total / big_dt));
        int big_steps = 0;
        for (int i = 0; i < big_frames; ++i) big_steps += advance(big, big_dt);

        // Completed sim-steps and final sim_time agree within one H (remainder
        // is banked in the accumulator), so the sim is frame-rate independent.
        check(std::abs(small_steps - big_steps) <= 1, "sim-step count matches across frame sizes");
        check(std::abs(small.clock.sim_time - big.clock.sim_time) <= small.clock.H + 1.0e-9,
              "final sim_time matches within one H");

        // Determinism: identical frame_dt sequences -> bit-identical species x.
        LiveSim a = make_sim();
        LiveSim b = make_sim();
        std::vector<double> seq;
        for (int i = 0; i < 500; ++i) seq.push_back(((i % 7) + 1) / 200.0);
        for (double dt : seq) { advance(a, dt); advance(b, dt); }
        check(a.community.species.size() == b.community.species.size(), "same species count");
        bool bit_identical = true;
        for (std::size_t i = 0; i < a.community.species.size(); ++i) {
            if (a.community.species[i].x != b.community.species[i].x) bit_identical = false;
        }
        check(bit_identical, "identical frame_dt sequences -> bit-identical state");
    }

    // --- Interpolation + smoothed pulse --------------------------------------
    {
        LiveSim s = make_sim();
        // Advance a partial step so the accumulator holds a fractional remainder.
        advance(s, s.clock.H * 0.5);
        const double alpha = interp_alpha(s);
        check(alpha >= 0.0 && alpha < 1.0, "interp_alpha in [0,1)");

        // Run a few full steps so x_prev and current differ.
        for (int i = 0; i < 4; ++i) advance(s, s.clock.H);
        advance(s, s.clock.H * 0.3);
        bool render_bounded = true;
        for (std::size_t i = 0; i < s.community.species.size(); ++i) {
            const double lo = std::min(s.x_prev[i], s.community.species[i].x);
            const double hi = std::max(s.x_prev[i], s.community.species[i].x);
            const double rx = render_x(s, static_cast<int>(i));
            const double tol = 1.0e-9 + 1.0e-9 * hi;
            if (rx < lo - tol || rx > hi + tol) render_bounded = false;
        }
        check(render_bounded, "render_x lies between x_prev and current state");

        bool pulse_bounded = true;
        for (std::size_t i = 0; i < s.community.species.size(); ++i) {
            const double rx = render_x(s, static_cast<int>(i));
            const double p = smoothed_pulse(rx, s.community.species[i].xeq);
            if (p < 0.45 || p > 2.2) pulse_bounded = false;
        }
        check(pulse_bounded, "smoothed_pulse in [0.45, 2.2]");
        // Extremes clamp.
        check(smoothed_pulse(0.0, 1.0) == 0.45, "pulse clamps low at 0.45");
        check(smoothed_pulse(1.0e9, 1.0) == 2.2, "pulse clamps high at 2.2");
    }

    // --- Bounded long-run dynamics with seasonal forcing ----------------------
    {
        LiveSim s = make_sim();
        s.season_amp = 0.3;
        s.season_period = 30.0;
        int taken = 0;
        // Drive exactly one substep per advance until 5000 substeps accumulate.
        while (taken < 5000) taken += advance(s, s.clock.H);
        check(taken >= 5000, "ran at least 5000 substeps");
        bool bounded = true;
        for (const auto& sp : s.community.species) {
            if (!is_finite(sp.x)) bounded = false;
            if (sp.x < 1.0e-7 * sp.xeq - 1.0e-12) bounded = false;
            if (sp.x > 9.0 * sp.xeq + 1.0e-9) bounded = false;
        }
        check(bounded, "all species x finite and within [1e-7*xeq, 9*xeq]");
    }

    // --- max_substeps cap (no spiral of death) --------------------------------
    {
        LiveSim s = make_sim();
        const int n = advance(s, 10.0); // a single huge frame
        check(n <= s.clock.max_substeps, "single huge frame_dt capped at max_substeps");
        // Accumulator does not grow without bound across repeated huge frames.
        for (int i = 0; i < 50; ++i) advance(s, 10.0);
        check(s.clock.accumulator < s.clock.H * (s.clock.max_substeps + 1),
              "accumulator stays bounded (no spiral of death)");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_ecosim_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_ecosim_verification: " << g_failures << " check(s) failed\n";
    return 1;
}
