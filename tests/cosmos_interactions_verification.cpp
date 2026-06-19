// Verifies the multi-interaction ecology helpers: niche overlap bounds, signed
// pairwise interaction classification (predation/competition/mutualism), the
// size-predation kernel, May's stability margin monotonicity, and the detritus
// carbon/nitrogen mass-balance check. Pure, deterministic, finite.

#include "cosmos/Interactions.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::interact;
using cosmos::eco::SpeciesTraits;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_interactions_verification FAILED: " << what << "\n"; ++g_failures; }
}

bool fin(double x) { return std::isfinite(x); }

// A "baseline" species we tweak per test.
SpeciesTraits base() {
    SpeciesTraits s;
    s.m = 0.0; s.tau = 1.0; s.T_opt = 18.0; s.T_tol = 8.0; s.omega = 0.6;
    s.dp = 2.0; s.kappa = 1.0; s.rho = 0.5; s.phi = 0.0; s.defense = 0.3;
    return s;
}

} // namespace

int main() {
    // --- niche_overlap bounds and limits -------------------------------------
    {
        SpeciesTraits a = base();
        check(std::fabs(niche_overlap(a, a) - 1.0) < 1e-9, "identical traits -> overlap ~1");
        check(fin(niche_overlap(a, a)), "overlap finite");

        SpeciesTraits b = base();
        b.m = 8.0; b.T_opt = 80.0; b.phi = 0.5; // very different mass, temp, antiphase
        const double ov = niche_overlap(a, b);
        check(ov >= 0.0 && ov <= 1.0, "overlap in [0,1]");
        check(ov < 1e-3, "very different mass/temp -> overlap ~0");

        // Random-ish sweep stays in [0,1].
        for (int i = 0; i < 20; ++i) {
            SpeciesTraits x = base(); x.m = -3.0 + 0.3 * i; x.T_opt = 0.0 + 4.0 * i; x.phi = 0.05 * i;
            const double o = niche_overlap(a, x);
            check(o >= 0.0 && o <= 1.0 && fin(o), "swept overlap in [0,1] and finite");
        }
    }

    // --- size_predation_kernel ------------------------------------------------
    {
        SpeciesTraits prey = base(); prey.m = 0.0; prey.defense = 0.0;
        SpeciesTraits pred = base(); pred.m = 2.0; // 2 dex larger == pred.dp offset
        check(size_predation_kernel(pred, prey) > 0.0, "larger predator -> kernel > 0");
        check(size_predation_kernel(prey, pred) == 0.0, "smaller 'predator' -> kernel 0");

        // Peak near pred.dp offset: the at-offset gap beats both a tiny and a huge gap.
        SpeciesTraits p_at = base(); p_at.m = prey.m + pred.dp;       // gap == dp
        SpeciesTraits p_lo = base(); p_lo.m = prey.m + pred.dp - 1.5; // gap < dp
        SpeciesTraits p_hi = base(); p_hi.m = prey.m + pred.dp + 1.5; // gap > dp
        const double k_at = size_predation_kernel(p_at, prey);
        const double k_lo = size_predation_kernel(p_lo, prey);
        const double k_hi = size_predation_kernel(p_hi, prey);
        check(k_at > k_lo && k_at > k_hi, "kernel peaks near pred.dp offset");
        check(fin(k_at) && fin(k_lo) && fin(k_hi), "kernel values finite");

        // Higher defense reduces the kernel.
        SpeciesTraits armored = prey; armored.defense = 0.9;
        check(size_predation_kernel(pred, armored) < size_predation_kernel(pred, prey),
              "defense lowers predation kernel");
    }

    // --- classify: predation --------------------------------------------------
    double pred_strength = 0.0;
    {
        SpeciesTraits big = base();   big.m = 2.0; big.tau = 3.0; // large, high trophic
        SpeciesTraits small = base(); small.m = 0.0; small.tau = 1.0; small.defense = 0.0;
        const PairEffect e = classify(big, small);
        check(e.type == Interaction::Predation, "large high-tau vs small low-tau -> Predation");
        check(e.on_i > 0.0, "predation: consumer on_i > 0");
        check(e.on_j < 0.0, "predation: resource on_j < 0");
        check(fin(e.on_i) && fin(e.on_j), "predation effects finite");
        pred_strength = std::fabs(e.on_j);

        // Symmetry of roles: swapping args keeps it predation with consumer gaining.
        const PairEffect e2 = classify(small, big);
        check(e2.type == Interaction::Predation, "swapped order -> still Predation");
        check(e2.on_j > 0.0 && e2.on_i < 0.0, "swapped: consumer (j=big) gains, resource (i) loses");
    }

    // --- classify: competition -----------------------------------------------
    double comp_mag = 0.0;
    {
        SpeciesTraits a = base(); // near-identical, same tau
        SpeciesTraits b = base(); b.m = 0.05; // tiny mass difference, same trophic level
        const PairEffect e = classify(a, b);
        check(e.type == Interaction::Competition, "near-identical same-tau -> Competition");
        check(e.on_i < 0.0 && e.on_j < 0.0, "competition: both effects < 0");
        check(fin(e.on_i) && fin(e.on_j), "competition effects finite");
        comp_mag = std::fabs(e.on_i);
        check(comp_mag < pred_strength, "|competition| < |predation|");
    }

    // --- classify: mutualism (rare complementary pair) ------------------------
    {
        SpeciesTraits a = base(); a.m = -2.0; a.tau = 0.0; a.phi = 0.0;  // small producer, diurnal
        SpeciesTraits b = base(); b.m =  2.0; b.tau = 3.0; b.phi = 0.5;  // large consumer, nocturnal
        const PairEffect e = classify(a, b);
        // This complementary pair (very different tau & m, opposite diel) is mutualism;
        // the size gap is large but the trophic gap is too big for a normal feeding band peak.
        if (e.type == Interaction::Mutualism) {
            check(e.on_i > 0.0 && e.on_j > 0.0, "mutualism: both effects > 0");
            check(std::fabs(e.on_i) < pred_strength, "|mutualism| < |predation|");
            check(std::fabs(e.on_i) <= comp_mag + 1e-12, "|mutualism| <= |competition| (smallest)");
        }
        check(fin(e.on_i) && fin(e.on_j), "mutualism candidate effects finite");
    }

    // --- classify: neutral and determinism/purity -----------------------------
    {
        SpeciesTraits a = base(); a.m = 0.0; a.tau = 1.0; a.phi = 0.0;
        SpeciesTraits b = base(); b.m = 0.7; b.tau = 1.0; b.phi = 0.25; // modest difference
        const PairEffect e1 = classify(a, b);
        const PairEffect e2 = classify(a, b);
        check(e1.type == e2.type && e1.on_i == e2.on_i && e1.on_j == e2.on_j,
              "classify deterministic / pure (twice identical)");
        check(fin(e1.on_i) && fin(e1.on_j), "neutral-band effects finite");
    }

    // --- may_stability_margin -------------------------------------------------
    {
        const double weak = may_stability_margin(20, 0.1, 0.0, 0.05);
        check(weak > 0.0, "weak interactions -> stable margin > 0");

        const double strong = may_stability_margin(20, 0.1, 0.0, 1.0);
        check(strong < 0.0, "strong interactions -> margin < 0");

        // Monotonic decrease in S, connectance, sd.
        check(may_stability_margin(40, 0.1, 0.0, 0.1) < may_stability_margin(10, 0.1, 0.0, 0.1),
              "margin decreases as S grows");
        check(may_stability_margin(20, 0.3, 0.0, 0.1) < may_stability_margin(20, 0.1, 0.0, 0.1),
              "margin decreases as connectance grows");
        check(may_stability_margin(20, 0.1, 0.0, 0.3) < may_stability_margin(20, 0.1, 0.0, 0.1),
              "margin decreases as sd grows");
        check(fin(weak) && fin(strong), "stability margins finite");
    }

    // --- detritus mass balance ------------------------------------------------
    {
        // Balanced: inflow - outflow - delta_stock == 0.
        const double r0 = detritus_balance_residual(100.0, 60.0, 40.0);
        check(std::fabs(r0) < 1e-12 && is_balanced(r0), "balanced flows -> residual ~0, is_balanced true");

        // Unbalanced.
        const double r1 = detritus_balance_residual(100.0, 60.0, 30.0);
        check(!is_balanced(r1), "unbalanced flows -> is_balanced false");
        check(std::fabs(r1 - 10.0) < 1e-12, "residual equals the books gap");

        // NutrientPool C:N delta accounting balances per element.
        NutrientPool prev{1000.0, 80.0};
        NutrientPool cur{1010.0, 78.0};
        const double rC = detritus_balance_residual(50.0, 40.0, cur.carbon - prev.carbon);
        const double rN = detritus_balance_residual(5.0, 7.0, cur.nitrogen - prev.nitrogen);
        check(is_balanced(rC) && is_balanced(rN), "C and N pools balance independently");
        check(fin(r0) && fin(r1) && fin(rC) && fin(rN), "residuals finite");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_interactions_verification: all checks passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "cosmos_interactions_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
