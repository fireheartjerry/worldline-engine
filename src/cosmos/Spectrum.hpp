#pragma once
// Continuous deterministic sampling — the foundation for procedural generation
// that is drawn from spectra, not from discrete banks. Everything here is a pure
// function of a 64-bit seed: same seed -> same value, but values come from
// continuous distributions (uniform, Gaussian, power-law, log-uniform) so the
// space of outcomes is effectively continuous rather than a short menu.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cosmos {

constexpr double kTwoPi = 6.28318530717958647692;

// SplitMix64 step (advances state, returns a well-mixed 64-bit value).
inline std::uint64_t splitmix64(std::uint64_t& s) {
    s += 0x9E3779B97F4A7C15ull;
    std::uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Stateless 64-bit finalizer and a 2-input mix for deriving child seeds/streams.
inline std::uint64_t hash64(std::uint64_t x) {
    x ^= x >> 33; x *= 0xFF51AFD7ED558CCDull;
    x ^= x >> 33; x *= 0xC4CEB9FE1A85EC53ull;
    x ^= x >> 33;
    return x;
}
inline std::uint64_t mix2(std::uint64_t a, std::uint64_t b) {
    return hash64(a ^ (0x9E3779B97F4A7C15ull * (b + 1)));
}

inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double e0, double e1, double x) {
    const double t = clampd((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// A continuous random stream. Construct from a seed (optionally salted to make an
// independent stream from the same seed), then draw continuous samples.
struct Stream {
    std::uint64_t state;

    explicit Stream(std::uint64_t seed) : state(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    Stream(std::uint64_t seed, std::uint64_t salt) : state(mix2(seed, salt) | 1ull) {}

    std::uint64_t u64() { return splitmix64(state); }

    double unit() { return static_cast<double>(u64() >> 11) * (1.0 / 9007199254740992.0); } // [0,1)
    double range(double a, double b) { return a + (b - a) * unit(); }
    double sym() { return unit() * 2.0 - 1.0; } // [-1,1)
    bool   chance(double p) { return unit() < p; }
    double sign() { return (u64() & 1ull) ? 1.0 : -1.0; }

    // Gaussian via Box-Muller (one value per call; the paired value is discarded
    // for statelessness/simplicity).
    double gaussian(double mean = 0.0, double sd = 1.0) {
        const double u1 = std::max(1.0e-12, unit());
        const double u2 = unit();
        return mean + sd * std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
    }

    // Pareto power-law sample x >= xmin with tail exponent alpha (>1); heavy-tailed
    // continuous magnitudes (e.g. masses, populations).
    double power_law(double xmin, double alpha) {
        const double u = std::max(1.0e-12, 1.0 - unit());
        return xmin * std::pow(u, -1.0 / (alpha - 1.0));
    }

    // Continuous across orders of magnitude (uniform in log space).
    double log_uniform(double a, double b) {
        return std::exp(range(std::log(a), std::log(b)));
    }

    // A value in [0,1] biased toward `center` by `tightness` (0 = uniform,
    // 1 = sharply peaked). Useful for continuous trait clustering.
    double beta_like(double center, double tightness) {
        const double g = gaussian(0.0, std::max(0.02, 1.0 - tightness) * 0.5);
        return clampd(center + g, 0.0, 1.0);
    }
};

// Soft, continuous "category" weighting: given a position t in [0,1] over n
// categories and a softness, return the weight of category i — a smooth partition
// of unity so labels emerge from a continuous axis instead of hard buckets.
inline double soft_bin_weight(double t, int i, int n, double softness = 0.18) {
    const double center = (i + 0.5) / n;
    const double d = (t - center) / std::max(1.0e-4, softness);
    return std::exp(-0.5 * d * d);
}

} // namespace cosmos
