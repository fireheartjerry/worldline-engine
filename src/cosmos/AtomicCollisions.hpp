// AtomicCollisions.hpp -- atoms hitting things: geometric and Coulomb cross
// sections, the mean free path and collision frequency of a gas, the Bethe
// stopping power of charged particles in matter, electron-impact excitation /
// ionization thresholds, and radiative recombination. The microphysics behind
// gas transport, radiation damage, and plasma cooling.
//
// Header-only, pure, deterministic. SI units unless noted; energies in eV.
//
// Sources:
//   - Mean free path lambda = 1/(n sigma); collision frequency nu = n sigma v.
//   - Bethe (1930) stopping power -dE/dx ~ (z^2/v^2) n_e ln(...).
//   - Radiative recombination rate alpha_rec ~ T^(-1/2) (slow-electron capture).

#ifndef COSMOS_ATOMICCOLLISIONS_HPP
#define COSMOS_ATOMICCOLLISIONS_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace collisions {

// --- Cross sections ---------------------------------------------------------

// Hard-sphere geometric cross section for two atoms of radii r1, r2:
//   sigma = pi (r1 + r2)^2. [m^2]
inline double geometric_cross_section(double r1_m, double r2_m) {
    const double d = r1_m + r2_m;
    return constants::pi * d * d;
}

// Rutherford-style Coulomb cross section scale ~ (Z1 Z2 e^2 / E)^2: small-angle
// scattering dominates and the cross section falls as 1/E^2. Returns a relative
// scale (proportional, units arbitrary).
inline double coulomb_cross_section_scale(int Z1, int Z2, double E_eV) {
    if (E_eV <= 0.0)
        return INFINITY;
    const double a = static_cast<double>(Z1 * Z2) / E_eV;
    return a * a;
}

// --- Transport: mean free path and collision frequency ----------------------

// Mean free path lambda = 1/(n sigma). [m]
inline double mean_free_path(double n_density, double sigma_m2) {
    if (n_density <= 0.0 || sigma_m2 <= 0.0)
        return INFINITY;
    return 1.0 / (n_density * sigma_m2);
}

// Collision frequency nu = n sigma v. [1/s]
inline double collision_frequency(double n_density, double sigma_m2, double v_ms) {
    return n_density * sigma_m2 * v_ms;
}

// Mean thermal speed v = sqrt(8 k T / (pi m)). [m/s]
inline double mean_thermal_speed(double T_K, double m_kg) {
    if (T_K <= 0.0 || m_kg <= 0.0)
        return 0.0;
    return std::sqrt(8.0 * constants::kB * T_K / (constants::pi * m_kg));
}

// --- Stopping power (Bethe) -------------------------------------------------

// Non-relativistic Bethe stopping-power scale -dE/dx ~ (z^2 / v^2) n_e ln(2 m_e
// v^2 / I), for a projectile of charge z and speed v through a medium of electron
// density n_e and mean excitation energy I. Returns a positive (relative) scale.
inline double bethe_stopping_scale(int z, double v_ms, double n_e, double I_eV) {
    if (v_ms <= 0.0 || I_eV <= 0.0)
        return 0.0;
    const double me = constants::electron_mass_kg;
    const double arg = 2.0 * me * v_ms * v_ms / (I_eV * constants::e);
    if (arg <= 1.0)
        return 0.0; // below the logarithmic threshold, no net loss
    return (static_cast<double>(z * z) / (v_ms * v_ms)) * n_e * std::log(arg);
}

// --- Electron-impact processes ----------------------------------------------

// Electron-impact excitation/ionization is possible only above the threshold
// energy (the excitation or ionization energy of the target).
inline bool impact_above_threshold(double electron_ev, double threshold_ev) {
    return electron_ev >= threshold_ev;
}

// --- Recombination ----------------------------------------------------------

// Radiative recombination rate coefficient scales as ~ T^(-1/2): cooler plasmas
// recombine faster (slow electrons are captured more easily). Returns a relative
// rate (proportional, units arbitrary).
inline double radiative_recombination_scale(double T_K) {
    if (T_K <= 0.0)
        return INFINITY;
    return std::pow(T_K, -0.5);
}

} // namespace collisions
} // namespace cosmos

#endif // COSMOS_ATOMICCOLLISIONS_HPP
