// Verifies cosmos/Nucleosynthesis.hpp: the generation step that turns a law
// genome into a full nuclear profile. An all-1.0 genome must reproduce our
// universe (iron peak ~56-62, s-process peaks near Sr/Ba/Pb, full periodic
// table), and genome drifts must cross the real anthropic boundaries (Hoyle
// detuning, fission-limit collapse). Plus the cosmic abundance pattern and sites.

#include "cosmos/LawGenome.hpp"
#include "cosmos/Nucleosynthesis.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;
using namespace cosmos::nucleosynth;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nucleosynthesis_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
LawGenome ours() {
    return LawGenome{};
}
} // namespace

int main() {
    const NuclearUniverse u = synthesize(ours());

    // --- Iron peak ----------------------------------------------------------
    check(u.iron_peak_A >= 55 && u.iron_peak_A <= 63, "iron peak at A ~ 56-62");
    check(u.max_binding_per_nucleon > 8.4 && u.max_binding_per_nucleon < 9.0,
          "peak BE/A ~ 8.7 MeV");

    // --- s-process / r-process peaks ----------------------------------------
    // s-process peaks pinned to neutron magic numbers: N=50 -> A~88 (Sr),
    // N=82 -> A~138 (Ba), N=126 -> A~208 (Pb).
    check(u.s_process[0].mass_number_A >= 84 && u.s_process[0].mass_number_A <= 92,
          "s-process N=50 peak near A~88 (Sr)");
    check(u.s_process[1].mass_number_A >= 134 && u.s_process[1].mass_number_A <= 142,
          "s-process N=82 peak near A~138 (Ba)");
    check(u.s_process[2].mass_number_A >= 204 && u.s_process[2].mass_number_A <= 212,
          "s-process N=126 peak near A~208 (Pb)");
    // r-process peaks sit at lower A than the s-process peaks for the same shell.
    for (int i = 0; i < 3; ++i)
        check(u.r_process[i].mass_number_A < u.s_process[i].mass_number_A,
              "r-process peak below s-process peak");

    // --- Full periodic table & viability ------------------------------------
    check(u.carbon_resonance_ok, "Hoyle resonance ok in our universe");
    check(u.can_fuse_to_iron, "fusion reaches the iron peak");
    check(u.valley_of_stability_exists, "valley of stability exists");
    check(u.stable_heavy_elements, "periodic table reaches lead/bismuth");
    check(u.r_process_possible, "r-process heavy synthesis possible");
    check(u.fission_limit_A > 240, "fission limit in the super-heavy region");
    check(u.complexity_score > 0.75, "complexity high for our universe");
    check(u.verdict.find("Full nucleosynthesis") != std::string::npos, "rich verdict");
    check(std::abs(u.primordial_H + u.primordial_He - 1.0) < 1e-9, "H + He = 1");

    // --- Anthropic boundary: Hoyle detuning ---------------------------------
    LawGenome gh = ours();
    gh.coupling_strong = 1.10; // 10% stronger -> resonance detuned
    const NuclearUniverse hu = synthesize(gh);
    check(!hu.carbon_resonance_ok, "10% stronger force detunes the Hoyle resonance");
    check(!hu.can_fuse_to_iron, "no carbon -> no path to iron");
    check(hu.verdict.find("Hoyle") != std::string::npos, "Hoyle verdict");
    check(hu.complexity_score < u.complexity_score, "detuned universe scores lower");

    // --- Anthropic boundary: strong EM lowers the fission limit -------------
    LawGenome ge = ours();
    ge.coupling_em = 2.0; // double the Coulomb repulsion
    const NuclearUniverse eu = synthesize(ge);
    check(eu.fission_limit_A < u.fission_limit_A, "stronger EM lowers the fission limit");

    // --- Cosmic abundance pattern -------------------------------------------
    // Light elements vastly more abundant than heavy ones; even-A favoured;
    // a pronounced iron-peak bump.
    check(cosmic_abundance(1) > cosmic_abundance(100), "light elements more abundant");
    check(cosmic_abundance(12) > cosmic_abundance(13), "even-A (C-12) favoured over odd (13)");
    check(cosmic_abundance(56) > cosmic_abundance(45) &&
              cosmic_abundance(56) > cosmic_abundance(70),
          "iron-peak bump stands above its neighbours");

    // --- Sites --------------------------------------------------------------
    const auto s = sites();
    check(s.size() == 6, "six nucleosynthesis sites");
    check(std::string(s.front().name) == "Big Bang", "first site is the Big Bang");
    check(std::string(s.back().process).find("r-process") != std::string::npos,
          "neutron-star mergers run the r-process");

    // --- Determinism --------------------------------------------------------
    const NuclearUniverse u2 = synthesize(ours());
    check(u2.iron_peak_A == u.iron_peak_A && u2.complexity_score == u.complexity_score,
          "synthesis deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nucleosynthesis_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nucleosynthesis_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
