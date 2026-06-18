#include "cosmos/ScaleLadder.hpp"

namespace cosmos {

const std::array<ScaleTier, kScaleCount>& scale_ladder() {
    // length_m / time_s are real order-of-magnitude anchors. The four weights
    // (gravity, charge, strong, core) shape the sandbox force law per tier.
    static const std::array<ScaleTier, kScaleCount> kLadder = {{
        {Scale::SUBATOMIC, "Subatomic", "quarks & leptons",
         1.0e-18, 1.0e-24, 0.00, 0.85, 1.00, 0.60},
        {Scale::NUCLEAR, "Nuclear", "protons, neutrons, nuclei",
         1.0e-15, 1.0e-23, 0.00, 0.70, 1.00, 0.70},
        {Scale::ATOMIC, "Atomic", "atoms",
         1.0e-10, 1.0e-16, 0.00, 1.00, 0.45, 0.80},
        {Scale::MOLECULAR, "Molecular", "molecules",
         1.0e-9, 1.0e-13, 0.00, 0.90, 0.65, 0.85},
        {Scale::NANOSCALE, "Nanoscale", "grains & assemblies",
         1.0e-6, 1.0e-6, 0.05, 0.55, 0.30, 0.90},
        {Scale::PLANETARY, "Planetary", "asteroids, moons, planets",
         1.0e7, 1.0e4, 0.85, 0.05, 0.00, 0.95},
        {Scale::STELLAR, "Stellar", "stars & remnants",
         1.0e10, 1.0e7, 1.00, 0.00, 0.00, 0.80},
        {Scale::GALACTIC, "Galactic", "galaxies",
         1.0e21, 1.0e15, 1.00, 0.00, 0.00, 0.60},
        {Scale::COSMIC, "Cosmic", "clusters & large-scale structure",
         1.0e24, 1.0e17, 1.00, 0.00, 0.00, 0.40},
    }};
    return kLadder;
}

const ScaleTier& tier_for(Scale scale) {
    return scale_ladder()[scale_index(scale)];
}

} // namespace cosmos
