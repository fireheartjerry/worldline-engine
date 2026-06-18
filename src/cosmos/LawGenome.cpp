#include "cosmos/LawGenome.hpp"

#include <cmath>

namespace cosmos {
namespace {

double frob2(const double m[2][2]) {
    return std::sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] +
                     m[1][0] * m[1][0] + m[1][1] * m[1][1]);
}

double trace2(const double m[2][2]) {
    return m[0][0] + m[1][1];
}

double frob3(const double m[2][2][2]) {
    double sum = 0.0;
    for (int a = 0; a < 2; ++a) {
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                sum += m[a][i][j] * m[a][i][j];
            }
        }
    }
    return std::sqrt(sum);
}

// Squash any real into (lo, hi) with a smooth, monotone, deterministic curve.
double squash(double raw, double lo, double hi) {
    const double t = std::tanh(0.85 * raw); // (-1, 1)
    return lo + (t * 0.5 + 0.5) * (hi - lo);
}

// Quantize to 1e-6 so the genome is bit-identical across platforms despite tiny
// differences in transcendental (tanh/pow) implementations.
double quantize(double v) {
    return std::round(v * 1.0e6) / 1.0e6;
}

std::uint64_t mix64(std::uint64_t h, double value) {
    // Fold a rounded value into an FNV-1a-style accumulator. Rounding keeps the
    // signature stable against last-bit differences in transcendental results.
    const std::int64_t q = static_cast<std::int64_t>(std::llround(value * 1.0e6));
    h ^= static_cast<std::uint64_t>(q);
    h *= 1099511628211ull;
    return h;
}

} // namespace

LawGenome generate_law_genome(const MetaSpec& meta_spec) {
    LawGenome genome;

    const double strong_raw = frob3(meta_spec.C) + 0.6 * meta_spec.s_a - 1.0;
    const double em_raw = frob2(meta_spec.W) + 0.5 * frob2(meta_spec.S) - 1.0;
    const double gravity_raw = frob2(meta_spec.g) - 1.0;
    const double mass_raw = 0.5 * trace2(meta_spec.V);
    const double drift_raw = frob2(meta_spec.T) + meta_spec.drift_amp - 0.5;
    const double stability_raw = meta_spec.s_b - 0.5 * meta_spec.s_c;

    genome.coupling_strong = quantize(squash(strong_raw, 0.35, 1.80));
    genome.coupling_em = quantize(squash(em_raw, 0.35, 1.80));
    genome.coupling_gravity = quantize(squash(gravity_raw, 0.40, 1.70));
    genome.gravity_exponent = quantize(squash(meta_spec.p, 1.75, 2.25));
    genome.mass_scale = quantize(squash(mass_raw, 0.60, 1.60));
    genome.cosmological_drift = quantize(squash(drift_raw, 0.70, 1.50));
    genome.stability_bias = quantize(squash(stability_raw, 0.15, 0.90));

    std::uint64_t sig = 1469598103934665603ull;
    sig = mix64(sig, genome.coupling_strong);
    sig = mix64(sig, genome.coupling_em);
    sig = mix64(sig, genome.coupling_gravity);
    sig = mix64(sig, genome.gravity_exponent);
    sig = mix64(sig, genome.mass_scale);
    sig = mix64(sig, genome.cosmological_drift);
    sig = mix64(sig, genome.stability_bias);
    genome.signature = sig;

    return genome;
}

LawGenome generate_law_genome(const std::string& seed) {
    return generate_law_genome(generate_meta_spec(seed));
}

std::string describe_genome(const LawGenome& genome) {
    std::string out;
    out += (genome.coupling_strong > 1.15)   ? "strong-bound"
         : (genome.coupling_strong < 0.70)   ? "weakly-bound"
                                             : "balanced";
    out += ", ";
    out += (genome.mass_scale > 1.15) ? "heavy"
         : (genome.mass_scale < 0.85) ? "light"
                                      : "median";
    out += ", ";
    const double e = genome.gravity_exponent;
    out += (std::abs(e - 2.0) < 0.06) ? "inverse-square"
         : (e > 2.0)                  ? "steep-gravity"
                                      : "shallow-gravity";
    return out;
}

} // namespace cosmos
