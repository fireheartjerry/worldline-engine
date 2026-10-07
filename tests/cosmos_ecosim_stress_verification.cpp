// Ecosystem generation + live dynamics under stress: structural invariants of
// generated communities across a grid of extreme biomes and many seeds, and
// boundedness of step_community / the EcoSim fixed-timestep driver for hostile
// time steps, seasonal forcing, clock settings and a community swapped in
// without init_live().

#include "cosmos/EcoSim.hpp"
#include "cosmos/Ecosystem.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_ecosim_stress_verification failed: " << message << '\n';
        ++g_failures;
    }
}

const double kNaN = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

// Every population inside the documented basin [1e-6 * xeq, 8 * xeq].
bool in_basin(const eco::Community& c) {
    for (const eco::Species& s : c.species) {
        if (!std::isfinite(s.x) || s.x < 1.0e-6 * s.xeq * (1.0 - 1e-12) || s.x > 8.0 * s.xeq * (1.0 + 1e-12))
            return false;
    }
    return true;
}

std::string describe(const eco::BiomeParams& b, std::uint64_t seed) {
    return "seed " + std::to_string(seed) + " T=" + std::to_string(b.temp_c) + " P=" +
           std::to_string(b.precip_mm) + " NPP=" + std::to_string(b.npp) + (b.aquatic ? " aquatic" : "");
}

// Structural invariants every generated community must satisfy.
void check_structure(const eco::Community& c, const std::string& where) {
    const int S = static_cast<int>(c.species.size());
    check(S >= 1 && S <= 40, where + ": species count in [1, 40] (got " + std::to_string(S) + ")");
    check(c.stats.n_species == S, where + ": stats.n_species matches");
    check(c.stats.n_links == static_cast<int>(c.links.size()), where + ": stats.n_links matches");
    check(c.stats.keystone >= -1 && c.stats.keystone < S, where + ": keystone index in range");
    int producers = 0;
    for (const eco::Species& s : c.species) {
        const bool finite = std::isfinite(s.t.m) && std::isfinite(s.t.tau) && std::isfinite(s.mass_kg) &&
                            std::isfinite(s.flux) && std::isfinite(s.density) && std::isfinite(s.biomass) &&
                            std::isfinite(s.population) && std::isfinite(s.x) && std::isfinite(s.xeq);
        check(finite, where + ": every species field is finite");
        check(s.xeq > 0.0 && s.x == s.xeq, where + ": starts at a positive equilibrium");
        check(s.t.tau >= 0.0 && s.t.tau < 10.0, where + ": trophic level is sane");
        check(!s.name.empty(), where + ": every species is named");
        if (s.t.tau < 0.5) ++producers;
    }
    check(producers >= 1, where + ": at least one producer");

    // Links reference valid species, predators are consumers, and each
    // predator's diet preferences are normalized.
    std::vector<double> pref_sum(static_cast<std::size_t>(S), 0.0);
    for (const eco::Link& l : c.links) {
        const bool valid = l.pred >= 0 && l.pred < S && l.prey >= 0 && l.prey < S;
        check(valid, where + ": link indices in range");
        if (!valid) return;
        check(l.pref > 0.0 && l.pref <= 1.0 + 1e-9 && std::isfinite(l.alpha) && l.alpha > 0.0,
              where + ": link preference/strength sane");
        check(c.species[static_cast<std::size_t>(l.pred)].t.tau >= 0.5, where + ": producers do not prey");
        pref_sum[static_cast<std::size_t>(l.pred)] += l.pref;
    }
    for (int i = 0; i < S; ++i) {
        const double p = pref_sum[static_cast<std::size_t>(i)];
        check(p == 0.0 || std::abs(p - 1.0) < 1e-9, where + ": diet preferences sum to 1");
    }

    // The CSR adjacency mirrors the link list exactly.
    check(c.csr_off.size() == static_cast<std::size_t>(S) + 1, where + ": csr_off has S+1 entries");
    if (c.csr_off.size() != static_cast<std::size_t>(S) + 1) return;
    check(c.csr_off.front() == 0 && c.csr_off.back() == static_cast<int>(c.links.size()),
          where + ": csr_off spans the link list");
    bool csr_ok = c.csr_prey.size() == c.links.size() && c.csr_alpha.size() == c.links.size() &&
                  c.csr_inv_xeq.size() == c.links.size();
    for (int i = 0; csr_ok && i < S; ++i) {
        const int e0 = c.csr_off[static_cast<std::size_t>(i)];
        const int e1 = c.csr_off[static_cast<std::size_t>(i) + 1];
        if (e1 < e0) { csr_ok = false; break; }
        for (int e = e0; e < e1; ++e) {
            const eco::Link& l = c.links[static_cast<std::size_t>(e)];
            if (l.pred != i || l.prey != c.csr_prey[static_cast<std::size_t>(e)] ||
                l.alpha != c.csr_alpha[static_cast<std::size_t>(e)])
                csr_ok = false;
        }
    }
    check(csr_ok, where + ": CSR adjacency matches the link list");
}

