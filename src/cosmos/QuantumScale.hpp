// QuantumScale.hpp -- header-only, pure, deterministic quantum & Planck-scale
// physics for the SMALLEST tier of the Worldline scale ladder (SUBATOMIC).
//
// This is the "first layer of existence": where classical intuition breaks down
// and length, time and mass are bounded from below by the Planck units. The big
// scales (STELLAR..COSMIC) already have closed-form instruments in CosmoStats.hpp
// and the matter scales have lookup tables in ParticleData.hpp; this module gives
// the quantum floor the same caliber of derived, computed physics.
//
// Every function is a closed-form evaluation of a standard relation. No state, no
// RNG, no allocation. Inputs are SI (kg, m, s, J, K) unless a name says otherwise;
// energies are also offered in MeV where the particle world prefers them.
//
// Sources:
//   - CODATA 2022 fundamental constants (see cosmos/Constants.hpp).
//   - Planck units: l_P=sqrt(hbar G/c^3), t_P=l_P/c, m_P=sqrt(hbar c/G),
//       E_P=m_P c^2, T_P=E_P/k_B (Planck 1899; standard definition).
//   - Compton wavelength lambda_C = h/(m c); de Broglie lambda = h/p (de Broglie 1924).
//   - Heisenberg uncertainty: dx dp >= hbar/2; dE dt >= hbar/2 (Kennard 1927).
//   - Quantum harmonic oscillator E_n = (n+1/2) hbar omega (zero-point at n=0).
//   - Bohr model / hydrogen: a_0 = hbar/(m_e c alpha); E_n = -Ry/n^2,
//       Ry = alpha^2 m_e c^2 / 2 = 13.6057 eV (Bohr 1913).
//   - Decay width <-> lifetime: tau = hbar/Gamma (natural linewidth).
//   - Hawking temperature T_H = hbar c^3 / (8 pi G M k_B) (Hawking 1974); the
//       Compton-Schwarzschild crossover at ~m_P motivates the Planck floor.
//   - Particle lifetimes/widths: PDG 2024 (Review of Particle Physics).

#ifndef COSMOS_QUANTUMSCALE_HPP
#define COSMOS_QUANTUMSCALE_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace quantum {

using namespace cosmos::constants;

// ---------------------------------------------------------------------------
// Energy unit bridges
// ---------------------------------------------------------------------------

// 1 MeV in joules (= 1e6 eV).
inline constexpr double kMeV_J = 1.0e6 * electron_volt_J;

inline double joules_to_ev(double E_J) {
    return E_J / electron_volt_J;
}
inline double ev_to_joules(double E_eV) {
    return E_eV * electron_volt_J;
}
inline double joules_to_mev(double E_J) {
    return E_J / kMeV_J;
}
inline double mev_to_joules(double E_mev) {
    return E_mev * kMeV_J;
}

// ---------------------------------------------------------------------------
// Mass <-> energy (Einstein E = m c^2)
// ---------------------------------------------------------------------------

inline double rest_energy_J(double mass_kg) {
    return mass_kg * c2;
}
inline double rest_energy_mev(double mass_kg) {
    return joules_to_mev(mass_kg * c2);
}
inline double mass_from_energy_mev(double E_mev) {
    return mev_to_joules(E_mev) / c2;
}

// Total relativistic energy E = sqrt((m c^2)^2 + (p c)^2), with momentum in
// SI units (kg m/s). Reduces to the rest energy at p=0 and to p c for m=0.
inline double relativistic_energy_J(double mass_kg, double momentum) {
    const double mc2 = mass_kg * c2;
    const double pc = momentum * c;
    return std::sqrt(mc2 * mc2 + pc * pc);
}

// ---------------------------------------------------------------------------
// Planck units -- the hard floor of scale, derived (not just stored)
// ---------------------------------------------------------------------------
//
// std::sqrt is not constexpr in C++17, so these are runtime helpers. They must
// reproduce the stored constexpr table in Constants.hpp; the verification test
// pins both against each other so neither can silently drift.

struct PlanckUnits {
    double length_m;
    double time_s;
    double mass_kg;
    double energy_J;
    double temperature_K;
};

inline PlanckUnits planck_units() {
    PlanckUnits p;
    p.length_m = std::sqrt(hbar * G / (c * c * c));
    p.time_s = p.length_m / c;
    p.mass_kg = std::sqrt(hbar * c / G);
    p.energy_J = p.mass_kg * c2;
    p.temperature_K = p.energy_J / kB;
    return p;
}

