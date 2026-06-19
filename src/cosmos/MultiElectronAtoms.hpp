// MultiElectronAtoms.hpp -- atoms past hydrogen, where electrons screen one
// another and interact: Slater's rules for the effective nuclear charge, Hund's
// rules for the ground-state term of a partially-filled subshell, the LS-coupling
// term symbol, and the Aufbau exceptions (Cr, Cu, ...) where a half- or fully-
// filled d shell wins. This is what makes the periodic table's chemistry real.
//
// Header-only, pure, deterministic.
//
// Sources:
//   - Slater (1930) screening rules for Z_eff = Z - S.
//   - Hund's rules: maximise S, then L; J = |L-S| (<= half full) or L+S (> half).
//   - Aufbau anomalies from half/full d-subshell stability.

#ifndef COSMOS_MULTIELECTRONATOMS_HPP
#define COSMOS_MULTIELECTRONATOMS_HPP

#include "cosmos/PeriodicTable.hpp"

#include <cmath>

namespace cosmos {
namespace multielectron {

// --- Slater screening / effective nuclear charge ----------------------------

// Slater screening constant S for the outermost (valence) electron, then
// Z_eff = Z - S. Uses the standard rules for an s/p valence electron:
//   same (n s,p) group: 0.35 each (0.30 in n=1); (n-1) shell: 0.85 each;
//   (n-2) and deeper: 1.00 each.
inline double slater_screening(int Z) {
    const auto cfg = periodic::electron_configuration(Z);
    const int vn = periodic::period(Z);
    double S = 0.0;
    for (const periodic::Subshell &s : cfg) {
        if (s.n == vn && (s.l == 0 || s.l == 1)) {
            const double same = (vn == 1) ? 0.30 : 0.35;
            S += same * s.occupancy;
        } else if (s.n == vn - 1) {
            S += 0.85 * s.occupancy;
        } else if (s.n <= vn - 2) {
            S += 1.00 * s.occupancy;
        }
        // (electrons in the valence n,d/f are ignored for an s/p valence electron)
    }
    // Remove the electron's own contribution (it does not screen itself).
    const double self = (vn == 1) ? 0.30 : 0.35;
    S -= self;
    return S > 0.0 ? S : 0.0;
}

inline double z_effective(int Z) {
    return Z - slater_screening(Z);
}

// --- Hund's rules -----------------------------------------------------------

struct Term {
    double S; // total spin
    int L;    // total orbital angular momentum
    double J; // total angular momentum
};

// Ground-state term of n_e electrons in a subshell of orbital l, by Hund's rules.
// Closed or empty subshells give the 1S0 singlet.
inline Term hunds_ground_term(int l, int n_e) {
    const int cap = 2 * (2 * l + 1);
    if (n_e <= 0 || n_e >= cap)
        return {0.0, 0, 0.0};

    int n_up = 0, n_down = 0, M_L = 0, remaining = n_e;
    for (int m = l; m >= -l && remaining > 0; --m) { // spin-up pass
        ++n_up;
        M_L += m;
        --remaining;
    }
    for (int m = l; m >= -l && remaining > 0; --m) { // spin-down pass
        ++n_down;
        M_L += m;
        --remaining;
    }
    const double S = (n_up - n_down) / 2.0;
    const int L = std::abs(M_L);
    const bool more_than_half = n_e > cap / 2;
    const double J = more_than_half ? (L + S) : std::abs(L - S);
    return {S, L, J};
}

inline int term_multiplicity(const Term &t) {
    return static_cast<int>(std::round(2.0 * t.S + 1.0));
}

// --- Aufbau exceptions ------------------------------------------------------

// Elements whose ground configuration departs from the naive Madelung filling
// (half/full d or f shells win): Cr, Cu, Nb, Mo, Ru, Rh, Pd, Ag, La, Ce, Gd, Pt,
// Au, Ac, Th, Pa, U, Np, Cm, Lr.
inline bool is_aufbau_exception(int Z) {
    for (int z : {24, 29, 41, 42, 44, 45, 46, 47, 57, 58, 64, 78, 79, 89, 90, 91, 92, 93, 96, 103})
        if (z == Z)
            return true;
    return false;
}

// --- Successive ionization energies -----------------------------------------

// Successive ionization energies always increase (each electron is pulled from an
// ever more positive ion); there is a large jump once a noble-gas core is reached.
// Returns true if the (one-indexed) ionization that removes the (n_core+1)-th
// electron breaks into a closed shell -- i.e. a big jump is expected after it.
inline bool big_ionization_jump_after(int valence_count, int which) {
    return which == valence_count; // the next electron comes from the core
}

} // namespace multielectron
} // namespace cosmos

#endif // COSMOS_MULTIELECTRONATOMS_HPP
