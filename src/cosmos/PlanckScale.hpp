// PlanckScale.hpp -- the complete Planck unit system and quantum-gravity bounds
// for the absolute floor of the scale ladder. Where QuantumScale.hpp gives the
// quantum mechanics of the smallest tier, this module gives the regime where
// quantum mechanics and gravity merge: the Planck units derived in full, black-
// hole thermodynamics, the holographic / Bekenstein entropy bounds, and the
// generalized uncertainty principle (a minimal length).
//
// Header-only, pure, deterministic. SI units unless a name says otherwise.
//
// Sources:
//   - Planck (1899) natural units; CODATA 2022 base constants.
//   - Bekenstein (1973) entropy bound; Bekenstein-Hawking S = k_B A / (4 l_P^2).
//   - Hawking (1975) radiation; Page (1976) evaporation lifetime
//       t_evap = 5120 pi G^2 M^3 / (hbar c^4).
//   - 't Hooft (1993) / Susskind (1995) holographic bound.
//   - Generalized uncertainty principle: dx >= hbar/dp + beta l_P^2 dp/hbar,
//       minimised at dx_min ~ l_P*sqrt(beta) (Amati-Ciafaloni-Veneziano 1989).

#ifndef COSMOS_PLANCKSCALE_HPP
#define COSMOS_PLANCKSCALE_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace planck {

using namespace cosmos::constants;

// ---------------------------------------------------------------------------
// The full Planck unit system -- derived from c, G, hbar, k_B, e.
// ---------------------------------------------------------------------------
//
// The five "base" units (length, mass, time, charge, temperature) fix every
// other Planck quantity by dimensional analysis. Note that several -- the force
// c^4/G, the power c^5/G -- do NOT contain hbar: they are the classical-gravity
// scale, which is why the Planck regime is where gravity becomes "strong".

struct PlanckSystem {
    // Base units.
    double length_m;      // l_P = sqrt(hbar G / c^3)
    double mass_kg;       // m_P = sqrt(hbar c / G)
    double time_s;        // t_P = l_P / c
    double charge_C;      // q_P = sqrt(4 pi eps0 hbar c) = e / sqrt(alpha)
    double temperature_K; // T_P = m_P c^2 / k_B
    // Derived units.
    double energy_J;      // E_P = m_P c^2
    double momentum_kgms; // p_P = m_P c
    double force_N;       // F_P = c^4 / G
    double power_W;       // P_P = c^5 / G
    double density_kgm3;  // rho_P = c^5 / (hbar G^2)
    double pressure_Pa;   // rho_P c^2
    double area_m2;       // l_P^2
    double volume_m3;     // l_P^3
    double acceleration;  // a_P = c / t_P
    double ang_freq;      // omega_P = 1 / t_P
};

inline PlanckSystem planck_system() {
    PlanckSystem p;
    p.length_m = std::sqrt(hbar * G / (c * c * c));
    p.mass_kg = std::sqrt(hbar * c / G);
    p.time_s = p.length_m / c;
    p.charge_C = e / std::sqrt(alpha);
    p.temperature_K = p.mass_kg * c2 / kB;
    p.energy_J = p.mass_kg * c2;
    p.momentum_kgms = p.mass_kg * c;
    p.force_N = (c * c * c * c) / G;
    p.power_W = (c * c * c * c * c) / G;
    p.density_kgm3 = (c * c * c * c * c) / (hbar * G * G);
    p.pressure_Pa = p.density_kgm3 * c2;
    p.area_m2 = p.length_m * p.length_m;
    p.volume_m3 = p.length_m * p.length_m * p.length_m;
    p.acceleration = c / p.time_s;
    p.ang_freq = 1.0 / p.time_s;
    return p;
}

// ---------------------------------------------------------------------------
// Black-hole thermodynamics -- the bridge between gravity, quanta and entropy.
// ---------------------------------------------------------------------------

// Schwarzschild radius r_s = 2 G M / c^2.
inline double schwarzschild_radius_m(double mass_kg) {
    return 2.0 * G * mass_kg / c2;
}

// Hawking temperature T_H = hbar c^3 / (8 pi G M k_B).
inline double hawking_temperature_K(double mass_kg) {
    if (mass_kg <= 0.0)
        return INFINITY;
    return hbar * c * c * c / (8.0 * pi * G * mass_kg * kB);
}

// Bekenstein-Hawking entropy S = k_B A / (4 l_P^2) = 4 pi G M^2 k_B / (hbar c),
// returned in units of k_B (i.e. dimensionless S/k_B).
inline double bekenstein_hawking_entropy_over_kb(double mass_kg) {
    return 4.0 * pi * G * mass_kg * mass_kg / (hbar * c);
}

// Page evaporation lifetime t = 5120 pi G^2 M^3 / (hbar c^4) [s].
inline double evaporation_time_s(double mass_kg) {
    return 5120.0 * pi * G * G * mass_kg * mass_kg * mass_kg / (hbar * c * c * c * c);
}

// Holographic bound: the maximum information (in bits) that can be stored on a
// surface of area A is A / (4 l_P^2 ln 2). Saturated by a black hole.
inline double holographic_bits(double area_m2) {
    const double lP2 = (hbar * G / (c * c * c)); // l_P^2
    return area_m2 / (4.0 * lP2 * std::log(2.0));
}

// Bekenstein bound: maximum entropy (in k_B) of a region of radius R holding
// energy E: S <= 2 pi k_B R E / (hbar c). Returned as S/k_B.
inline double bekenstein_bound_over_kb(double radius_m, double energy_J) {
    return 2.0 * pi * radius_m * energy_J / (hbar * c);
}

// ---------------------------------------------------------------------------
// Generalized uncertainty principle (GUP) -- a minimal length near l_P.
// ---------------------------------------------------------------------------
//
// String/quantum-gravity arguments modify Heisenberg to
//   dx >= hbar/(2 dp) + beta l_P^2 dp / hbar,
// which has a NON-ZERO minimum: probing shorter distances needs so much momentum
// that you make a black hole instead. The minimum is dx_min = l_P sqrt(beta).

inline double gup_position_uncertainty(double dp, double beta = 1.0) {
    const double lP = std::sqrt(hbar * G / (c * c * c));
    return hbar / (2.0 * dp) + beta * lP * lP * dp / hbar;
}

// The minimal resolvable length: minimising the expression above over dp gives
// dx_min = l_P*sqrt(2 beta), reached at dp = hbar / (l_P sqrt(2 beta)).
inline double gup_minimal_length(double beta = 1.0) {
    const double lP = std::sqrt(hbar * G / (c * c * c));
    return lP * std::sqrt(2.0 * beta);
}

// ---------------------------------------------------------------------------
// Compton-Schwarzschild crossover -- why the Planck mass is the boundary.
// ---------------------------------------------------------------------------

// Mass where the reduced Compton wavelength equals the Schwarzschild radius:
// m = sqrt(hbar c / (2 G)) = m_P / sqrt(2). Below it a particle, above it a hole.
inline double compton_schwarzschild_mass_kg() {
    return std::sqrt(hbar * c / (2.0 * G));
}

} // namespace planck
} // namespace cosmos

#endif // COSMOS_PLANCKSCALE_HPP
