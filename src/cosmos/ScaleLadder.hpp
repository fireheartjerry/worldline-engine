#pragma once

#include <array>
#include <cstddef>

namespace cosmos {

// Ordered tiers of the universe, climbing orders of magnitude. Each tier is its
// own interaction sandbox; together they form the navigable "ladder". The whole
// range cannot be one continuous simulation, so the ladder is the bridge.
enum class Scale {
    SUBATOMIC,   // quarks, leptons
    NUCLEAR,     // protons, neutrons, nuclei
    ATOMIC,      // atoms
    MOLECULAR,   // molecules
    NANOSCALE,   // grains, viruses, macromolecular assemblies
    PLANETARY,   // asteroids, moons, planets
    STELLAR,     // stars, stellar remnants
    GALACTIC,    // galaxies
    COSMIC,      // clusters, large-scale structure
    COUNT
};

struct ScaleTier {
    Scale scale;
    const char* name;
    const char* subtitle;
    double length_m; // characteristic length scale (meters)
    double time_s;   // characteristic dynamical timescale (seconds)

    // How the genome's couplings combine into the sandbox force law at this
    // tier. Large scales are gravity-dominated; small scales are charge- and
    // binding-dominated; the soft core is always present to prevent collapse.
    double gravity_weight;
    double charge_weight;
    double strong_weight;
    double core_weight;
};

constexpr std::size_t kScaleCount = static_cast<std::size_t>(Scale::COUNT);

// The fixed ladder. Returned by reference; data is static and immutable.
const std::array<ScaleTier, kScaleCount>& scale_ladder();

const ScaleTier& tier_for(Scale scale);

inline std::size_t scale_index(Scale scale) {
    return static_cast<std::size_t>(scale);
}

} // namespace cosmos
