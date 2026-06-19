// LightMatter.hpp -- how atoms and light exchange energy: the Einstein A and B
// coefficients (spontaneous emission, stimulated emission, absorption) and their
// thermodynamic relations, oscillator strengths, the photoelectric effect, the
// Rabi flopping of a driven two-level atom, Beer-Lambert absorption, and the
// population-inversion condition that makes a laser. This is the engine of
// spectroscopy, lasers, and stellar opacity.
//
// Header-only, pure, deterministic. SI units unless noted; energies in eV where
// the atomic world prefers them.
//
// Sources:
//   - Einstein (1917) A/B coefficients: A21/B21 = 8 pi h nu^3 / c^3; g1 B12 = g2 B21.
//   - Photoelectric effect (Einstein 1905): KE = h nu - W.
//   - Rabi (1937) flopping; Beer-Lambert law I = I0 exp(-sigma n x).

#ifndef COSMOS_LIGHTMATTER_HPP
#define COSMOS_LIGHTMATTER_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace lightmatter {

// --- Einstein coefficients --------------------------------------------------

// Ratio A21/B21 = 8 pi h nu^3 / c^3 (spontaneous-to-stimulated, per unit spectral
// energy density). Rises steeply with frequency -> spontaneous emission dominates
// in the optical/UV, stimulated emission in the radio/microwave.
inline double a_over_b_ratio(double nu_hz) {
    return 8.0 * constants::pi * constants::h * nu_hz * nu_hz * nu_hz /
           (constants::c * constants::c * constants::c);
}

// Detailed-balance relation between absorption and stimulated emission:
//   g1 B12 = g2 B21. Returns B12 given B21 and the degeneracies.
inline double b12_from_b21(double B21, int g1, int g2) {
    return (g1 > 0) ? B21 * g2 / g1 : 0.0;
}

// Spontaneous-emission rate A21 = 1/tau, from the upper-state lifetime [s].
inline double spontaneous_rate(double tau_s) {
    return (tau_s > 0.0) ? 1.0 / tau_s : INFINITY;
}

// --- Photoelectric effect ---------------------------------------------------

// Photoelectron kinetic energy KE = h nu - W (work function), in eV; clamped at 0
// below threshold (no emission).
inline double photoelectron_ke_ev(double photon_ev, double work_function_ev) {
    const double ke = photon_ev - work_function_ev;
    return ke > 0.0 ? ke : 0.0;
}

// Threshold frequency nu_0 = W / h for photoemission. [Hz] (W in eV)
inline double photoelectric_threshold_hz(double work_function_ev) {
    return work_function_ev * constants::e / constants::h;
}

inline bool emits_photoelectron(double photon_ev, double work_function_ev) {
    return photon_ev > work_function_ev;
}

// --- Driven two-level atom: Rabi flopping -----------------------------------

// Rabi frequency Omega = d E / hbar for a dipole d [C*m] in a field E [V/m]. [rad/s]
inline double rabi_frequency(double dipole_Cm, double field_Vm) {
    return dipole_Cm * field_Vm / constants::hbar;
}

// Generalised Rabi frequency on detuning delta: Omega_gen = sqrt(Omega^2 + delta^2).
inline double generalized_rabi(double omega, double detuning) {
    return std::sqrt(omega * omega + detuning * detuning);
}

// Excited-state probability of a resonantly driven atom after time t:
//   P_e(t) = sin^2(Omega t / 2). Oscillates between 0 and 1 (Rabi flopping).
inline double rabi_excited_probability(double omega, double t) {
    const double s = std::sin(0.5 * omega * t);
    return s * s;
}

// --- Beer-Lambert absorption ------------------------------------------------

// Transmitted fraction through a column: I/I0 = exp(-sigma n L), with cross
// section sigma [m^2], number density n [1/m^3], path L [m].
inline double transmission(double sigma_m2, double n_density, double L_m) {
    const double tau = sigma_m2 * n_density * L_m;
    return std::exp(-tau);
}

// Optical depth tau = sigma n L (tau ~ 1 marks the transition to opacity).
inline double optical_depth(double sigma_m2, double n_density, double L_m) {
    return sigma_m2 * n_density * L_m;
}

// --- Laser gain / population inversion --------------------------------------

// A medium amplifies light only when it is inverted: n2/g2 > n1/g1.
inline bool is_inverted(double n2, int g2, double n1, int g1) {
    if (g1 <= 0 || g2 <= 0)
        return false;
    return n2 / g2 > n1 / g1;
}

// Small-signal gain coefficient (proportional form): gamma ~ sigma (n2 g1/g2 - n1).
// Positive -> amplification (laser gain); negative -> absorption.
inline double gain_coefficient(double sigma_m2, double n2, int g2, double n1, int g1) {
    if (g2 <= 0)
        return 0.0;
    return sigma_m2 * (n2 * static_cast<double>(g1) / g2 - n1);
}

} // namespace lightmatter
} // namespace cosmos

#endif // COSMOS_LIGHTMATTER_HPP
