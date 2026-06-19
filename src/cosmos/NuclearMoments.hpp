// NuclearMoments.hpp -- the electromagnetic fingerprints of a nucleus: the
// nuclear magneton, the single-particle (Schmidt) magnetic moments, the free
// nucleon g-factors, electric quadrupole moments and their prolate/oblate sign,
// and Larmor precession (the basis of NMR/MRI). These are how a nucleus couples
// to magnetic and electric fields.
//
// Header-only, pure, deterministic. Magnetic moments in nuclear magnetons (mu_N).
//
// Sources:
//   - Nuclear magneton mu_N = e hbar / (2 m_p) = 3.15245e-8 eV/T.
//   - Free nucleon moments: mu_p = +2.792847, mu_n = -1.913043 mu_N.
//   - Schmidt single-particle moments (g_l = 1 (p) / 0 (n), g_s = 2 mu).

#ifndef COSMOS_NUCLEARMOMENTS_HPP
#define COSMOS_NUCLEARMOMENTS_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace moments {

// Nuclear magneton mu_N = e hbar / (2 m_p). [J/T] and [eV/T].
inline constexpr double kNuclearMagneton_J_per_T = 5.0507837461e-27;
inline constexpr double kNuclearMagneton_eV_per_T = 3.15245125417e-8;

// Free nucleon magnetic moments [mu_N] and the corresponding spin g-factors.
inline constexpr double kProtonMoment = 2.79284734;
inline constexpr double kNeutronMoment = -1.91304273;
inline constexpr double kProtonGs = 2.0 * kProtonMoment;   // g_s(p) = +5.5857
inline constexpr double kNeutronGs = 2.0 * kNeutronMoment; // g_s(n) = -3.8261

// --- Schmidt single-particle magnetic moments -------------------------------

// The Schmidt estimate of an odd-A nucleus's magnetic moment from its single
// unpaired nucleon (orbital l, total j = l +/- 1/2). [mu_N]
//   j = l + 1/2:  mu = (j - 1/2) g_l + g_s/2
//   j = l - 1/2:  mu = j/(j+1) [ (j + 3/2) g_l - g_s/2 ]
inline double schmidt_moment(bool j_is_l_plus_half, int l, bool is_proton) {
    const double g_l = is_proton ? 1.0 : 0.0;
    const double g_s = is_proton ? kProtonGs : kNeutronGs;
    if (j_is_l_plus_half) {
        const double j = l + 0.5;
        return (j - 0.5) * g_l + 0.5 * g_s;
    } else {
        const double j = l - 0.5;
        if (j <= 0.0)
            return 0.0;
        return j / (j + 1.0) * ((j + 1.5) * g_l - 0.5 * g_s);
    }
}

// Even-even nuclei have a 0+ ground state: zero magnetic and quadrupole moment.
inline double even_even_moment() {
    return 0.0;
}

// g-factor of a state of spin j with magnetic moment mu: g = mu / j.
inline double g_factor(double mu_muN, double j) {
    return j != 0.0 ? mu_muN / j : 0.0;
}

// --- Electric quadrupole moment ---------------------------------------------

// Single-particle quadrupole moment of an odd proton in orbital j:
//   Q_sp = -(2j - 1)/(2j + 2) <r^2>, with <r^2> ~ (3/5) R^2. [fm^2] (sign only
// rigorous; magnitude is the standard estimate).
inline double single_particle_quadrupole(double j, int A_mass) {
    const double R = 1.2 * std::cbrt((double)A_mass);
    const double r2 = 0.6 * R * R;
    return -(2.0 * j - 1.0) / (2.0 * j + 2.0) * r2;
}

// Sign convention: a prolate (cigar) intrinsic shape gives Q > 0, oblate Q < 0.
inline bool is_prolate(double Q) {
    return Q > 0.0;
}
inline bool is_oblate(double Q) {
    return Q < 0.0;
}

// --- Larmor precession ------------------------------------------------------

// Larmor angular frequency omega = g (mu_N/hbar) B for a field B [T]. [rad/s]
inline double larmor_frequency(double g_factor_val, double B_tesla) {
    return g_factor_val * kNuclearMagneton_J_per_T * B_tesla / constants::hbar;
}

} // namespace moments
} // namespace cosmos

#endif // COSMOS_NUCLEARMOMENTS_HPP
