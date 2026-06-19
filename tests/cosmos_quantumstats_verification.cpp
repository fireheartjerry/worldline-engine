// Verifies cosmos/QuantumStatistics.hpp: the three occupation statistics and
// their ordering, WKB tunnelling monotonicity, particle-in-a-box level scaling,
// the Gamow factor's sensitivity to charge/velocity, and degeneracy pressure /
// Fermi-energy scaling.

#include "cosmos/Constants.hpp"
#include "cosmos/QuantumStatistics.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::qstat;
using namespace cosmos::constants;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_quantumstats_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_quantumstats_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    const double T = 1.0e4; // 10,000 K
    const double kT = kB * T;
    const double mu = 0.0;

    // --- Occupation statistics ----------------------------------------------
    // Fermi-Dirac is exactly 1/2 at E = mu, and bounded in [0,1].
    close(fermi_dirac(mu, mu, T), 0.5, 1e-9, "FD(E=mu) = 1/2");
    check(fermi_dirac(10.0 * kT, mu, T) < 0.5 && fermi_dirac(-10.0 * kT, mu, T) > 0.5,
          "FD decreases through mu");
    check(fermi_dirac(1.0 * kT, mu, T) <= 1.0, "FD bounded above by 1 (Pauli)");
    // For E > mu: Bose-Einstein > Maxwell-Boltzmann > Fermi-Dirac.
    const double E = 2.0 * kT;
    check(bose_einstein(E, mu, T) > maxwell_boltzmann(E, mu, T), "BE > MB for E>mu");
    check(maxwell_boltzmann(E, mu, T) > fermi_dirac(E, mu, T), "MB > FD for E>mu");
    // The three converge in the classical (dilute) limit E - mu >> kT.
    const double Ehi = 30.0 * kT;
    close(bose_einstein(Ehi, mu, T), maxwell_boltzmann(Ehi, mu, T), 1e-6,
          "BE -> MB in the classical limit");

    // --- Quantum tunnelling (WKB) -------------------------------------------
    const double V = 10.0 * e; // 10 eV barrier
    const double En = 1.0 * e; // 1 eV particle
    const double Pthin = tunnel_probability(electron_mass_kg, En, V, 1.0e-10);
    const double Pthick = tunnel_probability(electron_mass_kg, En, V, 5.0e-10);
    check(Pthin > Pthick, "thinner barrier -> more tunnelling");
    check(Pthin > 0.0 && Pthin < 1.0, "tunnelling probability in (0,1)");
    check(tunnel_probability(electron_mass_kg, 12.0 * e, V, 1.0e-10) == 1.0,
          "E>V -> full transmission (leading order)");
    // Heavier particle tunnels less.
    check(tunnel_probability(proton_mass_kg, En, V, 1.0e-10) <
              tunnel_probability(electron_mass_kg, En, V, 1.0e-10),
          "heavier particle tunnels less");

    // --- Particle in a box --------------------------------------------------
    const double E1 = particle_in_box_energy_J(1, electron_mass_kg, 1.0e-9);
    const double E2 = particle_in_box_energy_J(2, electron_mass_kg, 1.0e-9);
    close(E2 / E1, 4.0, 1e-9, "box levels scale as n^2");
    // Narrower box -> higher ground state (E ~ 1/L^2).
    close(particle_in_box_energy_J(1, electron_mass_kg, 0.5e-9) / E1, 4.0, 1e-9,
          "box energy ~ 1/L^2");

    // --- Gamow factor -------------------------------------------------------
    // p-p (Z=1,1) tunnels more easily than p-He (Z=1,2) at the same velocity.
    check(gamow_factor(1, 1, 2.0e6) > gamow_factor(1, 2, 2.0e6),
          "lower charge -> easier Coulomb tunnelling");
    // Faster (hotter) nuclei tunnel far more readily.
    check(gamow_factor(1, 1, 4.0e6) > gamow_factor(1, 1, 2.0e6),
          "faster nuclei tunnel more (steep T sensitivity)");
    check(gamow_factor(1, 1, 2.0e6) > 0.0 && gamow_factor(1, 1, 2.0e6) < 1.0,
          "Gamow factor in (0,1)");

    // --- Degeneracy pressure ------------------------------------------------
    // P ~ n^(5/3): 8x the density -> 32x the pressure.
    close(degeneracy_pressure_Pa(8.0e30, electron_mass_kg) /
              degeneracy_pressure_Pa(1.0e30, electron_mass_kg),
          32.0, 1e-6, "degeneracy pressure ~ n^(5/3)");
    // Lighter fermions give MORE pressure (electrons hold up white dwarfs).
    check(degeneracy_pressure_Pa(1.0e36, electron_mass_kg) >
              degeneracy_pressure_Pa(1.0e36, proton_mass_kg),
          "electron degeneracy pressure > proton at same n");
    // E_F ~ n^(2/3): 8x density -> 4x Fermi energy.
    close(fermi_energy_J(8.0e30, electron_mass_kg) / fermi_energy_J(1.0e30, electron_mass_kg), 4.0,
          1e-6, "Fermi energy ~ n^(2/3)");

    // --- Determinism --------------------------------------------------------
    check(fermi_dirac(E, mu, T) == fermi_dirac(E, mu, T), "FD deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_quantumstats_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_quantumstats_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
