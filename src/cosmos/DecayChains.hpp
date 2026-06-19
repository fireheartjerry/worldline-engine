// DecayChains.hpp -- radioactive nuclei rarely decay alone; they cascade. This
// module solves the Bateman equations for a decay chain, classifies secular and
// transient equilibrium, and identifies the four natural decay series (the 4n,
// 4n+1, 4n+2, 4n+3 families) and how many alpha/beta steps carry a parent down
// to its stable end-point.
//
// Header-only, pure, deterministic. Decay constants in 1/s, times in s.
//
// Sources:
//   - Bateman (1910) solution for a linear decay chain.
//   - Secular equilibrium (t_half,parent >> t_half,daughter): equal activities.
//   - The four radioactive series keyed by A mod 4.

#ifndef COSMOS_DECAYCHAINS_HPP
#define COSMOS_DECAYCHAINS_HPP

#include "cosmos/NuclearDecay.hpp"

#include <cmath>

namespace cosmos {
namespace chains {

// --- Two-step Bateman A -> B -> C -------------------------------------------

// Daughter population N_B(t) for a pure parent start N_A(0)=N0, N_B(0)=0:
//   N_B(t) = N0 lambda_A / (lambda_B - lambda_A) (e^{-lambda_A t} - e^{-lambda_B t}).
inline double bateman_daughter(double N0, double lambda_A, double lambda_B, double t) {
    if (std::abs(lambda_B - lambda_A) < 1e-30) {
        // Degenerate (equal rates): N_B = N0 lambda t e^{-lambda t}.
        return N0 * lambda_A * t * std::exp(-lambda_A * t);
    }
    return N0 * lambda_A / (lambda_B - lambda_A) *
           (std::exp(-lambda_A * t) - std::exp(-lambda_B * t));
}

// Parent population N_A(t) = N0 e^{-lambda_A t}.
inline double parent_population(double N0, double lambda_A, double t) {
    return N0 * std::exp(-lambda_A * t);
}

// --- Equilibrium classification ---------------------------------------------

enum class Equilibrium { Secular, Transient, None };

// Secular: parent far longer-lived than daughter (lambda_A << lambda_B); after a
// few daughter half-lives the activities equalise. Transient: parent somewhat
// longer-lived. None: parent shorter-lived than the daughter (no equilibrium).
inline Equilibrium classify_equilibrium(double lambda_A, double lambda_B) {
    if (lambda_A >= lambda_B)
        return Equilibrium::None;
    if (lambda_A < 0.01 * lambda_B)
        return Equilibrium::Secular;
    return Equilibrium::Transient;
}

// In secular equilibrium the daughter activity approaches the parent activity:
//   A_B / A_A -> lambda_B / (lambda_B - lambda_A)  ~ 1 for lambda_A << lambda_B.
inline double secular_activity_ratio(double lambda_A, double lambda_B) {
    if (lambda_B <= lambda_A)
        return INFINITY;
    return lambda_B / (lambda_B - lambda_A);
}

// --- The four natural decay series ------------------------------------------

// Alpha decay lowers A by 4; beta decay leaves A unchanged. So A mod 4 is a
// conserved label, giving exactly four series.
enum class Series { Thorium4n, Neptunium4n1, Uranium4n2, Actinium4n3 };

inline Series series_for_A(int A) {
    switch (((A % 4) + 4) % 4) {
    case 0:
        return Series::Thorium4n;
    case 1:
        return Series::Neptunium4n1;
    case 2:
        return Series::Uranium4n2;
    default:
        return Series::Actinium4n3;
    }
}

inline const char *series_name(Series s) {
    switch (s) {
    case Series::Thorium4n:
        return "Thorium (4n)";
    case Series::Neptunium4n1:
        return "Neptunium (4n+1)";
    case Series::Uranium4n2:
        return "Uranium (4n+2)";
    case Series::Actinium4n3:
        return "Actinium (4n+3)";
    }
    return "";
}

// Number of alpha decays from a parent (Z_p,A_p) to a stable end-point
// (Z_s,A_s): each alpha removes (2 protons, 4 nucleons), so n_alpha = (A_p-A_s)/4.
inline int alpha_count(int A_parent, int A_stable) {
    return (A_parent - A_stable) / 4;
}

// Number of beta-minus decays needed to reach the end-point charge:
//   each alpha lowers Z by 2; the remaining Z change is made up by beta-minus.
//   n_beta = (Z_s - (Z_p - 2 n_alpha)) = Z_s - Z_p + 2 n_alpha.
inline int beta_minus_count(int Z_parent, int A_parent, int Z_stable, int A_stable) {
    const int na = alpha_count(A_parent, A_stable);
    return Z_stable - Z_parent + 2 * na;
}

} // namespace chains
} // namespace cosmos

#endif // COSMOS_DECAYCHAINS_HPP
