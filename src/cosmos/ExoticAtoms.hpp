// ExoticAtoms.hpp -- atoms that aren't ordinary hydrogen: the reduced-mass
// correction (deuterium vs hydrogen), positronium (an electron bound to its own
// antiparticle), muonic atoms (a muon 200x heavier than an electron, orbiting
// 200x tighter), and Rydberg atoms (enormous, fragile states of very high n with
// their characteristic n-power scaling laws). The same Bohr physics, rescaled.
//
// Header-only, pure, deterministic. Energies in eV, lengths in pm.
//
// Sources:
//   - Reduced-mass Rydberg R_M = R_inf * mu/m_e.
//   - Positronium: mu = m_e/2 -> levels halved, ground state -6.80 eV, radius 2 a0.
//   - Muonic hydrogen: mu ~ 186 m_e -> Bohr radius ~256 fm.
//   - Rydberg scaling: radius ~ n^2, energy ~ 1/n^2, lifetime ~ n^3, polarizability ~ n^7.

#ifndef COSMOS_EXOTICATOMS_HPP
#define COSMOS_EXOTICATOMS_HPP

#include "cosmos/AtomicStructure.hpp"
#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace exotic {

inline constexpr double kMuonMass_me = 206.7682830;  // m_mu / m_e
inline constexpr double kProtonMass_me = 1836.15267; // m_p / m_e

// --- Reduced-mass scaling ---------------------------------------------------

// Reduced mass mu = m1 m2 / (m1 + m2), in units of the electron mass, for an
// orbiting particle of mass m_orbit (in m_e) about a nucleus of mass m_nuc (m_e).
inline double reduced_mass_me(double m_orbit_me, double m_nuc_me) {
    return m_orbit_me * m_nuc_me / (m_orbit_me + m_nuc_me);
}

// Rydberg energy scaled by the reduced mass: heavier orbiting/nuclear masses bind
// more tightly. Returns the |ground-state| binding energy [eV] for charge Z.
inline double scaled_binding_ev(double mu_me, int Z = 1) {
    return atom::kRydberg_eV * mu_me * static_cast<double>(Z * Z);
}

// Bohr radius scaled by the reduced mass: a = a0 (m_e/mu) / Z. [pm]
inline double scaled_bohr_radius_pm(double mu_me, int Z = 1) {
    return atom::kBohrRadius_pm / (mu_me * Z);
}

// Ordinary hydrogen uses mu = m_e m_p/(m_e+m_p) ~ 0.99946 m_e (a 0.05% shift that
// distinguishes hydrogen from deuterium spectroscopically).
inline double hydrogen_reduced_mass_me() {
    return reduced_mass_me(1.0, kProtonMass_me);
}

// --- Positronium (e+ e-) ----------------------------------------------------

// mu = m_e/2: levels are exactly half of hydrogen's; ground state -6.80 eV.
inline double positronium_binding_ev() {
    return scaled_binding_ev(0.5, 1);
}
inline double positronium_radius_pm() {
    return scaled_bohr_radius_pm(0.5, 1);
} // 2 a0

// --- Muonic hydrogen (mu- p) ------------------------------------------------

inline double muonic_hydrogen_reduced_mass_me() {
    return reduced_mass_me(kMuonMass_me, kProtonMass_me);
}
inline double muonic_hydrogen_binding_ev() {
    return scaled_binding_ev(muonic_hydrogen_reduced_mass_me(), 1);
}
// The muon orbits ~186x closer than an electron -> Bohr radius ~256 fm.
inline double muonic_hydrogen_radius_pm() {
    return scaled_bohr_radius_pm(muonic_hydrogen_reduced_mass_me(), 1);
}

// --- Rydberg atoms (very high n) --------------------------------------------

// Orbital radius of a Rydberg state ~ n^2 a0. [pm]
inline double rydberg_radius_pm(int n) {
    return static_cast<double>(n * n) * atom::kBohrRadius_pm;
}

// Binding energy ~ Ry / n^2: Rydberg states are weakly bound (meV at n~50). [eV]
inline double rydberg_binding_ev(int n) {
    if (n <= 0)
        return 0.0;
    return atom::kRydberg_eV / static_cast<double>(n * n);
}

// Radiative lifetime scales as ~ n^3 (and as ~ n^5 for high-l circular states):
// Rydberg atoms are extraordinarily long-lived. Returns a relative lifetime.
inline double rydberg_lifetime_scaling(int n) {
    return std::pow(static_cast<double>(n), 3.0);
}

// Static polarizability scales as ~ n^7: Rydberg atoms are hugely sensitive to
// stray fields (the basis of Rydberg-atom quantum technology). Returns relative.
inline double rydberg_polarizability_scaling(int n) {
    return std::pow(static_cast<double>(n), 7.0);
}

// Energy spacing between adjacent Rydberg levels ~ 2 Ry / n^3 (vanishing gaps). [eV]
inline double rydberg_level_spacing_ev(int n) {
    if (n <= 0)
        return 0.0;
    return 2.0 * atom::kRydberg_eV / std::pow(static_cast<double>(n), 3.0);
}

} // namespace exotic
} // namespace cosmos

#endif // COSMOS_EXOTICATOMS_HPP
