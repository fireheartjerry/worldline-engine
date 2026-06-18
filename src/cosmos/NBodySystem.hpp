#pragma once

#include "cosmos/ObjectCatalog.hpp" // Color8, LawGenome, ScaleTier
#include "math/Vec2.hpp"

#include <vector>

namespace cosmos {

// Coefficients of the generalized pairwise force used in the sandbox. All four
// terms are derived from a potential, so total energy is conserved under the
// symplectic velocity-Verlet step (when the acceleration cap is not clipping).
struct ForceParams {
    double gravity = 1.6;     // long-range attraction strength
    double charge = 0.0;      // Coulomb strength (like charges repel)
    double strong = 0.0;      // short-range binding well depth
    double core = 1.2;        // soft-core exclusion strength
    double exponent = 2.0;    // gravity power law (2 = inverse square)
    double softening = 0.18;  // Plummer softening length
    double bond_range = 1.05; // binding well center, in units of summed radii
    double bond_width = 0.60; // binding well width, in units of summed radii
    double core_power = 6.0;  // steepness of the exclusion core
    double accel_cap = 80.0;  // clamp on |acceleration| for stability
    double damping = 0.0;     // velocity damping per unit time (0 = conservative)
};

// Build tier-appropriate force coefficients from a universe's law genome.
ForceParams make_force_params(const ScaleTier& tier, const LawGenome& genome);

struct Body {
    Vec2 pos;
    Vec2 vel;
    double mass = 1.0;
    double charge = 0.0;
    double radius = 1.0;
    int type = 0;       // index into the spawning object set (for coloring/legend)
    Color8 color;
};

// A 2D N-body sandbox. O(N^2) pairwise forces; intended for up to a few hundred
// bodies at interactive rates. Integrated with velocity-Verlet (symplectic).
class NBodySystem {
public:
    std::vector<Body> bodies;
    ForceParams params;

    void step(double dt, int substeps = 4);

    std::vector<Vec2> accelerations() const;

    // Observables for the "analyze" surface.
    double kinetic_energy() const;
    double potential_energy() const;
    double total_energy() const { return kinetic_energy() + potential_energy(); }
    Vec2 total_momentum() const;
    Vec2 center_of_mass() const;
    double virial_ratio() const;   // 2*KE / |PE|; ~1 indicates a relaxed bound system
    int bound_pair_count() const;  // pairs with negative total two-body energy
    double rms_radius() const;     // spread about the center of mass

private:
    void accumulate_pair(int i, int j, std::vector<Vec2>& accel) const;
    double pair_potential(int i, int j) const;
};

} // namespace cosmos