// A length expressed in Planck lengths. Below 1 there is no operational meaning
// of classical distance, so a deterministic generator must never sample there.
inline double in_planck_lengths(double length_m) {
    return length_m / planck_units().length_m;
}

// Whether a length is at or above the Planck floor (physically meaningful).
inline bool above_planck_floor(double length_m) {
    return length_m >= planck_units().length_m;
}

// ---------------------------------------------------------------------------
// Compton & de Broglie wavelengths -- the quantum "size" of a particle
// ---------------------------------------------------------------------------

// Compton wavelength lambda_C = h/(m c): the scale at which pair production
// makes a single-particle picture untenable. Returns +inf for massless input.
inline double compton_wavelength_m(double mass_kg) {
    if (mass_kg <= 0.0)
        return INFINITY;
    return h / (mass_kg * c);
}

// Reduced Compton wavelength lambdabar_C = hbar/(m c) = lambda_C / (2 pi).
inline double reduced_compton_wavelength_m(double mass_kg) {
    if (mass_kg <= 0.0)
        return INFINITY;
    return hbar / (mass_kg * c);
}

// de Broglie wavelength lambda = h/p for a given momentum (kg m/s).
inline double de_broglie_wavelength_m(double momentum) {
    if (momentum <= 0.0)
        return INFINITY;
    return h / momentum;
}

// Non-relativistic de Broglie wavelength for mass m moving at speed v.
inline double de_broglie_wavelength_nr_m(double mass_kg, double velocity) {
    if (mass_kg <= 0.0 || velocity <= 0.0)
        return INFINITY;
    return h / (mass_kg * velocity);
}

// Thermal de Broglie wavelength Lambda = h / sqrt(2 pi m k_B T): below the mean
// particle spacing, quantum statistics (degeneracy) take over. Returns +inf as
// T -> 0 (everything is wave-like at absolute zero).
inline double thermal_de_broglie_m(double mass_kg, double temperature_K) {
    if (mass_kg <= 0.0 || temperature_K <= 0.0)
        return INFINITY;
    return h / std::sqrt(2.0 * pi * mass_kg * kB * temperature_K);
}

// ---------------------------------------------------------------------------
// Gravity meets the quantum: Schwarzschild radius and the Planck crossover
// ---------------------------------------------------------------------------

// Schwarzschild radius r_s = 2 G M / c^2 (the event-horizon radius of mass M).
inline double schwarzschild_radius_m(double mass_kg) {
    return 2.0 * G * mass_kg / c2;
}

// The mass at which a particle's reduced Compton wavelength equals its
// Schwarzschild radius: hbar/(m c) = 2 G m / c^2  ->  m = sqrt(hbar c / (2 G)).
// This is the Planck mass up to a factor of sqrt(2); it is *why* the Planck mass
// is the boundary where quantum field theory and gravity must merge. Below this
// mass a particle is "bigger" (Compton) than its horizon; above it, the horizon
// wins and a black hole is the better description.
inline double compton_schwarzschild_crossover_mass_kg() {
    return std::sqrt(hbar * c / (2.0 * G));
}

// Hawking temperature T_H = hbar c^3 / (8 pi G M k_B): smaller holes are hotter.
// Evaluated at the Planck mass it returns ~T_P / (8 pi).
inline double hawking_temperature_K(double mass_kg) {
    if (mass_kg <= 0.0)
        return INFINITY;
    return hbar * c * c * c / (8.0 * pi * G * mass_kg * kB);
}

// ---------------------------------------------------------------------------
// Heisenberg uncertainty principle
// ---------------------------------------------------------------------------

// Minimum momentum spread given a position spread: dp_min = hbar/(2 dx).
inline double min_momentum_uncertainty(double dx_m) {
    if (dx_m <= 0.0)
        return INFINITY;
    return hbar / (2.0 * dx_m);
}

// Minimum energy spread given a lifetime/observation window: dE_min = hbar/(2 dt).
inline double min_energy_uncertainty_J(double dt_s) {
    if (dt_s <= 0.0)
        return INFINITY;
    return hbar / (2.0 * dt_s);
}

// Does a proposed (dx, dp) pair respect dx*dp >= hbar/2 ? (Tiny tolerance for
// floating-point equality at the bound.)
inline bool satisfies_uncertainty(double dx_m, double dp) {
    return dx_m * dp >= 0.5 * hbar * (1.0 - 1.0e-12);
}

