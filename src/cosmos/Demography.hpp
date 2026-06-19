#pragma once
// Structured demography: a 3-stage Lefkovitch (juvenile / subadult / adult)
// projection matrix derived from a species' life history, plus its stable stage
// distribution (the dominant right eigenvector). This is a DISPLAY overlay — the
// live sim's master state stays the scalar biomass x; demography only partitions
// it into age classes and exposes survivorship type (I/II/III by the r<->K axis).
//
// Lefkovitch matrix (Caswell 2001):
//     n_{t+1} = A n_t,   A = [[P_j, 0,   F ],
//                             [G_j, P_s, 0 ],
//                             [0,   G_s, P_a]]
// where P_* are stage-survival (stay) probabilities, G_* growth (advance)
// probabilities, and F adult fecundity. r-strategists (rho->1) get Type III
// survivorship (heavy juvenile loss, high fecundity); K-strategists (rho->0)
// get Type I (high adult survival, low fecundity).

#include "cosmos/Ecosystem.hpp" // eco::SpeciesTraits
#include "cosmos/Organism.hpp"  // bio::Organism

#include <algorithm>
#include <array>
#include <cmath>

namespace cosmos {
namespace demo {

struct StageModel {
    // Row-major 3x3 Lefkovitch matrix.
    std::array<double, 9> A{};
    std::array<double, 3> stable{};   // stable stage distribution (sums to 1)
    double lambda = 1.0;              // dominant eigenvalue (pop growth rate)
    double survivorship = 1.0;        // 1=Type I (K) .. 3=Type III (r)
};

namespace detail {
inline double clampd(double x, double lo, double hi) { return std::min(hi, std::max(lo, x)); }
}

// Build the stage model from continuous traits (+ derived organism for timescale).
inline StageModel stage_model(const eco::SpeciesTraits& t, const bio::Organism& org) {
    using detail::clampd;
    const double rho = clampd(t.rho, 0.0, 1.0);

    // Survival: K-strategists keep adults alive for many seasons; r-strategists
    // turn over fast and lose most juveniles.
    const double P_a = clampd(0.92 - 0.55 * rho, 0.05, 0.97);  // adult stay-alive
    const double P_s = clampd(0.80 - 0.45 * rho, 0.05, 0.95);  // subadult stay
    const double P_j = clampd(0.55 - 0.40 * rho, 0.02, 0.90);  // juvenile stay
    const double G_j = clampd(0.45 * (0.4 + 0.6 * (1.0 - rho)), 0.02, 0.95); // juv->subadult
    const double G_s = clampd(0.50 * (0.4 + 0.6 * (1.0 - rho)), 0.02, 0.95); // subadult->adult
    // Fecundity rises steeply with the r-strategy (many small young).
    const double F = clampd(0.3 + 6.0 * rho * rho, 0.05, 12.0);

    StageModel m;
    m.A = {P_j, 0.0, F,
           G_j, P_s, 0.0,
           0.0, G_s, P_a};

    // Dominant eigenpair by power iteration (3x3, converges in a few steps).
    std::array<double, 3> v{1.0, 1.0, 1.0};
    double lam = 1.0;
    for (int iter = 0; iter < 200; ++iter) {
        std::array<double, 3> w{};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) w[static_cast<std::size_t>(i)] += m.A[static_cast<std::size_t>(i * 3 + j)] * v[static_cast<std::size_t>(j)];
        const double norm = std::abs(w[0]) + std::abs(w[1]) + std::abs(w[2]);
        if (norm < 1e-300) break;
        lam = norm; // L1-normalized growth proxy; ratio below gives the eigenvalue
        for (int i = 0; i < 3; ++i) w[static_cast<std::size_t>(i)] /= norm;
        v = w;
    }
    // Eigenvalue as Rayleigh-style ratio ||Av||/||v|| with v normalized to sum 1.
    const double vs = v[0] + v[1] + v[2];
    if (vs > 0.0) for (double& x : v) x /= vs;
    std::array<double, 3> Av{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) Av[static_cast<std::size_t>(i)] += m.A[static_cast<std::size_t>(i * 3 + j)] * v[static_cast<std::size_t>(j)];
    m.lambda = (lam > 0.0) ? (Av[0] + Av[1] + Av[2]) : 1.0;
    m.stable = v;
    // Survivorship type: juvenile-heavy stable distribution => Type III.
    m.survivorship = clampd(1.0 + 2.0 * v[0], 1.0, 3.0);
    (void)org; // organism reserved for absolute-timescale extensions
    return m;
}

} // namespace demo
} // namespace cosmos
