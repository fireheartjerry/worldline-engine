#pragma once
// Cosmography — deterministic large-scale-structure (cosmic web) layout for the
// COSMIC tier. Places the structures of the cosmic web — clusters at the nodes,
// filaments bridging them, sheet-like walls, and the vast underdense voids that
// fill most of the volume — so a Universe node can lay out its galaxies and
// clusters on a realistic web instead of a uniform random scatter.
//
// The matter density field is approximated by summing a handful of random
// Fourier-like modes (sines with seed-derived wavevectors and phases). The raw
// Gaussian-ish field is passed through a lognormal transform, the standard
// closed-form model for the nonlinear matter density (Coles & Jones 1991): it
// is bounded below by delta = -1, has a long high-density tail, and — crucially —
// puts most of the *volume* at modest underdensity, reproducing the observed
// void-dominated universe. Points are then biased toward overdense regions so
// they pile onto filaments and nodes and leave the voids empty.
//
// Everything is a pure function of a 64-bit seed: same seed -> identical layout.
//
// Sources (deep-research appendix):
//   - Bond, Kofman & Pogosyan (1996) "How filaments of galaxies are woven into
//     the cosmic web." Nature 380, 603 — node/filament/wall/void taxonomy.
//   - Coles & Jones (1991) lognormal density field model. MNRAS 248, 1.
//   - Eisenstein et al. (2005) BAO standard ruler (~147 Mpc). ApJ 633, 560.

