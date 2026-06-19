// Verifies cosmos/AtomicStructure.hpp: hydrogenic energy levels and Z^2 scaling,
// quantum numbers and degeneracies, orbital radii and velocities, the Rydberg
// formula, and the fine-structure scale.

#include "cosmos/AtomicStructure.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::atom;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_atomicstructure_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_atomicstructure_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Energy levels ------------------------------------------------------
    close(energy_level_ev(1), -13.6057, 1e-3, "H ground state -13.6 eV");
    close(energy_level_ev(2), -3.4014, 1e-3, "H n=2 = -3.40 eV");
    // Z^2 scaling: He+ (Z=2) ground state is 4x deeper.
    close(energy_level_ev(1, 2), 4.0 * energy_level_ev(1, 1), 1e-9, "energy ~ Z^2");
    close(ionization_energy_ev(1, 1), 13.6057, 1e-3, "H ionization 13.6 eV");
    close(ionization_energy_ev(1, 2), 54.42, 1e-3, "He+ ionization 54.4 eV");
    // Quantum defect lowers the effective n and deepens the level.
    check(quantum_defect_energy_ev(3, 1.35) < energy_level_ev(3),
          "quantum defect deepens alkali levels");

    // --- Quantum numbers & degeneracy ---------------------------------------
    check(shell_degeneracy(1) == 2 && shell_degeneracy(2) == 8 && shell_degeneracy(3) == 18,
          "shell degeneracy 2n^2");
    check(subshell_capacity(0) == 2 && subshell_capacity(1) == 6 && subshell_capacity(2) == 10,
          "subshell capacity 2(2l+1)");
    check(orbital_count(2) == 5, "d subshell has 5 m_l values");
    check(orbital_letter(0) == 's' && orbital_letter(1) == 'p' && orbital_letter(3) == 'f',
          "orbital letters spdf");
    check(valid_state(2, 1, -1, 1), "(2,1,-1,up) is allowed");
    check(!valid_state(1, 1, 0, 1), "l must be < n");
    check(!valid_state(2, 1, 2, 1), "|m_l| must be <= l");

    // --- Sizes and velocities -----------------------------------------------
    close(orbital_radius_pm(1, 1), 52.918, 1e-3, "H Bohr radius 52.9 pm");
    close(orbital_radius_pm(2, 1) / orbital_radius_pm(1, 1), 4.0, 1e-9, "r ~ n^2");
    check(orbital_radius_pm(1, 2) < orbital_radius_pm(1, 1), "higher Z -> smaller atom");
    // Orbital velocity: ~alpha*c for hydrogen 1s; relativistic for heavy Z.
    close(orbital_velocity_over_c(1, 1), 1.0 / 137.036, 1e-4, "H 1s velocity = alpha c");
    check(orbital_velocity_over_c(1, 80) > 0.5, "Z=80 1s electron is relativistic");

    // --- Rydberg formula ----------------------------------------------------
    // Balmer-alpha (3->2) = 656.3 nm; Lyman-alpha (2->1) = 121.6 nm.
    close(transition_wavelength_nm(2, 3), 656.3, 3e-3, "Balmer-alpha 656 nm");
    close(transition_wavelength_nm(1, 2), 121.6, 3e-3, "Lyman-alpha 121.6 nm");
    check(transition_wavelength_nm(1, 2) < transition_wavelength_nm(2, 3),
          "Lyman lines bluer than Balmer");
    // Transition energy positive and matches the level difference.
    close(transition_energy_ev(1, 2), energy_level_ev(2) - energy_level_ev(1), 1e-9,
          "transition energy = level difference");
    check(transition_energy_ev(1, 2) > 10.0, "Lyman-alpha ~ 10.2 eV");

    // --- Fine structure -----------------------------------------------------
    // The fine-structure scale is ~alpha^2 below the gross structure, and grows
    // steeply (Z^4) with nuclear charge.
    check(fine_structure_scale_ev(2, 1) < 1e-3 * std::abs(energy_level_ev(2, 1)),
          "fine structure << gross structure");
    check(fine_structure_scale_ev(2, 10) > fine_structure_scale_ev(2, 1),
          "fine structure grows with Z^4");

    // --- Determinism --------------------------------------------------------
    check(energy_level_ev(2, 1) == energy_level_ev(2, 1), "energy deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_atomicstructure_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_atomicstructure_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
