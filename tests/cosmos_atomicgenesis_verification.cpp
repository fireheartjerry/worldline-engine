// Verifies cosmos/AtomicGenesis.hpp: the generation step that turns a law genome
// into a chemical profile. An all-1.0 genome reproduces our universe (alpha~1/137,
// periodic table to ~Z 137, full CHNOPS, 13.6 eV Rydberg, 52.9 pm Bohr radius),
// and EM-coupling drifts cross the real boundaries (table truncation, loss of the
// life elements).

#include "cosmos/AtomicGenesis.hpp"
#include "cosmos/LawGenome.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;
using namespace cosmos::atomgen;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_atomicgenesis_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_atomicgenesis_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
LawGenome ours() {
    return LawGenome{};
}
} // namespace

int main() {
    const AtomicUniverse u = synthesize(ours());

    // --- Our universe: the anchored baseline --------------------------------
    close(u.alpha_eff, 1.0 / 137.036, 1e-3, "alpha ~ 1/137 in our universe");
    check(u.max_stable_Z > 130, "periodic table reaches ~Z 137");
    close(u.rydberg_ev, 13.6057, 1e-3, "Rydberg 13.6 eV");
    close(u.bohr_radius_pm, 52.918, 1e-3, "Bohr radius 52.9 pm");
    close(u.bond_energy_scale, 1.0, 1e-6, "bond-energy scale = 1 in our universe");
    check(u.hydrogen_stable && u.carbon_available, "H and C exist");
    check(u.life_elements_available, "all CHNOPS elements exist");
    check(u.rich_periodic_table && u.full_periodic_table, "full periodic table");
    check(u.distinct_metals_nonmetals, "halogens exist -> ionic chemistry");
    check(u.complexity_score > 0.75, "high chemical complexity");
    check(u.verdict.find("Full chemistry") != std::string::npos, "rich verdict");
    check(is_life_element(6) && is_life_element(8) && is_life_element(16),
          "C, O, S are life elements");
    check(!is_life_element(2) && !is_life_element(26), "He, Fe are not CHNOPS");

    // --- Stronger EM truncates the periodic table ---------------------------
    LawGenome ge = ours();
    ge.coupling_em = 2.0;
    const AtomicUniverse eu = synthesize(ge);
    close(eu.alpha_eff, 2.0 * u.alpha_eff, 1e-12, "alpha scales with coupling_em");
    check(eu.max_stable_Z < u.max_stable_Z, "stronger EM lowers the heaviest element");
    check(eu.rydberg_ev > u.rydberg_ev, "stronger EM deepens atomic binding (Ry ~ alpha^2)");
    check(eu.bohr_radius_pm < u.bohr_radius_pm, "stronger EM shrinks atoms");
    check(!eu.full_periodic_table, "doubled EM truncates below uranium");
    // CHNOPS (heaviest is sulfur, Z=16) still survives at 2x EM.
    check(eu.life_elements_available, "life elements survive at 2x EM");

    // --- Extreme EM kills chemistry -----------------------------------------
    LawGenome gx = ours();
    gx.coupling_em = 10.0; // alpha_eff ~ 0.073 -> max Z ~ 13
    const AtomicUniverse xu = synthesize(gx);
    check(xu.max_stable_Z < 16, "10x EM truncates below sulfur");
    check(!xu.life_elements_available, "no full CHNOPS at 10x EM");
    check(xu.verdict.find("CHNOPS") != std::string::npos ||
              xu.verdict.find("chemistry") != std::string::npos,
          "barren-chemistry verdict");
    check(xu.complexity_score < u.complexity_score, "lower complexity than ours");

    // --- Element count monotonicity -----------------------------------------
    check(u.available_element_count > eu.available_element_count, "fewer elements at higher EM");
    check(u.available_element_count <= 118, "element count capped at 118");

    // --- Determinism --------------------------------------------------------
    const AtomicUniverse u2 = synthesize(ours());
    check(u2.max_stable_Z == u.max_stable_Z && u2.complexity_score == u.complexity_score,
          "synthesis deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_atomicgenesis_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_atomicgenesis_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