void test_generation_across_extreme_biomes() {
    const double temps[] = {-80.0, -20.0, 15.0, 40.0, 90.0};
    const double precips[] = {0.0, 250.0, 2500.0, 12000.0};
    const double npps[] = {-50.0, 0.0, 1.0, 90.0, 1200.0, 5000.0, 1.0e6};
    int built = 0;
    for (double t : temps) {
        for (double p : precips) {
            for (double n : npps) {
                for (bool aquatic : {false, true}) {
                    eco::BiomeParams b;
                    b.temp_c = t;
                    b.precip_mm = p;
                    b.npp = n;
                    b.aquatic = aquatic;
                    const std::uint64_t seed = 1000003ull * static_cast<std::uint64_t>(++built);
                    const eco::Community c = eco::generate_community(seed, b);
                    check_structure(c, describe(b, seed));
                    if (g_failures > 20) return; // keep the log readable
                }
            }
        }
    }
}

void test_generation_is_deterministic() {
    eco::BiomeParams b;
    b.temp_c = 22.0;
    b.precip_mm = 1800.0;
    b.npp = 1500.0;
    for (std::uint64_t seed : {1ull, 2ull, 0xFFFFFFFFFFFFFFFFull, 0ull}) {
        const eco::Community a = eco::generate_community(seed, b);
        const eco::Community c = eco::generate_community(seed, b);
        bool same = a.species.size() == c.species.size() && a.links.size() == c.links.size();
        for (std::size_t i = 0; same && i < a.species.size(); ++i) {
            same = a.species[i].name == c.species[i].name && a.species[i].xeq == c.species[i].xeq &&
                   a.species[i].t.tau == c.species[i].t.tau;
        }
        check(same, "generate_community must be deterministic for seed " + std::to_string(seed));
    }
}

void test_step_community_hostile_inputs() {
    eco::BiomeParams b;
    b.npp = 2400.0;
    eco::Community c = eco::generate_community(99ull, b);
    const double dts[] = {0.0, -1.0, 1.0e-9, 0.016, 0.05, 10.0, 1.0e300, kInf, -kInf, kNaN};
    const double seasons[] = {1.0, 0.0, -5.0, 0.5, 2.0, 50.0, kInf, -kInf, kNaN};
    for (double dt : dts) {
        for (double season : seasons) {
            for (int k = 0; k < 40; ++k) eco::step_community(c, dt, season);
            if (!in_basin(c)) {
                check(false, "step_community must stay in the bounded basin for dt=" + std::to_string(dt) +
                                 " season=" + std::to_string(season));
                return;
            }
        }
    }
    // A long, strongly forced run.
    for (int k = 0; k < 20000; ++k) eco::step_community(c, 0.05, 1.0 + 0.95 * std::sin(k * 0.01));
    check(in_basin(c), "a long forced run stays in the bounded basin");

    eco::Community empty;
    eco::step_community(empty, 0.05, 1.0); // must be a no-op, not a crash
    check(empty.species.empty(), "stepping an empty community is a no-op");
}

