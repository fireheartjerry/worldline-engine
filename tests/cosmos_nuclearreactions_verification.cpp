// Verifies cosmos/NuclearReactions.hpp: reaction/fusion Q-values, the Coulomb
// barrier, the Gamow peak and tunnelling / S-factor systematics, and fission
// (fissility, barrier, energy release).

#include "cosmos/NuclearReactions.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::reactions;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nuclearreactions_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_nuclearreactions_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Fusion Q-values ----------------------------------------------------
    // Fusing light nuclei toward mid-mass releases energy (the SEMF is reliable
    // for A >= 12, so we test there rather than on D/T where it is not).
    check(fusion_q_mev(6, 12, 6, 12) > 0.0, "C-12 + C-12 -> Mg-24 exothermic");
    check(fusion_q_mev(8, 16, 8, 16) > 0.0, "O-16 + O-16 -> S-32 exothermic");
    // Fusing two iron-group nuclei is endothermic: past the BE/A peak, building
    // heavier nuclei costs energy rather than releasing it.
    check(fusion_q_mev(26, 56, 26, 56) < 0.0, "Fe-56 + Fe-56 is endothermic (past the peak)");

    // --- Coulomb barrier ----------------------------------------------------
    // Higher charges -> higher barrier; p+p is the lowest.
    check(coulomb_barrier_mev(2, 4, 2, 4) > coulomb_barrier_mev(1, 1, 1, 1),
          "alpha-alpha barrier > p-p barrier");
    check(coulomb_barrier_mev(1, 1, 1, 1) > 0.0, "p-p Coulomb barrier positive");
    // p+p barrier is of order ~1 MeV at nuclear contact.
    check(coulomb_barrier_mev(1, 1, 1, 1) > 0.3 && coulomb_barrier_mev(1, 1, 1, 1) < 3.0,
          "p-p barrier ~ order 1 MeV");

    // --- Gamow peak ---------------------------------------------------------
    // Hotter plasma -> higher Gamow peak energy.
    check(gamow_peak_mev(1, 1, 469.0, 2.0e7) > gamow_peak_mev(1, 1, 469.0, 1.0e7),
          "Gamow peak rises with temperature");
    // Higher charges -> higher Gamow peak (bigger barrier).
    check(gamow_peak_mev(2, 2, 469.0, 1.5e7) > gamow_peak_mev(1, 1, 469.0, 1.5e7),
          "Gamow peak rises with charge");
    // Tunnelling probability increases with energy and decreases with charge.
    check(gamow_tunnelling(1, 1, 0.1, 469.0) > gamow_tunnelling(1, 1, 0.05, 469.0),
          "tunnelling rises with energy");
    check(gamow_tunnelling(2, 2, 0.1, 469.0) < gamow_tunnelling(1, 1, 0.1, 469.0),
          "tunnelling falls with charge");
    // S-factor cross section is positive and rises with energy at fixed S.
    check(cross_section_from_S(1.0, 1, 1, 0.1, 469.0) >
              cross_section_from_S(1.0, 1, 1, 0.05, 469.0),
          "S-factor cross section rises with energy");

    // --- Fission ------------------------------------------------------------
    // Fissility: U-238 has Z^2/A ~ 35.6 -> x ~ 0.70.
    close(fissility(92, 238), (92.0 * 92 / 238) / kFissilityCritical, 1e-9, "fissility formula");
    close(fissility(92, 238), 0.70, 5e-2, "U-238 fissility ~ 0.70");
    check(fissility(92, 238) < 1.0, "U-238 below spontaneous-fission threshold");
    check(fissility(110, 270) > fissility(92, 238), "heavier/charged -> higher fissility");
    // Barrier vanishes as x -> 1; lighter nuclei have a tall barrier.
    check(fission_barrier_factor(92, 238) > 0.0, "U-238 has a fission barrier");
    check(fission_barrier_factor(50, 120) > fission_barrier_factor(92, 238),
          "lighter nucleus has a taller fission barrier");
    // Fission of a heavy nucleus releases a large positive energy (~150-200 MeV).
    check(fission_energy_release_mev(92, 236) > 150.0, "U-236 fission releases > 150 MeV");

    // --- Determinism --------------------------------------------------------
    check(fissility(92, 238) == fissility(92, 238), "fissility deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nuclearreactions_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nuclearreactions_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
