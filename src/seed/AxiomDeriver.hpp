#pragma once
#include "UniverseAxioms.hpp"
#include "../physics/PendulumParams.hpp"

// ============================================================================
// AxiomDeriver — maps a UniverseAxioms bundle to concrete PendulumParams.
//
// This translation layer sits between the abstract seed system and the physics
// layer.  It is the only place that knows both representations.
//
// All output values are clamped to ranges the physics code can handle safely.
// ============================================================================

namespace Worldline {

// Concrete parameters for the StandardPendulum and Lagrangian-family universes.
// Extended parameter sets (for hyperbolic, stochastic, etc.) live in their own
// structs adjacent to each Universe subclass.
struct DerivedPhysicsParams {
    PendulumParams pendulum;

    // Stochastic extension
    double noise_amplitude = 0.0;  // std-dev of per-step Wiener increment (rad/s)

    // Memory extension
    double memory_depth    = 0.0;  // seconds of history the integro-DE looks back
    double memory_decay    = 0.5;  // exponential kernel decay rate (1/s)

    // Curvature extension
    double curvature       = 0.0;  // signed Gaussian curvature of the ambient space
};

// ---------------------------------------------------------------------------
// derive_physics_params
//
// Maps axiom fields to concrete numeric parameters.
//
// Mapping strategy:
//   gravity_exponent  → pendulum.gravity_exponent  (direct copy; already in range)
//   space_curvature   → curvature                  (direct copy)
//   noise_amplitude   → noise_amplitude             (direct copy)
//   memory_depth      → memory_depth               (direct copy)
//   dissipation       → a damping coefficient baked into the Lagrangian
//
//   Characteristic length and mass scales are held fixed (l1=l2=1, m1=m2=1)
//   for Phase 3.  Phase 4 can derive them from BobOntology.
// ---------------------------------------------------------------------------
inline DerivedPhysicsParams derive_physics_params(const UniverseAxioms& ax) {
    DerivedPhysicsParams dp;

    // Base pendulum parameters — lengths and masses fixed for Phase 3.
    dp.pendulum.l1 = 1.0;
    dp.pendulum.l2 = 1.0;
    dp.pendulum.m1 = 1.0;
    dp.pendulum.m2 = 1.0;

    // Gravity — scale g by the exponent so that at exponent=2 the feel is the
    // same as standard g=9.81.  Universes with higher exponent have stronger
    // effective gravity at arm-length scales.
    dp.pendulum.g               = 9.81;
    dp.pendulum.gravity_exponent = ax.gravity_exponent;

    // Extensions
    dp.noise_amplitude  = ax.noise_amplitude;
    dp.memory_depth     = ax.memory_depth;
    dp.memory_decay     = 1.0 / (ax.memory_depth > 0.0 ? ax.memory_depth : 1.0);
    dp.curvature        = ax.space_curvature;

    // Dissipation is added as a linear drag on angular velocities.
    // The physics layer multiplies omega by (1 - dissipation * dt) each step.
    // PendulumParams does not carry this yet; extend it when implementing
    // LAGRANGIAN_RK4 with dissipation > 0.
    // dp.pendulum.dissipation = ax.dissipation;  // placeholder for Phase 4

    return dp;
}

} // namespace Worldline
