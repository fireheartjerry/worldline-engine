// QuantumStatistics.hpp -- how quanta fill states and cross classically forbidden
// barriers: the three occupation statistics (Fermi-Dirac, Bose-Einstein,
// Maxwell-Boltzmann), quantum tunnelling (WKB), bound-state energies of the
// particle-in-a-box, the Gamow factor that lets stars fuse, and the degeneracy
// pressure that holds up white dwarfs and neutron stars.
//
// Header-only, pure, deterministic. SI units unless a name says otherwise.
//
// Sources:
//   - Fermi-Dirac / Bose-Einstein / Maxwell-Boltzmann occupation numbers.
//   - WKB transmission through a barrier: T ~ exp(-2 integral kappa dx).
//   - Particle in a 1-D box: E_n = n^2 pi^2 hbar^2 / (2 m L^2).
//   - Gamow (1928) factor for Coulomb-barrier tunnelling: P ~ exp(-2 pi eta).
//   - Non-relativistic degeneracy pressure of a fermion gas
//       P = (3 pi^2)^(2/3) hbar^2 n^(5/3) / (5 m).

#ifndef COSMOS_QUANTUMSTATISTICS_HPP
#define COSMOS_QUANTUMSTATISTICS_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace qstat {

using namespace cosmos::constants;

// ---------------------------------------------------------------------------
// Occupation statistics -- average number of particles in a state of energy E
// at temperature T with chemical potential mu.
// ---------------------------------------------------------------------------

// Fermi-Dirac: <n> = 1/(e^((E-mu)/kT) + 1), in [0,1] (Pauli exclusion). At E=mu
// the occupation is exactly 1/2.
inline double fermi_dirac(double E_J, double mu_J, double T_K) {
    if (T_K <= 0.0)
        return E_J < mu_J ? 1.0 : (E_J > mu_J ? 0.0 : 0.5);
    const double x = (E_J - mu_J) / (kB * T_K);
    return 1.0 / (std::exp(x) + 1.0);
}

// Bose-Einstein: <n> = 1/(e^((E-mu)/kT) - 1), unbounded above (condensation).
// Requires E > mu.
inline double bose_einstein(double E_J, double mu_J, double T_K) {
    if (T_K <= 0.0)
        return 0.0;
    const double x = (E_J - mu_J) / (kB * T_K);
    if (x <= 0.0)
        return INFINITY;
    return 1.0 / (std::exp(x) - 1.0);
}

// Maxwell-Boltzmann (classical limit): <n> = e^(-(E-mu)/kT).
inline double maxwell_boltzmann(double E_J, double mu_J, double T_K) {
    if (T_K <= 0.0)
        return 0.0;
    const double x = (E_J - mu_J) / (kB * T_K);
    return std::exp(-x);
}

// ---------------------------------------------------------------------------
// Quantum tunnelling -- the WKB transmission coefficient.
// ---------------------------------------------------------------------------

// Transmission probability through a rectangular barrier of height V and width L
// for a particle of mass m and energy E < V:
//   T ~ exp(-2 L sqrt(2 m (V - E)) / hbar).
// Returns 1 for E >= V (over the top, no suppression in this leading form).
inline double tunnel_probability(double mass_kg, double E_J, double V_J, double width_m) {
    if (E_J >= V_J)
        return 1.0;
    if (mass_kg <= 0.0 || width_m <= 0.0)
        return 1.0;
    const double kappa = std::sqrt(2.0 * mass_kg * (V_J - E_J)) / hbar;
    return std::exp(-2.0 * kappa * width_m);
}

// ---------------------------------------------------------------------------
// Particle in a box -- the simplest quantised bound system.
// ---------------------------------------------------------------------------

// Energy of level n (n >= 1) in a 1-D infinite well of width L:
//   E_n = n^2 pi^2 hbar^2 / (2 m L^2).
inline double particle_in_box_energy_J(int n, double mass_kg, double width_m) {
    if (n < 1 || mass_kg <= 0.0 || width_m <= 0.0)
        return 0.0;
    const double nn = static_cast<double>(n);
    return nn * nn * pi * pi * hbar * hbar / (2.0 * mass_kg * width_m * width_m);
}

// ---------------------------------------------------------------------------
// Gamow factor -- Coulomb-barrier penetration that powers stellar fusion.
// ---------------------------------------------------------------------------

// Sommerfeld parameter eta = Z1 Z2 e^2 / (4 pi eps0 hbar v) = Z1 Z2 alpha c / v.
inline double sommerfeld_eta(int Z1, int Z2, double relative_velocity_ms) {
    if (relative_velocity_ms <= 0.0)
        return INFINITY;
    return static_cast<double>(Z1 * Z2) * alpha * c / relative_velocity_ms;
}

// Gamow tunnelling probability P ~ exp(-2 pi eta): the exponential sensitivity of
// fusion to temperature (via velocity) and charge lives here.
inline double gamow_factor(int Z1, int Z2, double relative_velocity_ms) {
    return std::exp(-2.0 * pi * sommerfeld_eta(Z1, Z2, relative_velocity_ms));
}

// ---------------------------------------------------------------------------
// Degeneracy pressure -- what holds up a white dwarf or neutron star.
// ---------------------------------------------------------------------------

// Non-relativistic Fermi gas pressure for number density n [1/m^3] of fermions
// of mass m: P = (3 pi^2)^(2/3) hbar^2 n^(5/3) / (5 m). [Pa]
inline double degeneracy_pressure_Pa(double number_density, double fermion_mass_kg) {
    if (number_density <= 0.0 || fermion_mass_kg <= 0.0)
        return 0.0;
    const double pref = std::pow(3.0 * pi * pi, 2.0 / 3.0) * hbar * hbar / (5.0 * fermion_mass_kg);
    return pref * std::pow(number_density, 5.0 / 3.0);
}

// Fermi energy E_F = hbar^2 (3 pi^2 n)^(2/3) / (2 m). [J]
inline double fermi_energy_J(double number_density, double fermion_mass_kg) {
    if (number_density <= 0.0 || fermion_mass_kg <= 0.0)
        return 0.0;
    return hbar * hbar * std::pow(3.0 * pi * pi * number_density, 2.0 / 3.0) /
           (2.0 * fermion_mass_kg);
}

} // namespace qstat
} // namespace cosmos

#endif // COSMOS_QUANTUMSTATISTICS_HPP
