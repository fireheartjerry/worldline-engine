#pragma once
// Multi-interaction ecology helpers — generalize a predation-only food web into a
// SIGNED community matrix. Each pairwise interaction is classified (predation,
// competition, mutualism, parasitism, neutral) and quantified as a per-capita
// signed effect, derived purely from continuous trait overlap (body mass, thermal
// niche, diel phase, trophic position, defense). Also: May's local-stability
// criterion and a carbon/nitrogen detritus mass-balance check.
//
// These are self-contained, pure, deterministic helpers (no RNG, no state). The
// integration into generate_community() happens elsewhere; nothing here mutates a
// Community. Magnitudes are tuned so competition/mutualism stay SMALL relative to
// predation, keeping the realized community matrix May-stable.

#include "cosmos/Ecosystem.hpp" // eco::SpeciesTraits
#include "cosmos/Spectrum.hpp"  // clampd, lerp, kTwoPi

#include <cmath>

namespace cosmos {
namespace interact {

inline double sq(double x) { return x * x; }

// Gaussian similarity in [0,1]: 1 when d==0, decaying with (d/scale)^2.
inline double gaussian_overlap(double d, double scale) {
    const double s = scale > 1.0e-9 ? scale : 1.0e-9;
    return std::exp(-0.5 * sq(d / s));
}

// Circular distance between two diel phases in [0,1) -> [0,0.5].
inline double diel_distance(double phi_a, double phi_b) {
    double d = std::fabs(phi_a - phi_b);
    d = d - std::floor(d); // wrap into [0,1)
    return d > 0.5 ? 1.0 - d : d;
}

// Diel-activity overlap in [0,1]: 1 when in phase, ~0 when antiphase (0.5 apart).
inline double diel_overlap(double phi_a, double phi_b) {
    // map circular distance [0,0.5] to a cosine bump.
    const double d = diel_distance(phi_a, phi_b);
    return 0.5 * (1.0 + std::cos(kTwoPi * d));
}

// Climate (thermal niche) overlap in [0,1]: Gaussian in the temperature-optimum
// gap, scaled by the combined thermal tolerances.
inline double climate_overlap(const eco::SpeciesTraits& a, const eco::SpeciesTraits& b) {
    const double tol = 0.5 * (std::fabs(a.T_tol) + std::fabs(b.T_tol)) + 1.0;
    return gaussian_overlap(a.T_opt - b.T_opt, tol);
}

enum class Interaction { Predation, Competition, Mutualism, Parasitism, Neutral };

struct PairEffect {
    Interaction type = Interaction::Neutral;
    double on_i = 0.0; // signed per-capita effect of j on i
    double on_j = 0.0; // signed per-capita effect of i on j
};

struct NutrientPool {
    double carbon = 0.0;
    double nitrogen = 0.0;
};

// Niche overlap in [0,1] — product of Gaussian overlaps in body mass, thermal
// niche, and diel phase. Two species with similar mass, climate and activity have
// overlap near 1; they share resources and compete strongly when co-trophic.
inline double niche_overlap(const eco::SpeciesTraits& a, const eco::SpeciesTraits& b) {
    const double mass = gaussian_overlap(a.m - b.m, 1.5);     // ~1.5 dex scale
    const double clim = climate_overlap(a, b);
    const double diel = diel_overlap(a.phi, b.phi);
    return clampd(mass * clim * diel, 0.0, 1.0);
}

// Size-structured predation kernel (>=0). Predator must be meaningfully larger
// than prey (m gap > ~0.1 dex); the size term peaks when the predator is pred.dp
// dex larger than the prey (the preferred predator/prey mass offset), with a width
// set by the predator's diet breadth omega. Modulated by climate overlap, diel
// overlap, and prey defense penetration (1-defense)^(1+2*rho).
inline double size_predation_kernel(const eco::SpeciesTraits& pred, const eco::SpeciesTraits& prey) {
    const double gap = pred.m - prey.m;
    if (gap <= 0.1) return 0.0; // predator not larger -> no feeding link

    const double width = std::fabs(pred.omega) + 0.25; // dex; avoid zero width
    const double size = gaussian_overlap(gap - pred.dp, width);

    const double clim = climate_overlap(pred, prey);
    const double diel = diel_overlap(pred.phi, prey.phi);

    const double def = clampd(prey.defense, 0.0, 1.0);
    const double pen = std::pow(1.0 - def, 1.0 + 2.0 * clampd(pred.rho, 0.0, 1.0));

    const double k = size * clim * diel * pen * std::fabs(pred.kappa);
    return k > 0.0 ? k : 0.0;
}

// Classify the pairwise interaction between species i and j and return the signed
// per-capita effects. Deterministic function of traits only — no RNG. Predation
// dominates magnitude; competition/mutualism/parasitism are kept small so the
// signed community matrix stays May-stable.
inline PairEffect classify(const eco::SpeciesTraits& i, const eco::SpeciesTraits& j) {
    PairEffect e;

    // Predation: whichever species is larger AND trophically above the other,
    // within the size-predation band. Consumer gains (+), resource loses (-).
    const double k_ij = size_predation_kernel(i, j); // i eats j
    const double k_ji = size_predation_kernel(j, i); // j eats i
    const double dtau = i.tau - j.tau;

    const bool i_above = (k_ij > 0.0) && (dtau > 0.25);
    const bool j_above = (k_ji > 0.0) && (-dtau > 0.25);

    if (i_above || j_above) {
        // Pick the stronger / clearer direction if both could feed.
        const bool i_is_consumer = i_above && (!j_above || k_ij >= k_ji);
        const double k = i_is_consumer ? k_ij : k_ji;
        const double assim = 0.5; // consumer assimilation efficiency of the loss
        if (i_is_consumer) {
            e.type = Interaction::Predation;
            e.on_i = assim * k; // i (consumer) gains
            e.on_j = -k;        // j (resource) loses
        } else {
            e.type = Interaction::Predation;
            e.on_j = assim * k; // j (consumer) gains
            e.on_i = -k;        // i (resource) loses
        }
        return e;
    }

    // Parasitism: a tiny consumer on a much larger host with low defense. The
    // parasite is far smaller (negative size gap), trophically just above, and the
    // host's defense barely impedes it. Parasite gains small (+), host loses small.
    const double host_gap = std::fabs(i.m - j.m);
    if (host_gap > 1.5 && std::fabs(dtau) > 0.1) {
        const eco::SpeciesTraits& parasite = (i.m < j.m) ? i : j;
        const eco::SpeciesTraits& host     = (i.m < j.m) ? j : i;
        const double clim = climate_overlap(parasite, host);
        const double pen  = 1.0 - clampd(host.defense, 0.0, 1.0);
        if (clim > 0.4 && pen > 0.6) {
            const double k = 0.05 * clim * pen; // deliberately small
            if (&parasite == &i) { e.on_i = k; e.on_j = -0.25 * k; }
            else                 { e.on_j = k; e.on_i = -0.25 * k; }
            e.type = Interaction::Parasitism;
            return e;
        }
    }

    // Competition: similar trophic level and high niche overlap -> both lose. Kept
    // small relative to predation.
    const double ov = niche_overlap(i, j);
    if (std::fabs(dtau) < 0.5 && ov > 0.5) {
        const double k = 0.10 * ov; // small negative coupling
        e.type = Interaction::Competition;
        e.on_i = -k;
        e.on_j = -k;
        return e;
    }

    // Mutualism: rare complementary pair — very different trophic role and body
    // size, and opposite diel phase (one diurnal, one nocturnal). Both gain a
    // little. Smallest of all signed effects.
    const double diel_d = diel_distance(i.phi, j.phi);
    if (std::fabs(dtau) > 1.0 && host_gap > 1.5 && diel_d > 0.35) {
        const double clim = climate_overlap(i, j);
        const double k = 0.03 * clim * (diel_d / 0.5); // tiny
        e.type = Interaction::Mutualism;
        e.on_i = k;
        e.on_j = k;
        return e;
    }

    // Otherwise no meaningful interaction.
    e.type = Interaction::Neutral;
    return e;
}

// May's local-stability criterion for a random community matrix: a system of S
// species with connectance C and interaction strengths of standard deviation
// sd is almost surely stable when sd*sqrt(S*C) < 1. The returned margin is
// 1 - sd*sqrt(S*C); >0 = stable, decreasing in S, C and sd. mean_interaction is
// accepted for API completeness (May's bound is driven by the variance) and does
// not affect the diagonal-normalized margin.
inline double may_stability_margin(int S, double connectance, double mean_interaction,
                                   double sd_interaction) {
    (void)mean_interaction;
    const double Sd = static_cast<double>(S > 0 ? S : 0);
    const double C  = connectance > 0.0 ? connectance : 0.0;
    const double sd = sd_interaction > 0.0 ? sd_interaction : 0.0;
    return 1.0 - sd * std::sqrt(Sd * C);
}

// Detritus loop mass-balance residual: for a conserved pool, inflow minus outflow
// must equal the change in stock. Returns inflow - outflow - delta_stock (~0 when
// the carbon/nitrogen books balance).
inline double detritus_balance_residual(double inflow, double outflow, double delta_stock) {
    return inflow - outflow - delta_stock;
}

inline bool is_balanced(double residual, double tol = 1e-9) {
    return std::fabs(residual) <= tol;
}

} // namespace interact
} // namespace cosmos
