#pragma once
// Deterministic phylogeny ("clade pool") reconstructed ANALYTICALLY — no
// generation-by-generation time stepping. The standing extant tip count is read
// from the closed-form logistic diversity-dependent diversification curve
// N(t)=K/(1+((K-N0)/N0)e^{-r t}); the tree itself is an ULTRAMETRIC Yule tree
// built by recursive binary splitting with waiting times ~ 1/(k*lambda), so all
// tips end at the same depth (the present). Languages drift down each branch so
// sister tips share a phonetic family (derive_language); trait centroids evolve
// by Brownian motion (variance ∝ branch length). Everything is a pure function
// of a 64-bit seed: same (seed, age, target) -> byte-identical pool.

#include "cosmos/Spectrum.hpp"
#include "cosmos/Phonology.hpp"
#include "cosmos/Ecosystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cosmos {
namespace phylo {

// Hard caps (flat-array sizing): tips <= 48, total nodes <= 2*48.
constexpr int kMaxTips  = 48;
constexpr int kMaxNodes = 2 * kMaxTips;

// A reconstructed clade as flat parallel arrays. Node 0 is the root stem.
// Internal nodes precede / interleave their children only by construction order;
// `parent[i]` gives the topology (parent[0] == -1 for the root). `depth[i]` is
// the time from the root to node i (root depth 0); tips share a common depth
// (ultrametric). `tip_node[k]` maps tip index k -> node index.
struct CladePool {
    int n_nodes = 0;
    int n_tips = 0;
    std::vector<int> parent;                       // [n_nodes], parent[0] == -1
    std::vector<double> depth;                      // [n_nodes], time from root
    std::vector<eco::SpeciesTraits> centroid;       // [n_nodes]
    std::vector<phon::Language> lang;               // [n_nodes]
    std::vector<int> tip_node;                       // [n_tips]
};

namespace detail {

// Closed-form logistic diversity: N(t) = K/(1+((K-N0)/N0) e^{-r t}).
inline double logistic_diversity(double K, double N0, double r, double t) {
    const double denom = 1.0 + ((K - N0) / N0) * std::exp(-r * t);
    return K / denom;
}

// Clamp one trait field to a sane range (in place).
inline void clamp_traits(eco::SpeciesTraits& s) {
    s.m       = clampd(s.m, -6.0, 5.0);
    s.tau     = clampd(s.tau, 0.0, 5.0);
    s.T_opt   = clampd(s.T_opt, -40.0, 120.0);
    s.T_tol   = clampd(s.T_tol, 0.5, 60.0);
    s.omega   = clampd(s.omega, 0.05, 4.0);
    s.dp      = clampd(s.dp, 0.0, 8.0);
    s.kappa   = clampd(s.kappa, 0.05, 8.0);
    s.rho     = clampd(s.rho, 0.0, 1.0);
    s.phi     = clampd(s.phi, 0.0, 1.0);
    s.defense = clampd(s.defense, 0.0, 1.0);
}

} // namespace detail

// Patristic distance: sum of branch lengths from tipA up to the MRCA and back
// down to tipB. Returns 0 if the tips are identical or indices are invalid.
inline double phylo_distance(const CladePool& cp, int tipA, int tipB) {
    if (tipA < 0 || tipB < 0 || tipA >= cp.n_tips || tipB >= cp.n_tips) return 0.0;
    int a = cp.tip_node[static_cast<std::size_t>(tipA)];
    int b = cp.tip_node[static_cast<std::size_t>(tipB)];
    if (a == b) return 0.0;

    // Collect the ancestor path of a (including a itself) up to the root.
    std::vector<int> path_a;
    path_a.reserve(static_cast<std::size_t>(cp.n_nodes));
    for (int x = a; x != -1; x = cp.parent[static_cast<std::size_t>(x)]) path_a.push_back(x);

    // Walk b upward until we hit a node on a's path: that node is the MRCA.
    int mrca = 0;
    for (int x = b; x != -1; x = cp.parent[static_cast<std::size_t>(x)]) {
        bool found = false;
        for (int y : path_a) {
            if (y == x) { found = true; break; }
        }
        if (found) { mrca = x; break; }
    }

    const double dm = cp.depth[static_cast<std::size_t>(mrca)];
    return (cp.depth[static_cast<std::size_t>(a)] - dm) +
           (cp.depth[static_cast<std::size_t>(b)] - dm);
}

inline int tip_count(const CladePool& cp) { return cp.n_tips; }

// Build a deterministic ultrametric birth-death clade pool.
inline CladePool build_clade_pool(std::uint64_t seed, double biosphere_age_gyr,
                                  int target_tips) {
    CladePool cp;

    // --- Standing extant tip count from the closed-form logistic curve --------
    const double K  = clampd(static_cast<double>(target_tips), 2.0, static_cast<double>(kMaxTips));
    const double N0 = 1.0;
    const double r  = 0.4;
    const double t  = std::max(0.0, biosphere_age_gyr);
    const double Nt = detail::logistic_diversity(K, N0, r, t);

    int n_tips = static_cast<int>(std::lround(Nt));
    n_tips = std::max(2, std::min(n_tips, kMaxTips));
    cp.n_tips = n_tips;

    // --- Topology: ultrametric Yule tree by recursive binary splitting --------
    // We process a queue of "live" lineages. At split k (k = current number of
    // live lineages) the waiting time to the next split is ~ 1/(k*lambda). We
    // accumulate split times, then normalize so the tallest tip sits at the
    // present and back-fill every tip to a common depth (ultrametric).
    const double lambda = 1.0;

    // Reserve flat arrays for the worst case.
    cp.parent.reserve(static_cast<std::size_t>(kMaxNodes));
    cp.depth.reserve(static_cast<std::size_t>(kMaxNodes));
    cp.centroid.reserve(static_cast<std::size_t>(kMaxNodes));
    cp.lang.reserve(static_cast<std::size_t>(kMaxNodes));

    // Root stem (node 0).
    cp.parent.push_back(-1);
    cp.depth.push_back(0.0);

    // live[] holds node indices of lineages currently available to split; the
    // companion accumulated split "time" axis grows as we add splits.
    std::vector<int> live;
    live.reserve(static_cast<std::size_t>(kMaxTips));
    live.push_back(0);

    // Cumulative split time from the root. The first split happens at t1 after
    // the root; subsequent ones add 1/(k*lambda) each.
    double cum_time = 0.0;
    Stream split_stream(seed, 0x9C1AD3ull);

    // Keep splitting a lineage into two children until we reach n_tips lineages.
    while (static_cast<int>(live.size()) < n_tips) {
        const int k = static_cast<int>(live.size());
        // Yule waiting time to the next split: exponential mean 1/(k*lambda).
        const double u = std::max(1.0e-12, 1.0 - split_stream.unit());
        const double wait = -std::log(u) / (static_cast<double>(k) * lambda);
        cum_time += wait;

        // Choose which live lineage splits (deterministic, seed-driven).
        const int which = static_cast<int>(split_stream.range(0.0, static_cast<double>(k)));
        const int pick = which >= k ? k - 1 : which;
        const int parent_node = live[static_cast<std::size_t>(pick)];

        // Create two children at depth cum_time.
        const int childL = static_cast<int>(cp.parent.size());
        cp.parent.push_back(parent_node);
        cp.depth.push_back(cum_time);
        const int childR = static_cast<int>(cp.parent.size());
        cp.parent.push_back(parent_node);
        cp.depth.push_back(cum_time);

        // The split node is no longer live; its two children are.
        live[static_cast<std::size_t>(pick)] = childL;
        live.push_back(childR);
    }

    // Total tree height: the present is just past the last split. Add one more
    // Yule waiting time so tips have a non-zero terminal branch.
    {
        const int k = static_cast<int>(live.size());
        const double u = std::max(1.0e-12, 1.0 - split_stream.unit());
        const double wait = -std::log(u) / (static_cast<double>(k) * lambda);
        cum_time += wait;
    }
    const double total_height = std::max(1.0e-9, cum_time);

    cp.n_nodes = static_cast<int>(cp.parent.size());

    // Record tip nodes (the live lineages) and pin them to the common present
    // depth so the tree is ULTRAMETRIC.
    cp.tip_node.reserve(static_cast<std::size_t>(n_tips));
    for (int node : live) {
        cp.tip_node.push_back(node);
        cp.depth[static_cast<std::size_t>(node)] = total_height;
    }

    // --- Languages and trait centroids down each branch -----------------------
    cp.lang.resize(static_cast<std::size_t>(cp.n_nodes));
    cp.centroid.resize(static_cast<std::size_t>(cp.n_nodes));

    // Root: a reasonable random point in trait space and the lineage language.
    cp.lang[0] = phon::make_language(seed);
    {
        Stream rs(seed, 0x5EED00ull);
        eco::SpeciesTraits root;
        root.m       = rs.range(-3.0, 1.0);
        root.tau     = rs.range(0.0, 2.0);
        root.T_opt   = rs.range(0.0, 35.0);
        root.T_tol   = rs.range(3.0, 12.0);
        root.omega   = rs.range(0.3, 1.2);
        root.dp      = rs.range(1.0, 3.0);
        root.kappa   = rs.range(0.5, 1.5);
        root.rho     = rs.range(0.0, 1.0);
        root.phi     = rs.range(0.0, 1.0);
        root.defense = rs.range(0.0, 0.6);
        detail::clamp_traits(root);
        cp.centroid[0] = root;
    }

    const float drift  = 0.05f;  // small per-branch phonetic drift
    const double sigma = 0.6;    // Brownian motion rate per sqrt(branch length)

    // Nodes were created in increasing index order with parent index < child
    // index, so a single forward pass propagates parent -> child correctly.
    for (int i = 1; i < cp.n_nodes; ++i) {
        const int par = cp.parent[static_cast<std::size_t>(i)];
        const std::uint64_t node_seed = mix2(seed, static_cast<std::uint64_t>(i) * 0x100000001B3ull);

        // Language: small drift from the parent (sisters stay phonetically close).
        cp.lang[static_cast<std::size_t>(i)] =
            phon::derive_language(cp.lang[static_cast<std::size_t>(par)], node_seed, drift);

        // Brownian motion on traits: each field += gaussian(0, sigma*sqrt(bl)).
        const double bl = std::max(0.0,
            cp.depth[static_cast<std::size_t>(i)] - cp.depth[static_cast<std::size_t>(par)]);
        const double sd = sigma * std::sqrt(bl);
        Stream bs(node_seed, 0xB17040ull);
        eco::SpeciesTraits c = cp.centroid[static_cast<std::size_t>(par)];
        c.m       += bs.gaussian(0.0, sd);
        c.tau     += bs.gaussian(0.0, sd * 0.5);
        c.T_opt   += bs.gaussian(0.0, sd * 4.0);
        c.T_tol   += bs.gaussian(0.0, sd);
        c.omega   += bs.gaussian(0.0, sd * 0.2);
        c.dp      += bs.gaussian(0.0, sd * 0.3);
        c.kappa   += bs.gaussian(0.0, sd * 0.2);
        c.rho     += bs.gaussian(0.0, sd * 0.15);
        c.phi     += bs.gaussian(0.0, sd * 0.15);
        c.defense += bs.gaussian(0.0, sd * 0.15);
        detail::clamp_traits(c);
        cp.centroid[static_cast<std::size_t>(i)] = c;
    }

    return cp;
}

} // namespace phylo
} // namespace cosmos
