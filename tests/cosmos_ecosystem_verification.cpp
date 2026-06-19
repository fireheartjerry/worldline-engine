// Verifies the continuous trait-based ecosystem engine: determinism, structure
// (producers -> consumers, bounded trophic levels), an energy-consistent
// bottom-heavy biomass pyramid, a valid food web (predators larger than prey),
// richness scaling with productivity, continuous trophic levels, and bounded
// live dynamics.

#include "cosmos/Ecosystem.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::eco;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_ecosystem_verification FAILED: " << what << "\n"; ++g_failures; }
}

BiomeParams rainforest() { return {25.0, 3000.0, 2200.0, 5.0e14, false}; }
BiomeParams desert()     { return {28.0, 80.0,   90.0,   5.0e14, false}; }

bool same(const Community& a, const Community& b) {
    if (a.species.size() != b.species.size() || a.links.size() != b.links.size()) return false;
    for (std::size_t i = 0; i < a.species.size(); ++i) {
        if (a.species[i].name != b.species[i].name) return false;
        if (a.species[i].t.m != b.species[i].t.m) return false;
        if (a.species[i].t.tau != b.species[i].t.tau) return false;
        if (a.species[i].biomass != b.species[i].biomass) return false;
    }
    return true;
}

} // namespace

int main() {
    // --- Determinism ----------------------------------------------------------
    check(same(generate_community(0xABCDull, rainforest()),
               generate_community(0xABCDull, rainforest())),
          "same (seed,biome) -> identical community");
    check(!same(generate_community(1ull, rainforest()),
                generate_community(2ull, rainforest())),
          "different seeds -> different communities");

    // --- Structure, energy pyramid, food web ----------------------------------
    {
        const Community c = generate_community(0x5EEDull, rainforest());
        const int S = static_cast<int>(c.species.size());
        check(S >= 4 && S <= 40, "species count bounded [4,40]");
        check(c.stats.n_producers >= 1, "at least one producer");

        int producers = 0;
        double prod_bio = 0.0, cons_bio = 0.0, max_tau = 0.0;
        for (const Species& s : c.species) {
            check(std::isfinite(s.biomass) && s.biomass > 0.0, "finite positive biomass");
            check(std::isfinite(s.t.tau), "finite trophic level");
            check(s.t.tau <= 5.2, "trophic level capped (Lindeman)");
            max_tau = std::max(max_tau, s.t.tau);
            if (s.t.tau < 0.5) { ++producers; prod_bio += s.biomass; }
            else cons_bio += s.biomass;
        }
        check(producers >= 1, "producers exist");
        check(max_tau >= 1.0, "there are consumers above the producers");
        // Bottom-heavy biomass pyramid (producers dominate standing biomass).
        check(prod_bio > cons_bio, "biomass pyramid is bottom-heavy");

        // Food web: links exist; every predator is strictly larger than its prey.
        check(c.stats.n_links > 0, "food web has links");
        for (const Link& lk : c.links) {
            check(c.species[lk.pred].t.m > c.species[lk.prey].t.m, "predator larger than prey");
            check(lk.pref > 0.0 && lk.pref <= 1.0001, "diet fraction in (0,1]");
        }
        // Connectance in a plausible range.
        check(c.stats.connectance > 0.0 && c.stats.connectance < 0.6, "connectance plausible");
        check(std::isfinite(c.stats.stability_margin), "stability margin finite");
        check(c.stats.keystone >= 0 && c.stats.keystone < S, "a keystone species is identified");
    }

    // --- Continuous trophic levels (omnivory): not just integers --------------
    {
        const Community c = generate_community(0xC0FFEEull, rainforest());
        bool fractional = false;
        for (const Species& s : c.species) {
            const double frac = std::abs(s.t.tau - std::round(s.t.tau));
            if (s.t.tau > 0.5 && frac > 0.08) { fractional = true; break; }
        }
        check(fractional, "trophic levels are continuous (omnivory), not bucketed");
    }

    // --- Richness scales with productivity (species-energy) -------------------
    {
        const Community rich = generate_community(0x77ull, rainforest());
        const Community poor = generate_community(0x77ull, desert());
        check(rich.species.size() > poor.species.size(),
              "a productive biome supports more species than a desert");
    }

    // --- Live dynamics stay bounded (no divergence, no extinction-to-zero) ----
    {
        Community c = generate_community(0xA11FEull, rainforest());
        for (int step = 0; step < 5000; ++step) step_community(c, 0.02);
        for (const Species& s : c.species) {
            check(std::isfinite(s.x) && s.x > 0.0, "population stays finite and positive");
            check(s.x < 1.0e6 * (s.xeq + 1.0e-9), "population does not blow up");
        }
    }

    // --- Robustness soak: thousands of seeds x biomes, many steps, large dt ----
    //     The positivity-preserving integrator must keep every population finite
    //     and inside its bounded basin [floor, 8*xeq] for any world / step size.
    {
        bool all_bounded = true;
        for (std::uint64_t seed = 0; seed < 1500 && all_bounded; ++seed) {
            Community c = generate_community(seed * 0x9E3779B9ull + 1ull,
                                             (seed % 2) ? rainforest() : desert());
            for (int step = 0; step < 400; ++step) step_community(c, 0.05);
            for (const Species& s : c.species) {
                if (!std::isfinite(s.x) || s.x <= 0.0 || s.x > 8.0001 * s.xeq) {
                    all_bounded = false;
                    break;
                }
            }
        }
        check(all_bounded, "soak: every population stays finite and within [floor, 8*xeq]");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_ecosystem_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_ecosystem_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
