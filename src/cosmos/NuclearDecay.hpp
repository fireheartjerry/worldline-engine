// NuclearDecay.hpp -- radioactivity: the decay law, and every Standard-Model
// decay mode of a nucleus with its energetics and rate systematics. Alpha decay
// (Q-value, Gamow tunnelling, the Geiger-Nuttall law), beta decay (beta-minus,
// beta-plus, electron capture, with Sargent's Q^5 rate rule), and gamma decay
// (Weisskopf single-particle estimates, multipolarity). Decay-mode prediction
// from the energetics ties it back to the SEMF.
//
// Header-only, pure, deterministic. Energies in MeV, times in seconds.
//
// Sources:
//   - Decay law N(t) = N0 e^{-lambda t}, lambda = ln2 / t_half.
//   - Q-values from nuclear masses (cosmos/NuclearData.hpp).
//   - Geiger-Nuttall law log10 t_half = a Z / sqrt(Q) + b.
//   - Sargent's rule: beta-decay rate ~ Q^5 (Fermi theory phase space).
//   - Weisskopf single-particle gamma rates ~ E^{2L+1}.

#ifndef COSMOS_NUCLEARDECAY_HPP
#define COSMOS_NUCLEARDECAY_HPP

#include "cosmos/Constants.hpp"
#include "cosmos/NuclearData.hpp"

#include <cmath>

