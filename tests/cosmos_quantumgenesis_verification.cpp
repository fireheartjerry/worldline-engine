// Verifies cosmos/QuantumGenesis.hpp: the generation step that turns a law genome
// into a full quantum-tier profile. Checks that an all-1.0 genome reproduces our
// universe (n-p split 1.29 MeV, bound deuteron, unbound di-proton, Y_He ~ 0.25,
// full chemistry), and that drifting the genome crosses the real anthropic
// boundaries (proton decay, di-proton catastrophe, deuteron bottleneck), plus the
// early-universe timeline ordering.

#include "cosmos/LawGenome.hpp"
#include "cosmos/QuantumGenesis.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;
using namespace cosmos::genesis;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_quantumgenesis_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_quantumgenesis_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
LawGenome ours() {
    LawGenome g; // all couplings 1.0, stability_bias 0.5 by default
    return g;
}
} // namespace

int main() {
    // --- Our universe: the anchored baseline --------------------------------
    const QuantumUniverse u = synthesize(ours());
    close(u.np_mass_diff_mev, 1.29, 1e-2, "n-p mass split ~ 1.29 MeV");
    check(u.proton_stable, "proton is stable in our universe");
    check(u.free_neutron_decays, "free neutron decays in our universe");
    check(u.deuteron_bound, "deuteron is bound");
    check(!u.diproton_bound, "di-proton is unbound (no stellar runaway)");
    check(u.hydrogen_forms && u.carbon_stable && u.heavy_atoms_stable,
          "hydrogen, carbon and heavy atoms all stable");
    check(u.bbn_possible && u.chemistry_possible, "BBN and chemistry both possible");
    close(u.primordial_He, 0.247, 1e-2, "primordial helium fraction ~ 0.25");
    close(u.primordial_H + u.primordial_He, 1.0, 1e-9, "H + He fractions sum to 1");
    check(u.max_stable_Z > 130, "max stable Z ~ 137 (1/alpha)");
    check(u.meson_count == 9 && u.baryon_count == 10, "light hadron nonet + decuplet");
    close(u.complexity_score, 0.8, 1e-6, "all gates pass -> score 0.8 at bias 0.5");
    check(u.verdict.find("Complex-matter friendly") != std::string::npos,
          "verdict is complexity-friendly");

    // --- Proton-unstable universe (EM too strong) ---------------------------
    LawGenome gem = ours();
    gem.coupling_em = 4.0; // huge EM self-energy flips the n-p ordering
    const QuantumUniverse pu = synthesize(gem);
    check(!pu.proton_stable, "strong EM -> proton unstable");
    check(!pu.hydrogen_forms, "no stable proton -> no hydrogen");
    check(pu.verdict.find("Proton-unstable") != std::string::npos, "barren verdict");
    check(pu.complexity_score < u.complexity_score, "barren universe scores lower");

    // --- Di-proton catastrophe (strong force a few % stronger) --------------
    LawGenome gs = ours();
    gs.coupling_strong = 1.05;
    const QuantumUniverse dp = synthesize(gs);
    check(dp.diproton_bound, "stronger strong force -> di-proton binds");
    check(!dp.bbn_possible, "bound di-proton breaks normal nucleosynthesis");
    check(dp.verdict.find("Di-proton") != std::string::npos, "di-proton verdict");

    // --- Deuteron bottleneck (strong force a few % weaker) ------------------
    LawGenome gw = ours();
    gw.coupling_strong = 0.90;
    const QuantumUniverse db = synthesize(gw);
    check(!db.deuteron_bound, "weaker strong force -> deuteron unbinds");
    check(!db.bbn_possible, "unbound deuteron closes the BBN bottleneck");
    check(db.verdict.find("Deuteron unbound") != std::string::npos, "deuteron verdict");

    // --- Monotonicity: heavier quarks -> larger n-p split -> less helium -----
    LawGenome gh = ours();
    gh.mass_scale = 1.5;
    const QuantumUniverse hm = synthesize(gh);
    check(hm.np_mass_diff_mev > u.np_mass_diff_mev, "larger mass scale -> larger n-p split");
    check(hm.primordial_He < u.primordial_He, "larger n-p split -> less primordial helium");

    // --- Effective couplings scale with the genome --------------------------
    LawGenome ge = ours();
    ge.coupling_em = 2.0;
    const QuantumUniverse eu = synthesize(ge);
    close(eu.alpha_em_eff, 2.0 * u.alpha_em_eff, 1e-12, "alpha_em scales with coupling_em");
    check(eu.max_stable_Z < u.max_stable_Z, "stronger EM truncates the periodic table");

    // --- Early-universe timeline --------------------------------------------
    const auto tl = cosmic_timeline(ours());
    check(tl.size() == 6, "timeline has six epochs");
    for (std::size_t i = 1; i < tl.size(); ++i) {
        check(tl[i].time_s > tl[i - 1].time_s, "epoch times strictly increase");
        check(tl[i].temperature_K < tl[i - 1].temperature_K, "epoch temperatures cool");
        check(tl[i].energy_GeV < tl[i - 1].energy_GeV, "epoch energies fall");
    }
    check(std::string(tl.front().name) == "Planck", "timeline starts at the Planck era");
    check(tl.back().temperature_K > 1000.0 && tl.back().temperature_K < 1.0e4,
          "recombination ~ a few thousand K");

    // --- Determinism --------------------------------------------------------
    const QuantumUniverse u2 = synthesize(ours());
    check(u2.np_mass_diff_mev == u.np_mass_diff_mev && u2.complexity_score == u.complexity_score,
          "synthesis is deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_quantumgenesis_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_quantumgenesis_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
