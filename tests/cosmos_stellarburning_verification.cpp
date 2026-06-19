// Verifies cosmos/StellarBurning.hpp: hydrogen-burning Q-values and neutrino
// losses, the pp/CNO temperature crossover and sensitivities, triple-alpha and
// the advanced burning stages with their ordered ignition temperatures.

#include "cosmos/StellarBurning.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::burning;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_stellarburning_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_stellarburning_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Hydrogen burning ---------------------------------------------------
    close(kQ_pp_chain_mev, 26.73, 1e-9, "pp-chain Q = 26.73 MeV");
    // Heat deposited is Q minus neutrino losses, and less than the full Q.
    check(pp_chain_heat_mev() < kQ_pp_chain_mev, "pp neutrino losses reduce heat");
    check(cno_heat_mev() < pp_chain_heat_mev(), "CNO loses more to neutrinos than pp");
    // CNO is far more temperature-sensitive than the pp-chain.
    check(kCNO_temperature_exponent > kPP_temperature_exponent,
          "CNO steeper in temperature than pp");
    // pp dominates in cool stars, CNO in hot ones.
    check(!cno_dominates(1.5e7), "pp dominates in the cool Sun");
    check(cno_dominates(2.5e7), "CNO dominates in hot massive stars");

    // --- Helium burning -----------------------------------------------------
    close(kQ_triple_alpha_mev, 7.275, 1e-9, "triple-alpha Q = 7.275 MeV");
    // Triple-alpha is even more temperature sensitive than CNO (Hoyle resonance).
    check(kTripleAlpha_temperature_exponent > kCNO_temperature_exponent,
          "triple-alpha steeper than CNO");

    // --- Burning stages -----------------------------------------------------
    check(burning_stage_count() == 6, "six burning stages");
    check(stages_temperature_ordered(), "stages ignite in increasing-temperature order");
    // Hydrogen ignites coolest, silicon hottest.
    const BurningStage *st = burning_stages();
    check(std::string(st[0].name) == "Hydrogen", "first stage is hydrogen");
    check(std::string(st[burning_stage_count() - 1].name) == "Silicon", "last stage is silicon");
    check(st[burning_stage_count() - 1].ignition_T_K > st[0].ignition_T_K,
          "silicon ignites far hotter than hydrogen");
    // Silicon burning ends at the iron peak.
    check(kIronPeakA == 56, "iron peak at A=56");

    // --- Determinism --------------------------------------------------------
    check(pp_chain_heat_mev() == pp_chain_heat_mev(), "burning deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_stellarburning_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_stellarburning_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
