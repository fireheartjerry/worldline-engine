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
    double confinement = 0.0; // harmonic pull toward the origin (keeps the cloud
                              // on-stage); a single-body force, so it is excluded
                              // from the conservative energy/momentum tests.
    double linear = 0.0;      // pairwise attraction growing with distance
                              // (quark-style confinement). Momentum/energy safe.
    double swirl = 0.0;       // rotational forcing a += swirl * perp(pos); drives
                              // galactic disks. Single-body, non-conservative.
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
//
// step() caches the per-pair force constants and the accelerations at the end
// of each step (forces depend only on positions + body/force parameters, never
// on velocity), so the next substep/step reuses them instead of recomputing.
// The cache is validated bit-for-bit against `bodies` and `params` on every
// step(), so mutating either between steps is always safe; results are
// bit-identical to evaluating the forces from scratch every time.
class NBodySystem {
public:
    std::vector<Body> bodies;
    ForceParams params;

    void step(double dt, int substeps = 4);

    std::vector<Vec2> accelerations() const; // fresh evaluation (no cache)

    // Observables for the "analyze" surface.
    double kinetic_energy() const;
    double potential_energy() const;
    double total_energy() const { return kinetic_energy() + potential_energy(); }
    Vec2 total_momentum() const;
    Vec2 center_of_mass() const;
    double angular_momentum() const; // total L about the origin (rotation signature)
    double virial_ratio() const;     // 2*KE / |PE|; for an inverse-square bound
                                     // system in equilibrium this tends to ~1
                                     // (the sandbox mixes several potentials, so
                                     // read it as a relative relaxation indicator)
    int bound_pair_count() const;    // pairs with negative total two-body energy
    double rms_radius() const;       // spread about the center of mass
    double max_radius() const;       // farthest body from the center of mass

private:
    // Per-pair constants of the force law: everything in the pair force that
    // does not depend on the separation (pow(sigma, core_power) is the costly
    // one). Stored for i < j in row-major upper-triangle order.
    struct PairTerms {
        double gravity = 0.0;  // G * m_i * m_j
        double coulomb = 0.0;  // -k * q_i * q_j
        double core = 0.0;     // core * core_power * sigma^core_power
        double bond_r0 = 0.0;  // binding well center
        double bond_w2 = 0.0;  // w^2
        double bond_2w2 = 0.0; // 2 w^2
    };

    // Inputs a cached evaluation was made from (compared bitwise).
    struct BodyKey {
        double x, y, mass, charge, radius;
    };

    double pair_potential(int i, int j) const;
    void build_pair_terms(std::vector<PairTerms>& terms) const;
    void evaluate_accelerations(const std::vector<PairTerms>& terms, std::vector<Vec2>& accel) const;
    bool cache_inputs_match(bool& positions_match) const;
    void remember_cache_inputs();

    std::vector<PairTerms> pair_terms_;  // valid while cache_valid_
    std::vector<Vec2> accel_;            // accelerations at cache_key_ positions
    std::vector<BodyKey> cache_key_;
    ForceParams cache_params_{};
    bool cache_valid_ = false;
};

} // namespace cosmos