#include "cosmos/Spectrum.hpp"
#include "cosmos/CosmoStats.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cosmos {
namespace cosmoweb {

// Morphological class of a point in the cosmic web, ordered by increasing
// density: voids fill most of the volume, walls (sheets) bound them, filaments
// thread between, and nodes (galaxy clusters) sit where filaments intersect.
enum class WebClass { Void, Wall, Filament, Node };

// A sampled position in the box with its local density contrast and class.
struct WebPoint {
    float x, y, z;            // position in Mpc, within [0, box_mpc]
    WebClass cls;             // morphological class
    double density_contrast;  // delta = rho/rho_bar - 1  (>= -1)
};

// Classify a point by its overdensity delta = rho/rho_bar - 1. Thresholds are
// the conventional cosmic-web bands used to separate the four morphologies:
//   delta <  -0.6        -> Void      (deeply underdense)
//   -0.6 <= delta <  0.5 -> Wall      (near-mean sheets)
//    0.5 <= delta <  5.0 -> Filament  (mildly nonlinear bridges)
//   delta >= 5.0         -> Node      (collapsed cluster overdensities)
inline WebClass classify_density(double delta) {
    if (delta < -0.6) return WebClass::Void;
    if (delta < 0.5)  return WebClass::Wall;
    if (delta < 5.0)  return WebClass::Filament;
    return WebClass::Node;
}

namespace detail {

// Number of Fourier-like modes summed to build the smooth density field. A few
// modes give a richly structured but inexpensive field.
constexpr int kNumModes = 12;

// rms amplitude of the underlying Gaussian field before the lognormal map. Tuned
// so the resulting void volume fraction lands in the observed ~0.6..0.85 range.
constexpr double kFieldSigma = 1.85;

struct Mode {
    double kx, ky, kz;  // wavevector components (cycles per box length, scaled)
    double phase;       // phase offset
    double amp;         // per-mode amplitude weight
};

// Build the deterministic mode set for a seed. Wavevectors are integer-ish
// (a few wavelengths across the box) so the field is smooth and periodic-like;
// amplitudes are weighted toward larger scales (smaller |k|), as in a red P(k).
inline std::array<Mode, kNumModes> build_modes(std::uint64_t seed) {
    std::array<Mode, kNumModes> modes{};
    Stream rng(seed, 0xC05305A1Cull); // "cosmological" salt
    double amp_sum_sq = 0.0;
    for (int i = 0; i < kNumModes; ++i) {
        // 1..4 wavelengths across the box on each axis.
        const double nx = std::floor(rng.range(1.0, 4.999));
        const double ny = std::floor(rng.range(1.0, 4.999));
        const double nz = std::floor(rng.range(1.0, 4.999));
        const double kmag = std::sqrt(nx * nx + ny * ny + nz * nz);
        const double amp = 1.0 / kmag; // red tilt: more power on large scales
        modes[static_cast<std::size_t>(i)] =
            Mode{nx, ny, nz, rng.range(0.0, kTwoPi), amp};
        amp_sum_sq += amp * amp;
    }
    // Normalise so the field has unit variance before scaling by kFieldSigma.
    // Each independent sine mode contributes variance amp^2/2.
    const double var = 0.5 * amp_sum_sq;
    const double norm = (var > 0.0) ? (kFieldSigma / std::sqrt(var)) : 1.0;
    for (auto& m : modes) m.amp *= norm;
    return modes;
}

// Evaluate the Gaussian-ish field g at a normalised position (u,v,w) in [0,1)^3.
inline double field_gauss(const std::array<Mode, kNumModes>& modes,
                          double u, double v, double w) {
    double g = 0.0;
    for (const auto& m : modes) {
        const double arg = kTwoPi * (m.kx * u + m.ky * v + m.kz * w) + m.phase;
        g += m.amp * std::sin(arg);
    }
    return g;
}

// Lognormal map from the Gaussian field to a physical density contrast:
//   1 + delta = exp(g - sigma^2/2),  so <1+delta> = 1 (mean density preserved),
//   delta >= -1, with a long high-density tail. (Coles & Jones 1991.)
inline double lognormal_delta(double g) {
    const double s2 = kFieldSigma * kFieldSigma;
    return std::exp(g - 0.5 * s2) - 1.0;
}

} // namespace detail

// Generate n_points inside a cube of side box_mpc, laid out on a deterministic
// cosmic web. Each point is drawn at a candidate position, evaluated against the
// density field, and nudged toward the local high-density direction so points
// accumulate on filaments and nodes and thin out in voids. Returns the points
// with their density contrast and morphological class assigned.
inline std::vector<WebPoint> sample_cosmic_web(std::uint64_t seed, int n_points,
                                              double box_mpc) {
    std::vector<WebPoint> pts;
    if (n_points <= 0 || box_mpc <= 0.0) return pts;
    pts.reserve(static_cast<std::size_t>(n_points));

    const auto modes = detail::build_modes(seed);
    Stream rng(seed, 0x57AB11ABull); // "structure" salt

    // Finite-difference step (in normalised units) for the density gradient.
    const double h = 1.0e-3;

    for (int i = 0; i < n_points; ++i) {
        // Candidate position in normalised [0,1)^3.
        double u = rng.unit();
        double v = rng.unit();
        double w = rng.unit();

        // Bias toward higher density: take a couple of small steps along the
        // (wrapped) gradient of the field. This pulls points up density ridges
        // onto filaments/nodes while leaving the deep voids underpopulated.
        for (int step = 0; step < 3; ++step) {
            const double g0 = detail::field_gauss(modes, u, v, w);
            const double gx = detail::field_gauss(modes, u + h, v, w) - g0;
            const double gy = detail::field_gauss(modes, u, v + h, w) - g0;
            const double gz = detail::field_gauss(modes, u, v, w + h) - g0;
            const double rate = 0.06; // gentle bias keeps voids from emptying fully
            u += rate * gx / h * (1.0 / detail::kNumModes);
            v += rate * gy / h * (1.0 / detail::kNumModes);
            w += rate * gz / h * (1.0 / detail::kNumModes);
            // Wrap into [0,1) so the field stays smooth and bounded.
            u -= std::floor(u);
            v -= std::floor(v);
            w -= std::floor(w);
        }

        const double g = detail::field_gauss(modes, u, v, w);
        const double delta = detail::lognormal_delta(g);

        WebPoint p;
        p.x = static_cast<float>(clampd(u, 0.0, 1.0) * box_mpc);
        p.y = static_cast<float>(clampd(v, 0.0, 1.0) * box_mpc);
        p.z = static_cast<float>(clampd(w, 0.0, 1.0) * box_mpc);
        p.cls = classify_density(delta);
        p.density_contrast = delta;
        pts.push_back(p);
    }
    return pts;
}

// Fraction of points classified as Void. For a realistic field this lands in the
// cosmic-web range (~0.6..0.85): voids occupy most of the volume of the universe.
inline double void_volume_fraction(const std::vector<WebPoint>& pts) {
    if (pts.empty()) return 0.0;
    std::size_t voids = 0;
    for (const auto& p : pts)
        if (p.cls == WebClass::Void) ++voids;
    return static_cast<double>(voids) / static_cast<double>(pts.size());
}

// The baryon-acoustic-oscillation scale (~147 Mpc), the standard ruler imprinted
// on the matter distribution and the characteristic clustering scale of the web.
inline double bao_scale_mpc() {
    return cstat::kRdragMpc;
}

// Sample a cluster mass [Msun] for a point. Clusters — the largest gravitationally
// bound structures — live at the nodes; only Node-class overdensities host them.
// Nodes draw a log-uniform mass in ~1e14..1e15 Msun, scaled up with the overdensity
// delta (richer nodes are more massive). Non-node points return a much smaller
// (group/galaxy-scale) mass.
inline double cluster_mass_msun(double delta, Stream& rng) {
    if (delta < 5.0) {
        // Group/galaxy-scale halo for non-node points.
        return rng.log_uniform(1.0e11, 1.0e13);
    }
    const double base = rng.log_uniform(1.0e14, 1.0e15);
    // Monotone, bounded boost with overdensity (delta=5 -> ~1x, growing slowly).
    const double boost = 1.0 + 0.15 * (delta - 5.0);
    return base * boost;
}

// Two-point correlation excess at separation r [Mpc]: the excess probability of
// finding a pair of structures at separation r over a random distribution.
// xi(5 Mpc) = 1 by the power-law normalisation in CosmoStats.
inline double two_point_excess(double r_mpc) {
    return cstat::correlation_xi(r_mpc);
}

} // namespace cosmoweb
} // namespace cosmos
