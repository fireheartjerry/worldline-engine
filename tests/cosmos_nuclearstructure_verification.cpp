// Verifies cosmos/NuclearStructure.hpp: rotational bands and the R_4/2 rotor /
// vibrator signatures, the rotational constant, vibrational phonon spectra,
// quadrupole deformation sign, and the giant dipole resonance + TRK sum rule.

#include "cosmos/NuclearStructure.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::structure;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nuclearstructure_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_nuclearstructure_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Rotational bands ---------------------------------------------------
    // E(2+) anchors the band; E(4+)/E(2+) = 20/6 = 10/3 for a perfect rotor.
    const double E2 = 0.1; // 100 keV (typical deformed rare earth)
    close(rotational_energy(2, E2), E2, 1e-12, "E(2+) anchors the band");
    close(rotational_energy(4, E2) / rotational_energy(2, E2), 10.0 / 3.0, 1e-9,
          "rotor E(4+)/E(2+) = 10/3");
    close(rotational_energy(6, E2) / rotational_energy(2, E2), 7.0, 1e-9, "E(6+)/E(2+) = 7");
    check(rotational_energy(8, E2) > rotational_energy(6, E2), "band energy rises with J");
    close(R42_rotor(), 10.0 / 3.0, 1e-12, "rotor ratio 3.33");
    close(R42_vibrator(), 2.0, 1e-12, "vibrator ratio 2.0");

    // The rotational constant is a small (keV-scale) positive energy, larger for
    // lighter nuclei (smaller moment of inertia).
    check(rotational_constant_mev(170) > 0.0, "rotational constant positive");
    check(rotational_constant_mev(20) > rotational_constant_mev(238),
          "lighter nucleus -> larger rotational constant");
    check(rotational_constant_mev(170) < 0.1, "rotational constant is keV-scale");

    // --- Collective classification ------------------------------------------
    check(classify_collective(3.33) == Collective::Rotational, "R42=3.33 -> rotor");
    check(classify_collective(2.0) == Collective::Vibrational, "R42=2.0 -> vibrator");
    check(classify_collective(2.6) == Collective::Transitional, "R42=2.6 -> transitional");

    // --- Vibrational spectra ------------------------------------------------
    close(vibrational_energy(2, 0.5), 1.0, 1e-12, "two-phonon = 2 hbar omega");
    check(vibrational_energy(3, 0.5) > vibrational_energy(2, 0.5), "more phonons -> more energy");

    // --- Deformation --------------------------------------------------------
    check(is_prolate(0.3) && !is_oblate(0.3), "beta2>0 is prolate");
    check(is_oblate(-0.2) && !is_prolate(-0.2), "beta2<0 is oblate");
    check(is_spherical(0.0), "beta2=0 is spherical");
    // Intrinsic quadrupole moment has the sign of beta2 and grows with Z.
    check(intrinsic_quadrupole(66, 164, 0.3) > 0.0, "prolate Q0 > 0");
    check(intrinsic_quadrupole(66, 164, -0.3) < 0.0, "oblate Q0 < 0");

    // --- Giant dipole resonance ---------------------------------------------
    // E_GDR ~ 79 A^(-1/3): Pb-208 -> ~13.3 MeV; lighter nuclei resonate higher.
    close(giant_dipole_energy(208), 13.3, 5e-2, "GDR(Pb-208) ~ 13.3 MeV");
    check(giant_dipole_energy(16) > giant_dipole_energy(208), "lighter nucleus -> higher GDR");
    // TRK sum rule scales as N Z / A and is positive.
    check(trk_sum_rule(82, 208) > 0.0, "TRK sum rule positive");
    check(trk_sum_rule(82, 208) > trk_sum_rule(8, 16), "TRK larger for heavier nucleus");

    // --- Determinism --------------------------------------------------------
    check(giant_dipole_energy(208) == giant_dipole_energy(208), "GDR deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nuclearstructure_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nuclearstructure_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