void test_live_sim_hostile_clock() {
    eco::BiomeParams b;
    b.npp = 900.0;
    const eco::Community base = eco::generate_community(7ull, b);

    struct Config {
        double H, period, amp;
        int max_substeps;
    };
    const Config configs[] = {
        {1.0 / 120.0, 30.0, 0.3, 6},  {1.0 / 120.0, 0.0, 0.3, 6},   {1.0 / 120.0, -5.0, 5.0, 6},
        {1.0 / 120.0, kNaN, kNaN, 6}, {0.0, 30.0, 0.3, 6},          {-1.0, 30.0, 0.3, 6},
        {kNaN, 30.0, 0.3, 6},         {1.0 / 120.0, 30.0, 0.3, 0},  {1.0 / 120.0, 30.0, 0.3, -4},
        {0.5, 1.0e-12, 0.95, 1000},
    };
    const double frames[] = {1.0 / 60.0, 0.0, -1.0, kNaN, kInf, 1.0e9, 1.0 / 144.0};
    int idx = 0;
    for (const Config& cfg : configs) {
        ecosim::LiveSim sim;
        ecosim::init_live(sim, base);
        sim.clock.H = cfg.H;
        sim.clock.max_substeps = cfg.max_substeps;
        sim.season_period = cfg.period;
        sim.season_amp = cfg.amp;
        bool ok = true;
        for (int f = 0; f < 600 && ok; ++f) {
            const int n = ecosim::advance(sim, frames[static_cast<std::size_t>(f) % 7]);
            ok = n >= 0 && n <= std::max(0, cfg.max_substeps) && in_basin(sim.community) &&
                 std::isfinite(sim.clock.accumulator) && std::isfinite(sim.clock.sim_time) &&
                 sim.clock.accumulator > -1.0e-9;
            const double a = ecosim::interp_alpha(sim);
            ok = ok && a >= 0.0 && a <= 1.0;
            for (int s = 0; ok && s < static_cast<int>(sim.community.species.size()); ++s) {
                ok = std::isfinite(ecosim::render_x(sim, s));
            }
        }
        check(ok, "LiveSim must stay bounded and finite for clock config #" + std::to_string(idx));
        ++idx;
    }
}

void test_live_sim_community_swap() {
    // Swapping a larger community in without init_live() must not index past
    // the per-species buffers (ASan would flag a heap-buffer-overflow).
    eco::BiomeParams poor;
    poor.npp = 5.0;
    eco::BiomeParams rich;
    rich.npp = 5000.0;
    ecosim::LiveSim sim;
    ecosim::init_live(sim, eco::generate_community(1ull, poor));
    const std::size_t small = sim.community.species.size();
    sim.community = eco::generate_community(2ull, rich);
    const std::size_t large = sim.community.species.size();
    check(large > small, "test setup: the swapped-in community must be larger");
    for (int s = 0; s < static_cast<int>(large); ++s) {
        check(std::isfinite(ecosim::render_x(sim, s)), "render_x before any step must be finite");
    }
    for (int f = 0; f < 120; ++f) ecosim::advance(sim, 1.0 / 60.0);
    check(sim.history.size() == large && sim.x_prev.size() == large, "buffers follow the swapped community");
    check(in_basin(sim.community), "swapped community stays bounded");
    check(ecosim::render_x(sim, -1) == 0.0 && ecosim::render_x(sim, static_cast<int>(large)) == 0.0,
          "out-of-range species index renders as 0");
}

} // namespace

int main() {
    test_generation_across_extreme_biomes();
    test_generation_is_deterministic();
    test_step_community_hostile_inputs();
    test_live_sim_hostile_clock();
    test_live_sim_community_swap();
    if (g_failures > 0) {
        std::cerr << g_failures << " ecosim stress check(s) failed\n";
        return EXIT_FAILURE;
    }
    return 0;
}
