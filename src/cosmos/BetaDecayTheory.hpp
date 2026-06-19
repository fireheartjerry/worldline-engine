// BetaDecayTheory.hpp -- the Fermi theory of beta decay: the statistical phase-
// space factor and ft-values, the log ft classification (superallowed / allowed
// / forbidden), Fermi vs Gamow-Teller selection rules, the Fermi Coulomb
// correction function, the Kurie-plot linearisation, and double beta decay. This
// is the weak interaction acting inside the nucleus.
//
// Header-only, pure, deterministic. Energies in MeV, times in seconds.
//
// Sources:
//   - Fermi (1934) theory; allowed phase-space factor f ~ Q^5 (Sargent's rule).
//   - Comparative half-life ft and log ft systematics (superallowed Ft ~ 3072 s).
//   - Fermi (Vector) and Gamow-Teller (Axial) selection rules.
//   - Double beta decay phase space: 2nu ~ Q^11, 0nu ~ Q^5.

#ifndef COSMOS_BETADECAYTHEORY_HPP
#define COSMOS_BETADECAYTHEORY_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace beta {

// --- Phase space and ft-values ----------------------------------------------

// Allowed beta-decay statistical phase-space factor f ~ Q^5 (Sargent's rule:
// integrating the electron/neutrino phase space over the endpoint energy).
inline double phase_space_factor(double Q_mev) {
    if (Q_mev <= 0.0)
        return 0.0;
    return std::pow(Q_mev, 5.0);
}

// Comparative half-life ft = f * t_half [s], and its base-10 logarithm.
inline double ft_value(double f, double t_half_s) {
    return f * t_half_s;
}
inline double log_ft(double f, double t_half_s) {
    const double ft = f * t_half_s;
    return ft > 0.0 ? std::log10(ft) : -INFINITY;
}

// The superallowed 0+ -> 0+ Fermi transitions share a near-constant Ft ~ 3072 s
// (used to extract V_ud and test CVC).
inline constexpr double kSuperallowedFt_s = 3072.0;

enum class Transition { Superallowed, Allowed, FirstForbidden, HigherForbidden };

// Classify a transition by its log ft: superallowed (~2.9-3.7), allowed
// (~4.4-6), first-forbidden (~6-9), higher-forbidden (>9).
inline Transition classify_log_ft(double logft) {
    if (logft < 3.8)
        return Transition::Superallowed;
    if (logft < 6.0)
        return Transition::Allowed;
    if (logft < 9.0)
        return Transition::FirstForbidden;
    return Transition::HigherForbidden;
}

// --- Selection rules --------------------------------------------------------

// Fermi (vector) transitions: Delta J = 0, no parity change, and Delta T = 0.
inline bool is_allowed_fermi(int dJ, bool parity_change) {
    return dJ == 0 && !parity_change;
}

// Gamow-Teller (axial) transitions: Delta J = 0 or 1 (but NOT 0 -> 0), no parity
// change. Ji, Jf are twice the spins is unnecessary here; pass integer spins.
inline bool is_allowed_gamow_teller(int Ji, int Jf, bool parity_change) {
    if (parity_change)
        return false;
    const int dJ = std::abs(Ji - Jf);
    if (dJ > 1)
        return false;
    if (Ji == 0 && Jf == 0)
        return false; // 0 -> 0 forbidden for GT
    return true;
}

// Is the transition "allowed" (no parity change, Delta J <= 1)?
inline bool is_allowed(int Ji, int Jf, bool parity_change) {
    return !parity_change && std::abs(Ji - Jf) <= 1;
}

// --- Fermi Coulomb correction -----------------------------------------------

// Non-relativistic Fermi function F(Z, beta): distorts the emitted lepton
// spectrum by the daughter's Coulomb field. For beta-minus (electron) it
// ENHANCES low-energy emission (F > 1); for beta-plus (positron) it SUPPRESSES
// it (F < 1). eta = -/+ Z alpha / beta.
inline double fermi_function(int Z_daughter, double beta_velocity, bool is_electron) {
    if (beta_velocity <= 0.0)
        return 1.0;
    const double sign = is_electron ? +1.0 : -1.0; // electron attracted, positron repelled
    const double eta = sign * Z_daughter * constants::alpha / beta_velocity;
    const double x = 2.0 * constants::pi * eta;
    if (std::abs(x) < 1e-12)
        return 1.0;
    return x / (1.0 - std::exp(-x));
}

// --- Kurie plot -------------------------------------------------------------

// Kurie ordinate sqrt(N / (p^2 F)) is linear in the electron energy and hits zero
// at the endpoint Q (the spectral test for the neutrino mass). Here we return the
// linear factor (Q - T_e), which is >= 0 below the endpoint and 0 at it.
inline double kurie_linear(double Q_mev, double Te_mev) {
    return Q_mev - Te_mev;
}

// --- Double beta decay ------------------------------------------------------

// Two-neutrino double beta decay phase space ~ Q^11 (a second-order weak process,
// hence the astronomically long half-lives).
inline double double_beta_2nu_phase_space(double Q_mev) {
    if (Q_mev <= 0.0)
        return 0.0;
    return std::pow(Q_mev, 11.0);
}

// Neutrinoless double beta decay (if it exists) phase space ~ Q^5.
inline double double_beta_0nu_phase_space(double Q_mev) {
    if (Q_mev <= 0.0)
        return 0.0;
    return std::pow(Q_mev, 5.0);
}

} // namespace beta
} // namespace cosmos

#endif // COSMOS_BETADECAYTHEORY_HPP