// ---------------------------------------------------------------------------
// Quantum harmonic oscillator -- the universal "well" of bound quantum systems
// ---------------------------------------------------------------------------

// Energy of level n: E_n = (n + 1/2) hbar omega. The vacuum (n=0) still carries
// the zero-point energy hbar omega / 2 -- the quantum tier is never truly still.
inline double qho_level_energy_J(int n, double omega) {
    const double nn = n < 0 ? 0.0 : static_cast<double>(n);
    return (nn + 0.5) * hbar * omega;
}

// Zero-point energy E_0 = hbar omega / 2.
inline double qho_zero_point_energy_J(double omega) {
    return 0.5 * hbar * omega;
}

// ---------------------------------------------------------------------------
// Bohr model / hydrogen -- the bridge from the quantum floor up to chemistry
// ---------------------------------------------------------------------------

// Bohr radius a_0 = hbar / (m_e c alpha), the natural size of the hydrogen atom.
inline double bohr_radius_derived_m() {
    return hbar / (electron_mass_kg * c * alpha);
}

// Rydberg energy Ry = alpha^2 m_e c^2 / 2 (the hydrogen ionization energy,
// 13.6057 eV). Returned in joules.
inline double rydberg_energy_derived_J() {
    return 0.5 * alpha * alpha * electron_mass_kg * c2;
}

// Hydrogen (hydrogenic, charge Z) bound-state energy E_n = -Z^2 Ry / n^2 [eV].
// n=1 gives the ground state (-13.6057 eV for Z=1); E -> 0 as n -> infinity.
inline double hydrogen_level_energy_ev(int n, int Z = 1) {
    if (n <= 0)
        return 0.0;
    const double Ry_eV = joules_to_ev(rydberg_energy_derived_J());
    return -static_cast<double>(Z * Z) * Ry_eV / static_cast<double>(n * n);
}

// Energy of a photon emitted in the transition n_hi -> n_lo (n_hi > n_lo), eV.
// Positive (energy released). The Lyman/Balmer series fall out of this directly.
inline double hydrogen_transition_ev(int n_lo, int n_hi, int Z = 1) {
    // Electron falls n_hi -> n_lo and emits a photon; E(n_hi) > E(n_lo), so the
    // released energy is positive.
    return hydrogen_level_energy_ev(n_hi, Z) - hydrogen_level_energy_ev(n_lo, Z);
}

// Photon energy <-> wavelength: E = h c / lambda.
inline double photon_energy_J(double wavelength_m) {
    if (wavelength_m <= 0.0)
        return INFINITY;
    return h * c / wavelength_m;
}
inline double photon_wavelength_m(double energy_J) {
    if (energy_J <= 0.0)
        return INFINITY;
    return h * c / energy_J;
}

// ---------------------------------------------------------------------------
// Particle decay -- lifetime <-> width (the natural linewidth, tau = hbar/Gamma)
// ---------------------------------------------------------------------------

// Decay width Gamma = hbar / tau, from a mean lifetime in seconds.
inline double decay_width_J_from_lifetime(double tau_s) {
    if (tau_s <= 0.0)
        return INFINITY; // tau -> 0  =>  infinitely broad
    return hbar / tau_s;
}
inline double decay_width_mev_from_lifetime(double tau_s) {
    return joules_to_mev(decay_width_J_from_lifetime(tau_s));
}

// Mean lifetime tau = hbar / Gamma, from a decay width in MeV.
inline double lifetime_s_from_width_mev(double width_mev) {
    if (width_mev <= 0.0)
        return INFINITY; // stable
    return hbar / mev_to_joules(width_mev);
}

// PDG 2024 reference values -- used to validate the relations above and to give
// the inspector real, citable numbers for the unstable members of the zoo.
inline constexpr double kMuonLifetime_s = 2.1969811e-6; // mu -> e nu nu
inline constexpr double kTauLifetime_s = 2.903e-13;     // tau
inline constexpr double kNeutronLifetime_s = 878.4;     // free neutron beta decay
inline constexpr double kWWidth_mev = 2085.0;           // W boson total width
inline constexpr double kZWidth_mev = 2495.5;           // Z boson total width
inline constexpr double kHiggsWidth_mev = 4.1;          // SM Higgs total width

} // namespace quantum
} // namespace cosmos

#endif // COSMOS_QUANTUMSCALE_HPP
