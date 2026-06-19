// Ionization.hpp -- where atoms come apart: photoionization thresholds, the Saha
// equation of ionization equilibrium (the bridge from atomic physics to stellar
// atmospheres), and the collective behaviour of the resulting plasma -- the Debye
// screening length and the plasma frequency. This governs when matter is neutral
// gas versus ionized plasma.
//
// Header-only, pure, deterministic. SI units unless a name says otherwise;
// ionization energies in eV.
//
// Sources:
//   - Saha (1920) ionization equation.
//   - Debye length lambda_D = sqrt(eps0 k T / (n_e e^2)).
//   - Plasma frequency omega_p = sqrt(n_e e^2 / (eps0 m_e)).

#ifndef COSMOS_IONIZATION_HPP
#define COSMOS_IONIZATION_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace ionization {

inline constexpr double kEps0 = 8.8541878128e-12; // vacuum permittivity [F/m]

// --- Photoionization --------------------------------------------------------

// Threshold photon energy to ionize a bound electron = its binding energy. The
// threshold wavelength lambda = h c / E. [nm] (E in eV)
inline double photoionization_threshold_nm(double binding_ev) {
    if (binding_ev <= 0.0)
        return INFINITY;
    const double E_J = binding_ev * constants::e;
    return 1.0e9 * constants::h * constants::c / E_J;
}

// A photon ionizes only if its energy exceeds the binding energy.
inline bool can_ionize(double photon_ev, double binding_ev) {
    return photon_ev >= binding_ev;
}

// --- Saha equation ----------------------------------------------------------

// The Saha ratio n_{i+1} n_e / n_i for successive ionization stages:
//   = (2 g_{i+1}/g_i) (2 pi m_e k T / h^2)^{3/2} exp(-chi / kT),
// with chi the ionization energy. [SI: 1/m^3]
inline double saha_rhs(double T_K, double chi_ev, double g_ratio = 1.0) {
    if (T_K <= 0.0)
        return 0.0;
    const double kT = constants::kB * T_K;
    const double chi_J = chi_ev * constants::e;
    const double lambda_term = std::pow(2.0 * constants::pi * constants::electron_mass_kg * kT /
                                            (constants::h * constants::h),
                                        1.5);
    return 2.0 * g_ratio * lambda_term * std::exp(-chi_J / kT);
}

// Ionization ratio n_{i+1}/n_i given the electron density n_e [1/m^3].
inline double ionization_ratio(double T_K, double chi_ev, double n_e, double g_ratio = 1.0) {
    if (n_e <= 0.0)
        return INFINITY;
    return saha_rhs(T_K, chi_ev, g_ratio) / n_e;
}

// Ionized fraction x = n_{i+1} / (n_i + n_{i+1}) = r / (1 + r), in [0,1).
inline double ionized_fraction(double T_K, double chi_ev, double n_e, double g_ratio = 1.0) {
    const double r = ionization_ratio(T_K, chi_ev, n_e, g_ratio);
    if (!std::isfinite(r))
        return 1.0;
    return r / (1.0 + r);
}

// --- Plasma collective behaviour --------------------------------------------

// Debye screening length lambda_D = sqrt(eps0 k T / (n_e e^2)) [m]: the distance
// over which charge imbalances are screened.
inline double debye_length_m(double T_K, double n_e) {
    if (n_e <= 0.0 || T_K <= 0.0)
        return INFINITY;
    return std::sqrt(kEps0 * constants::kB * T_K / (n_e * constants::e * constants::e));
}

// Electron plasma (angular) frequency omega_p = sqrt(n_e e^2 / (eps0 m_e)) [rad/s].
inline double plasma_frequency_rad_s(double n_e) {
    if (n_e <= 0.0)
        return 0.0;
    return std::sqrt(n_e * constants::e * constants::e / (kEps0 * constants::electron_mass_kg));
}

// Number of electrons inside a Debye sphere (the plasma must be many-particle to
// behave collectively): N_D = (4/3) pi lambda_D^3 n_e.
inline double debye_number(double T_K, double n_e) {
    const double lD = debye_length_m(T_K, n_e);
    if (!std::isfinite(lD))
        return INFINITY;
    return (4.0 / 3.0) * constants::pi * lD * lD * lD * n_e;
}

} // namespace ionization
} // namespace cosmos

#endif // COSMOS_IONIZATION_HPP
