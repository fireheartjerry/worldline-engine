// NuclearReactions.hpp -- how nuclei combine and split: reaction Q-values, the
// Coulomb barrier and the Gamow peak that govern thermonuclear fusion (with the
// astrophysical S-factor), and fission -- the fissility parameter Z^2/A, the
// liquid-drop barrier, and the ~200 MeV energy release. The quantitative engine
// behind stellar burning and reactors.
//
// Header-only, pure, deterministic. Energies in MeV, temperatures in K.
//
// Sources:
//   - Coulomb barrier E_C = Z1 Z2 e^2 / (4 pi eps0 R), R = r0(A1^1/3 + A2^1/3).
//   - Gamow peak E0 = (b kT / 2)^{2/3}, b = pi sqrt(2 mu) Z1 Z2 e^2 / (4 pi eps0 hbar).
//   - Astrophysical S-factor: sigma(E) = S(E)/E exp(-2 pi eta).
//   - Bohr-Wheeler (1939) fissility x = (Z^2/A) / (Z^2/A)_crit, (Z^2/A)_crit ~ 50.9.

#ifndef COSMOS_NUCLEARREACTIONS_HPP
#define COSMOS_NUCLEARREACTIONS_HPP

#include "cosmos/Constants.hpp"
#include "cosmos/NuclearData.hpp"

#include <cmath>

namespace cosmos {
namespace reactions {

// --- Reaction Q-value -------------------------------------------------------

// Q for A(Z1,A1) + B(Z2,A2) -> C(Z3,A3) + D(Z4,A4): Q = (M_in - M_out) c^2.
// Positive Q is exothermic. [MeV]
inline double reaction_q_mev(int Z1, int A1, int Z2, int A2, int Z3, int A3, int Z4, int A4) {
    using namespace nuclear;
    const double m_in = nuclear_mass_mev(Z1, A1) + nuclear_mass_mev(Z2, A2);
    const double m_out = nuclear_mass_mev(Z3, A3) + nuclear_mass_mev(Z4, A4);
    return m_in - m_out;
}

// Fusion Q from binding energies for X + Y -> Z (single product):
//   Q = B(product) - B(X) - B(Y). [MeV]
inline double fusion_q_mev(int Zx, int Ax, int Zy, int Ay) {
    using namespace nuclear;
    return binding_energy_mev(Zx + Zy, Ax + Ay) - binding_energy_mev(Zx, Ax) -
           binding_energy_mev(Zy, Ay);
}

// --- Coulomb barrier --------------------------------------------------------

// The Coulomb barrier two nuclei must overcome to touch:
//   E_C = Z1 Z2 (e^2/4 pi eps0) / (r0 (A1^1/3 + A2^1/3)). [MeV]
inline double coulomb_barrier_mev(int Z1, int A1, int Z2, int A2) {
    using namespace nuclear;
    const double R = kR0_fm * (std::cbrt((double)A1) + std::cbrt((double)A2));
    if (R <= 0.0)
        return INFINITY;
    return Z1 * Z2 * kCoulomb_mev_fm / R;
}

// --- Gamow peak (thermonuclear fusion) --------------------------------------

// Sommerfeld parameter for the Gamow integrand at energy E: eta = Z1 Z2 alpha c / v,
// with v from the relative kinetic energy E and reduced mass mu (in MeV/c^2).
inline double sommerfeld_eta(int Z1, int Z2, double E_mev, double mu_mev) {
    if (E_mev <= 0.0 || mu_mev <= 0.0)
        return INFINITY;
    const double beta = std::sqrt(2.0 * E_mev / mu_mev); // v/c
    return static_cast<double>(Z1 * Z2) * constants::alpha / beta;
}

// The Gamow "b" constant (in MeV^{1/2}) such that the tunnelling factor is
// exp(-b / sqrt(E)): b = 2 pi Z1 Z2 alpha c sqrt(mu / (2 c^2))... in MeV units
//   b = pi sqrt(2 mu) Z1 Z2 alpha / 1 (with mu in MeV/c^2, b in MeV^{1/2}).
inline double gamow_b_sqrt_mev(int Z1, int Z2, double mu_mev) {
    return constants::pi * std::sqrt(2.0 * mu_mev) * Z1 * Z2 * constants::alpha;
}

// Gamow peak energy E0 = (b kT / 2)^{2/3}: the narrow window where the falling
// Maxwell tail meets the rising tunnelling probability. [MeV]
inline double gamow_peak_mev(int Z1, int Z2, double mu_mev, double T_K) {
    const double kT = constants::kB * T_K / (1.0e6 * constants::e); // MeV
    const double b = gamow_b_sqrt_mev(Z1, Z2, mu_mev);
    return std::pow(b * kT / 2.0, 2.0 / 3.0);
}

// Tunnelling suppression through the Coulomb barrier: exp(-b / sqrt(E)).
inline double gamow_tunnelling(int Z1, int Z2, double E_mev, double mu_mev) {
    if (E_mev <= 0.0)
        return 0.0;
    return std::exp(-gamow_b_sqrt_mev(Z1, Z2, mu_mev) / std::sqrt(E_mev));
}

// Cross section from the astrophysical S-factor: sigma(E) = S(E)/E exp(-b/sqrt(E)).
// S is the smooth nuclear part; the exponential is the Coulomb penetrability.
inline double cross_section_from_S(double S, int Z1, int Z2, double E_mev, double mu_mev) {
    if (E_mev <= 0.0)
        return 0.0;
    return (S / E_mev) * gamow_tunnelling(Z1, Z2, E_mev, mu_mev);
}

// --- Fission ----------------------------------------------------------------

inline constexpr double kFissilityCritical = 50.9; // (Z^2/A)_crit (Bohr-Wheeler)

// Fissility parameter x = (Z^2/A) / (Z^2/A)_crit. x >= 1 means the nucleus is
// unstable to spontaneous fission (no barrier).
inline double fissility(int Z, int A) {
    if (A <= 0)
        return 0.0;
    return (static_cast<double>(Z) * Z / A) / kFissilityCritical;
}

// Liquid-drop fission barrier height drops as fissility rises; a simple estimate
// scaling the surface energy by (1 - x)^3 (vanishes as x -> 1). [relative]
inline double fission_barrier_factor(int Z, int A) {
    const double x = fissility(Z, A);
    if (x >= 1.0)
        return 0.0;
    const double f = 1.0 - x;
    return f * f * f;
}

// Energy released in fission ~ the binding-energy gain going from a heavy nucleus
// (~7.6 MeV/nucleon) to two mid-mass fragments (~8.5 MeV/nucleon). [MeV]
inline double fission_energy_release_mev(int Z, int A) {
    using namespace nuclear;
    if (A < 4)
        return 0.0;
    const int Z1 = Z / 2, A1 = A / 2;
    const int Z2 = Z - Z1, A2 = A - A1;
    return binding_energy_mev(Z1, A1) + binding_energy_mev(Z2, A2) - binding_energy_mev(Z, A);
}

} // namespace reactions
} // namespace cosmos

#endif // COSMOS_NUCLEARREACTIONS_HPP