namespace cosmos {
namespace decay {

inline constexpr double kLn2 = 0.6931471805599453;

// --- Radioactive decay law --------------------------------------------------

inline double decay_constant_from_halflife(double t_half_s) {
    return (t_half_s > 0.0) ? kLn2 / t_half_s : INFINITY;
}
inline double halflife_from_decay_constant(double lambda) {
    return (lambda > 0.0) ? kLn2 / lambda : INFINITY;
}
inline double mean_lifetime_s(double t_half_s) {
    return t_half_s / kLn2;
}

// Surviving fraction after time t: N/N0 = 2^{-t/t_half} = e^{-lambda t}.
inline double surviving_fraction(double t_s, double t_half_s) {
    if (t_half_s <= 0.0)
        return 0.0;
    return std::exp(-kLn2 * t_s / t_half_s);
}

// Activity A = lambda N (decays per second).
inline double activity(double lambda, double N) {
    return lambda * N;
}

// --- Q-values (from nuclear masses) -----------------------------------------

// Alpha decay (Z,A) -> (Z-2,A-4) + alpha. Q_alpha = B(Z-2,A-4) + B(alpha) - B(Z,A)
// (= -S_alpha), using the measured alpha binding so the magic He-4 isn't mangled
// by the smooth SEMF.
inline double q_alpha_mev(int Z, int A) {
    using namespace nuclear;
    return binding_energy_mev(Z - 2, A - 4) + kAlphaBinding_mev - binding_energy_mev(Z, A);
}

// Beta-minus (Z,A) -> (Z+1,A) + e- + nu_bar. Q = M(Z,A) - M(Z+1,A) - m_e.
inline double q_beta_minus_mev(int Z, int A) {
    using namespace nuclear;
    return nuclear_mass_mev(Z, A) - nuclear_mass_mev(Z + 1, A) - kElectronMass_mev;
}

// Beta-plus (Z,A) -> (Z-1,A) + e+ + nu. Q = M(Z,A) - M(Z-1,A) - m_e.
inline double q_beta_plus_mev(int Z, int A) {
    using namespace nuclear;
    return nuclear_mass_mev(Z, A) - nuclear_mass_mev(Z - 1, A) - kElectronMass_mev;
}

// Electron capture (Z,A) + e- -> (Z-1,A) + nu. Q = M(Z,A) + m_e - M(Z-1,A).
// (Atomic binding of the captured electron neglected.)
inline double q_electron_capture_mev(int Z, int A) {
    using namespace nuclear;
    return nuclear_mass_mev(Z, A) + kElectronMass_mev - nuclear_mass_mev(Z - 1, A);
}

// --- Alpha decay systematics ------------------------------------------------

// Gamow factor for alpha tunnelling through the Coulomb barrier: the
// transmission ~ exp(-2 pi eta) with the Sommerfeld parameter
//   eta = Z_d * 2 * alpha c / v,  v from the alpha kinetic energy Q.
// Returns the exponent G = 2 pi eta (larger G -> slower decay).
inline double alpha_gamow_exponent(int Z_daughter, double Q_mev) {
    if (Q_mev <= 0.0)
        return INFINITY;
    // Non-relativistic alpha speed from Q: v = sqrt(2 Q / m_alpha).
    const double m_alpha = 3727.379;                      // MeV/c^2
    const double beta = std::sqrt(2.0 * Q_mev / m_alpha); // v/c
    const double eta = static_cast<double>(Z_daughter) * 2.0 * constants::alpha / beta;
    return 2.0 * constants::pi * eta;
}

// Geiger-Nuttall law: log10(t_half / s) = a Z_d / sqrt(Q[MeV]) + b, with the
// classic empirical constants a ~ 1.61, b ~ -28.9 (Z_d = daughter charge).
inline double geiger_nuttall_log10_halflife(int Z_daughter, double Q_mev) {
    if (Q_mev <= 0.0)
        return INFINITY;
    const double a = 1.61, b = -28.9;
    return a * Z_daughter / std::sqrt(Q_mev) + b;
}

// --- Beta decay systematics -------------------------------------------------

// Sargent's rule: the allowed beta-decay rate scales as Q^5 (Fermi phase space).
// Returned as a relative rate (proportional to Q^5; units arbitrary).
inline double sargent_relative_rate(double Q_mev) {
    if (Q_mev <= 0.0)
        return 0.0;
    return std::pow(Q_mev, 5.0);
}

// --- Gamma decay (Weisskopf single-particle estimates) ----------------------

// Weisskopf single-particle estimate of an electric-multipole (EL) gamma
// transition rate [1/s], for a nucleus of mass number A emitting a photon of
// energy E_gamma (MeV). The standard coefficients carry a steep L dependence, so
// each higher multipole is many orders of magnitude slower than the last; the
// rate also rises as E^{2L+1}. (Standard textbook Weisskopf estimates.)
inline double weisskopf_rate_per_s(int L, double E_gamma_mev, int A) {
    if (L < 1 || E_gamma_mev <= 0.0 || A <= 0)
        return 0.0;
    const double Ad = static_cast<double>(A);
    switch (L) {
    case 1:
        return 1.0e14 * std::pow(Ad, 2.0 / 3.0) * std::pow(E_gamma_mev, 3.0);
    case 2:
        return 7.28e7 * std::pow(Ad, 4.0 / 3.0) * std::pow(E_gamma_mev, 5.0);
    case 3:
        return 34.0 * std::pow(Ad, 2.0) * std::pow(E_gamma_mev, 7.0);
    case 4:
        return 1.07e-5 * std::pow(Ad, 8.0 / 3.0) * std::pow(E_gamma_mev, 9.0);
    default:
        return std::pow(E_gamma_mev, 2 * L + 1);
    }
}

// --- Decay-mode prediction --------------------------------------------------

enum class Mode { Stable, BetaMinus, BetaPlusOrEC, Alpha };

// Predict the dominant ground-state decay mode from the SEMF energetics: a
// nucleus beta-decays toward the valley of stability, and (for heavy nuclei)
// alpha-decays when that channel is open and energetically favoured.
inline Mode predict_mode(int Z, int A) {
    const double qbm = q_beta_minus_mev(Z, A);
    const double qbp = q_beta_plus_mev(Z, A);
    const double qa = (A > 4 && Z > 2) ? q_alpha_mev(Z, A) : -1.0;
    // Heavy alpha emitters: a clearly open alpha channel wins.
    if (qa > 1.0 && A >= 200)
        return Mode::Alpha;
    if (qbm > 0.0 && qbm >= qbp)
        return Mode::BetaMinus;
    if (qbp > 0.0)
        return Mode::BetaPlusOrEC;
    if (qa > 0.0)
        return Mode::Alpha;
    return Mode::Stable;
}

} // namespace decay
} // namespace cosmos

#endif // COSMOS_NUCLEARDECAY_HPP
