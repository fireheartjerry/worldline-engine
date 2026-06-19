// Verifies the deterministic analytic phylogeny module: byte-identical rebuilds,
// tip count tracking the closed-form logistic diversity curve (monotone, bounded
// [2,48]), an ULTRAMETRIC tree (all tips at a common depth), sister-clade naming
// coherence (small phylo distance -> small language distance), Brownian-motion
// trait variance growing with phylogenetic distance, and all-finite values.

#include "cosmos/Phylogeny.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;
using cosmos::phylo::CladePool;
using cosmos::phylo::build_clade_pool;
using cosmos::phylo::phylo_distance;
using cosmos::phylo::tip_count;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_phylogeny_verification FAILED: " << what << "\n"; ++g_failures; }
}

// L2 distance between two languages over the S_N style floats.
double lang_dist(const phon::Language& a, const phon::Language& b) {
    double s = 0.0;
    for (int i = 0; i < phon::S_N; ++i) {
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        s += d * d;
    }
    return std::sqrt(s);
}

// Euclidean distance between two trait centroids over the key continuous fields.
double trait_dist(const eco::SpeciesTraits& a, const eco::SpeciesTraits& b) {
    const double d0 = a.m - b.m;
    const double d1 = a.tau - b.tau;
    const double d2 = a.T_opt - b.T_opt;
    const double d3 = a.T_tol - b.T_tol;
    return std::sqrt(d0 * d0 + d1 * d1 + d2 * d2 + d3 * d3);
}

bool same_pool(const CladePool& a, const CladePool& b) {
    if (a.n_tips != b.n_tips || a.n_nodes != b.n_nodes) return false;
    if (a.parent.size() != b.parent.size()) return false;
    for (std::size_t i = 0; i < a.parent.size(); ++i) {
        if (a.parent[i] != b.parent[i]) return false;
        if (a.depth[i] != b.depth[i]) return false;
    }
    // Compare a representative tip language.
    if (!a.tip_node.empty()) {
        const phon::Language& la = a.lang[static_cast<std::size_t>(a.tip_node[0])];
        const phon::Language& lb = b.lang[static_cast<std::size_t>(b.tip_node[0])];
        for (int i = 0; i < phon::S_N; ++i) {
            if (la[i] != lb[i]) return false;
        }
    }
    return true;
}

bool all_finite(const CladePool& cp) {
    for (double d : cp.depth) {
        if (!std::isfinite(d)) return false;
    }
    for (const auto& c : cp.centroid) {
        if (!std::isfinite(c.m) || !std::isfinite(c.tau) || !std::isfinite(c.T_opt) ||
            !std::isfinite(c.T_tol) || !std::isfinite(c.omega) || !std::isfinite(c.dp) ||
            !std::isfinite(c.kappa) || !std::isfinite(c.rho) || !std::isfinite(c.phi) ||
            !std::isfinite(c.defense)) {
            return false;
        }
    }
    for (const auto& L : cp.lang) {
        for (int i = 0; i < phon::S_N; ++i) {
            if (!std::isfinite(static_cast<double>(L[i]))) return false;
        }
    }
    return true;
}

} // namespace

