#pragma once

#include "seed/MetaSpec.hpp"

#include <cstdint>
#include <string>

namespace cosmos {

// The "law genome" of a universe: a small set of dimensionless fundamental
// constants derived deterministically from a seed's MetaSpec. 1.0 means
// "like our universe"; values drift above/below to make each seed a distinct
// physics. These drive object properties (LawGenome -> ObjectCatalog) and the
// sandbox force law (LawGenome + ScaleTier -> NBodySystem ForceParams).
struct LawGenome {
    double coupling_strong = 1.0;   // nuclear binding strength
    double coupling_em = 1.0;       // electromagnetic strength
    double coupling_gravity = 1.0;  // gravitational strength
    double gravity_exponent = 2.0;  // inverse-power law exponent (~2 = inverse square)
    double mass_scale = 1.0;        // overall matter mass multiplier
    double cosmological_drift = 1.0;// large-scale expansion tendency
    double stability_bias = 0.5;    // how readily structures bind / persist, [0,1]
    std::uint64_t signature = 0;    // stable hash for display + reproducible spawning
};

// Derive the genome from an already-generated MetaSpec. Deterministic and
// cross-platform (uses the same bounded IEEE-754 math as the rest of the seed
// pipeline).
LawGenome generate_law_genome(const MetaSpec& meta_spec);

// Convenience: seed string -> MetaSpec -> genome.
LawGenome generate_law_genome(const std::string& seed);

// One-line human description, e.g. "strong-bound, heavy, inverse-square".
std::string describe_genome(const LawGenome& genome);

} // namespace cosmos
