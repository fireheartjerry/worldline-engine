// AtomicStructure.hpp -- the quantum structure of atoms: hydrogenic energy levels
// and their Z^2 scaling, the full set of quantum numbers and orbital degeneracies,
// orbital radii and electron velocities, the Rydberg formula, fine structure
// (spin-orbit), and the quantum defect that shifts alkali spectra. Builds on the
// Bohr/Rydberg anchors in QuantumScale and is the foundation the periodic table
// and spectra stand on.
//
// Header-only, pure, deterministic. Energies in eV, lengths in pm.
//
// Sources:
//   - Bohr model E_n = -Ry Z^2 / n^2, Ry = 13.6057 eV; a_0 = 52.918 pm.
//   - Rydberg formula 1/lambda = R_inf Z^2 (1/n1^2 - 1/n2^2), R_inf = 1.0974e7 /m.
//   - Fine structure ~ alpha^2; orbital velocity v_n = Z alpha c / n.

#ifndef COSMOS_ATOMICSTRUCTURE_HPP
#define COSMOS_ATOMICSTRUCTURE_HPP

#include "cosmos/Constants.hpp"

#include <cmath>
#include <string>

namespace cosmos {
namespace atom {

inline constexpr double kRydberg_eV = 13.605693;
inline constexpr double kBohrRadius_pm = 52.917721;
inline constexpr double kRydbergConst_per_m = 1.0973731568e7; // R_infinity

// --- Energy levels ----------------------------------------------------------

// Hydrogenic bound-state energy E_n = -Ry Z^2 / n^2 [eV]. n=1, Z=1 -> -13.606 eV.
inline double energy_level_ev(int n, int Z = 1) {
    if (n <= 0)
        return 0.0;
    return -kRydberg_eV * static_cast<double>(Z * Z) / static_cast<double>(n * n);
}

// Ionization energy from level n of a hydrogenic ion: +Ry Z^2 / n^2 [eV].
inline double ionization_energy_ev(int n, int Z = 1) {
    return -energy_level_ev(n, Z);
}

// Quantum-defect-corrected energy for an alkali-like valence electron:
//   E = -Ry / (n - delta)^2 (delta is the l-dependent quantum defect). [eV]
inline double quantum_defect_energy_ev(int n, double defect) {
    const double neff = n - defect;
    if (neff <= 0.0)
        return 0.0;
    return -kRydberg_eV / (neff * neff);
}

// --- Quantum numbers and degeneracy -----------------------------------------

// Degeneracy of principal shell n (including spin): 2 n^2.
inline int shell_degeneracy(int n) {
    return (n > 0) ? 2 * n * n : 0;
}

// Capacity of a subshell of orbital angular momentum l: 2(2l+1).
inline int subshell_capacity(int l) {
    return (l >= 0) ? 2 * (2 * l + 1) : 0;
}

// Number of allowed m_l values for orbital l: 2l + 1.
inline int orbital_count(int l) {
    return (l >= 0) ? 2 * l + 1 : 0;
}

// Spectroscopic letter for orbital angular momentum l (s,p,d,f,g,h,...).
inline char orbital_letter(int l) {
    static const char *k = "spdfghiklm";
    if (l < 0 || l >= 10)
        return '?';
    return k[l];
}

// Is (n, l, m_l, m_s) a physically allowed single-electron state? l<n, |m_l|<=l,
// m_s = +/-1/2 (passed as +/-1 for the two spin projections).
inline bool valid_state(int n, int l, int m_l, int two_m_s) {
    if (n < 1 || l < 0 || l >= n)
        return false;
    if (m_l < -l || m_l > l)
        return false;
    return two_m_s == 1 || two_m_s == -1;
}

// --- Sizes and velocities ---------------------------------------------------

// Bohr orbital radius r_n = n^2 a_0 / Z [pm]: atoms shrink with nuclear charge.
inline double orbital_radius_pm(int n, int Z = 1) {
    if (Z <= 0)
        return 0.0;
    return static_cast<double>(n * n) * kBohrRadius_pm / Z;
}

// Electron orbital speed as a fraction of c: v_n / c = Z alpha / n. As Z*alpha
// approaches 1 (heavy atoms) the inner electrons turn relativistic.
inline double orbital_velocity_over_c(int n, int Z = 1) {
    return static_cast<double>(Z) * constants::alpha / static_cast<double>(n);
}

// --- Spectral lines (Rydberg formula) ---------------------------------------

// Transition wavelength n2 -> n1 (n2 > n1) for a hydrogenic ion of charge Z:
//   1/lambda = R Z^2 (1/n1^2 - 1/n2^2). Returns lambda in nm.
inline double transition_wavelength_nm(int n1, int n2, int Z = 1) {
    if (n1 <= 0 || n2 <= n1)
        return 0.0;
    const double inv =
        kRydbergConst_per_m * static_cast<double>(Z * Z) * (1.0 / (n1 * n1) - 1.0 / (n2 * n2));
    if (inv <= 0.0)
        return 0.0;
    return 1.0e9 / inv; // m -> nm
}

// Photon energy released in a transition n2 -> n1 (n2 > n1): E(n2) - E(n1) > 0. [eV]
inline double transition_energy_ev(int n1, int n2, int Z = 1) {
    return energy_level_ev(n2, Z) - energy_level_ev(n1, Z);
}

// --- Fine structure ---------------------------------------------------------

// Fine-structure energy scale: the spin-orbit / relativistic correction is of
// order alpha^2 times the gross structure. Returns |Delta E_fs| for level n of a
// hydrogenic ion (magnitude, ~ Ry Z^4 alpha^2 / n^3). [eV]
inline double fine_structure_scale_ev(int n, int Z = 1) {
    if (n <= 0)
        return 0.0;
    const double a2 = constants::alpha * constants::alpha;
    return kRydberg_eV * std::pow(static_cast<double>(Z), 4.0) * a2 /
           static_cast<double>(n * n * n);
}

} // namespace atom
} // namespace cosmos

#endif // COSMOS_ATOMICSTRUCTURE_HPP