int main() {
    // --- Determinism ----------------------------------------------------------
    {
        const std::uint64_t seed = 0xC0FFEEull;
        const CladePool a = build_clade_pool(seed, 6.0, 24);
        const CladePool b = build_clade_pool(seed, 6.0, 24);
        check(same_pool(a, b), "same (seed, age, target) produces an identical pool");
    }

    // --- Bounds + tip count tracks the logistic prediction (monotone) ---------
    {
        const std::uint64_t seed = 0x1234567ull;
        const std::vector<double> ages = {0.1, 1.0, 3.0, 6.0, 12.0, 30.0};
        int prev = 0;
        bool monotone = true;
        bool bounded = true;
        for (double age : ages) {
            const CladePool cp = build_clade_pool(seed, age, 40);
            const int n = tip_count(cp);
            if (n < 2 || n > 48) bounded = false;
            if (n < prev) monotone = false;
            prev = n;
        }
        check(monotone, "tip count is monotone non-decreasing in biosphere age");
        check(bounded, "tip count stays within [2, 48]");

        // Young biosphere -> fewer tips than an old, saturated one.
        const CladePool young = build_clade_pool(seed, 0.2, 40);
        const CladePool old = build_clade_pool(seed, 40.0, 40);
        check(tip_count(old) >= tip_count(young), "older biosphere has >= tips than younger");

        // Target_tips clamp: requesting a huge target never exceeds 48.
        const CladePool huge = build_clade_pool(seed, 50.0, 9999);
        check(tip_count(huge) <= 48, "target_tips clamped to 48");
        const CladePool tiny = build_clade_pool(seed, 50.0, 0);
        check(tip_count(tiny) >= 2, "tip count floored at 2");
    }

    // --- Ultrametric: all tips at a common depth ------------------------------
    {
        const CladePool cp = build_clade_pool(0xABCDEFull, 8.0, 32);
        check(cp.n_tips >= 2, "pool has at least two tips");
        const double d0 = cp.depth[static_cast<std::size_t>(cp.tip_node[0])];
        bool ultrametric = true;
        for (int k = 0; k < cp.n_tips; ++k) {
            const double dk = cp.depth[static_cast<std::size_t>(cp.tip_node[static_cast<std::size_t>(k)])];
            if (std::abs(dk - d0) > 1.0e-6) ultrametric = false;
        }
        check(ultrametric, "all tip depths equal within 1e-6 (ultrametric)");

        // Root sits at depth 0; tips strictly deeper.
        check(cp.depth[0] == 0.0, "root depth is zero");
        check(d0 > 0.0, "tip depth is positive");
    }

    // --- Sister-clade naming: close tips sound more alike ---------------------
    {
        const CladePool cp = build_clade_pool(0x5151515ull, 10.0, 40);
        check(cp.n_tips >= 6, "enough tips for naming comparison");

        // Average language distance over the closest vs. farthest tip pairs.
        double near_lang = 0.0, far_lang = 0.0;
        int near_n = 0, far_n = 0;
        for (int i = 0; i < cp.n_tips; ++i) {
            for (int j = i + 1; j < cp.n_tips; ++j) {
                const double pd = phylo_distance(cp, i, j);
                const double ld = lang_dist(cp.lang[static_cast<std::size_t>(cp.tip_node[static_cast<std::size_t>(i)])],
                                            cp.lang[static_cast<std::size_t>(cp.tip_node[static_cast<std::size_t>(j)])]);
                // Threshold on patristic distance to separate sisters from distant tips.
                if (pd < 0.25 * cp.depth[static_cast<std::size_t>(cp.tip_node[0])]) {
                    near_lang += ld; ++near_n;
                } else {
                    far_lang += ld; ++far_n;
                }
            }
        }
        // Need both buckets populated to compare.
        if (near_n > 0 && far_n > 0) {
            check(near_lang / near_n < far_lang / far_n,
                  "phylogenetically close tips have smaller language distance");
        } else {
            check(near_n + far_n > 0, "tip pairs exist for naming comparison");
        }
    }

    // --- Brownian motion: trait difference grows with phylogenetic distance ---
    {
        const CladePool cp = build_clade_pool(0x7A7A7Aull, 12.0, 40);
        check(cp.n_tips >= 6, "enough tips for BM correlation");

        // Pearson-style sign check: covariance of (phylo_distance, trait_dist)
        // over all tip pairs should be positive.
        double sx = 0.0, sy = 0.0, n = 0.0;
        std::vector<double> xs, ys;
        for (int i = 0; i < cp.n_tips; ++i) {
            for (int j = i + 1; j < cp.n_tips; ++j) {
                const double pd = phylo_distance(cp, i, j);
                const double td = trait_dist(cp.centroid[static_cast<std::size_t>(cp.tip_node[static_cast<std::size_t>(i)])],
                                             cp.centroid[static_cast<std::size_t>(cp.tip_node[static_cast<std::size_t>(j)])]);
                xs.push_back(pd); ys.push_back(td);
                sx += pd; sy += td; n += 1.0;
            }
        }
        const double mx = sx / n, my = sy / n;
        double cov = 0.0;
        for (std::size_t k = 0; k < xs.size(); ++k) {
            cov += (xs[k] - mx) * (ys[k] - my);
        }
        check(cov > 0.0, "trait difference correlates positively with phylogenetic distance");
    }

    // --- All values finite ----------------------------------------------------
    {
        const CladePool cp = build_clade_pool(0xDEADBEEFull, 7.5, 36);
        check(all_finite(cp), "all depths, traits and language floats are finite");
    }

    if (g_failures != 0) {
        std::cerr << "cosmos_phylogeny_verification: " << g_failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_phylogeny_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
