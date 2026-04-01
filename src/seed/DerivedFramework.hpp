#pragma once
#include "UniverseAxioms.hpp"
#include <string>

// ============================================================================
// DerivedFramework — deterministic mapping from UniverseAxioms to the
// simulation method and metadata that the physics layer needs.
//
// This file contains both the struct definition and the full derivation logic
// in derive_framework().  You should not need to modify it; extend it only
// when adding a new Universe subclass.
// ============================================================================

namespace Worldline {

struct DerivedFramework {

    // -----------------------------------------------------------------------
    // SimMethod — which integration / update strategy the Universe subclass
    // will use.  Universe::create() switches on this value.
    //
    // Priority cascade (highest priority first) used by derive_framework:
    //
    //  1. CELLULAR_AUTOMATON   — dynamics == CELLULAR_AUTOMATON
    //  2. DISCRETE_MAP         — dynamics == DISCRETE_MAP  OR  time == DISCRETE
    //  3. REACTION_DIFFUSION   — dynamics == REACTION_DIFFUSION
    //  4. MEMORY_INTEGRO       — memory_depth > 0 AND dynamics == LEAST_ACTION
    //  5. STOCHASTIC_LANGEVIN  — dynamics == STOCHASTIC_DRIFT  OR
    //                            noise_amplitude > 0.5
    //  6. HYPERBOLIC_GEODESIC  — space == HYPERBOLIC  OR  space == ANTI_DE_SITTER
    //  7. SPHERICAL_GEODESIC   — space == SPHERICAL
    //  8. HAMILTONIAN_RK4      — dynamics == LEAST_ACTION AND
    //                            conservation.energy == true AND
    //                            space == EUCLIDEAN OR TORUS
    //  9. LAGRANGIAN_RK4       — dynamics == LEAST_ACTION (energy may drift)
    // 10. NEWTONIAN_DIRECT     — everything else; force law, no action principle
    // -----------------------------------------------------------------------
    enum class SimMethod {
        HAMILTONIAN_RK4,     // energy-conserving; symplectic; standard Lagrangian
        LAGRANGIAN_RK4,      // Lagrangian valid; energy may drift (dissipation > 0)
        NEWTONIAN_DIRECT,    // F=ma applied directly; no variational principle
        HYPERBOLIC_GEODESIC, // RK4 on H² with the Poincaré-disk metric tensor
        SPHERICAL_GEODESIC,  // RK4 on S² with the round metric tensor
        DISCRETE_MAP,        // iterated algebraic map; no ODE
        STOCHASTIC_LANGEVIN, // Lagrangian equations + Wiener noise (Itô)
        REACTION_DIFFUSION,  // coupled PDEs; bobs are tracked concentration peaks
        MEMORY_INTEGRO,      // integro-differential; requires circular history buffer
        CELLULAR_AUTOMATON,  // discrete grid bitfield; evolution by rule table
    } method = SimMethod::HAMILTONIAN_RK4;

    // Meta-flags derived alongside method — used by rendering and UI.
    bool lagrangian_valid  = true;   // a Lagrangian exists for this universe
    bool hamiltonian_valid = true;   // a Hamiltonian exists (implies symplectic)
    bool energy_conserved  = true;   // H is a constant of motion
    bool time_is_continuous = true;  // dt is a real number, not an integer tick
    bool space_is_curved    = false; // non-Euclidean metric; geodesic arms
    int  state_dimensions   = 4;     // number of generalised coordinates
                                     // (standard: theta1, theta2, omega1, omega2)

    // Human-readable description shown on screen.
    // Format: "<Space> · <Dynamics> · <Interaction>  [<extras>]"
    // Example: "Hyperbolic space · Lagrangian · Berry phase joint  [memory 2.3 s]"
    std::string description;
};

// ---------------------------------------------------------------------------
// Helper strings (keep in sync with the enums in UniverseAxioms.hpp)
// ---------------------------------------------------------------------------
namespace detail {

inline const char* space_name(Space s) {
    switch (s) {
        case Space::EUCLIDEAN:          return "Euclidean";
        case Space::HYPERBOLIC:         return "Hyperbolic";
        case Space::SPHERICAL:          return "Spherical";
        case Space::TORUS:              return "Torus";
        case Space::ANTI_DE_SITTER:     return "Anti-de Sitter";
        case Space::CONICAL:            return "Conical";
        case Space::RIEMANNIAN_DYNAMIC: return "Dynamic Riemannian";
        case Space::FRACTAL:            return "Fractal";
        case Space::WORMHOLE:           return "Wormhole";
        case Space::GRAPH:              return "Graph";
    }
    return "Unknown";
}

inline const char* dynamics_name(Dynamics d) {
    switch (d) {
        case Dynamics::LEAST_ACTION:          return "Lagrangian";
        case Dynamics::MAXIMUM_ENTROPY_PROD:  return "Max-entropy";
        case Dynamics::MINIMUM_ENERGY:        return "Min-energy";
        case Dynamics::GEODESIC_FLOW:         return "Geodesic flow";
        case Dynamics::MEAN_CURVATURE_FLOW:   return "Mean-curvature flow";
        case Dynamics::RICCI_FLOW:            return "Ricci flow";
        case Dynamics::DISCRETE_MAP:          return "Discrete map";
        case Dynamics::REACTION_DIFFUSION:    return "Reaction-diffusion";
        case Dynamics::CELLULAR_AUTOMATON:    return "Cellular automaton";
        case Dynamics::RETROCAUSAL_BOUNDARY:  return "Retrocausal boundary";
        case Dynamics::STOCHASTIC_DRIFT:      return "Stochastic (Langevin)";
    }
    return "Unknown";
}

inline const char* interaction_name(Interaction i) {
    switch (i) {
        case Interaction::RIGID_JOINT:      return "rigid joint";
        case Interaction::TORSIONAL_SPRING: return "torsional spring";
        case Interaction::GAUGE_FIELD:      return "U(1) gauge";
        case Interaction::BERRY_PHASE:      return "Berry phase";
        case Interaction::ENTROPIC_FORCE:   return "entropic force";
        case Interaction::CHERN_SIMONS:     return "Chern-Simons";
        case Interaction::MAGNETIC_COUPLING:return "magnetic coupling";
        case Interaction::NONLOCAL:         return "non-local";
        case Interaction::TORSION:          return "torsion";
        case Interaction::DELAYED:          return "delayed";
    }
    return "unknown";
}

} // namespace detail

// ---------------------------------------------------------------------------
// derive_framework — the priority cascade described in the SimMethod comment.
// ---------------------------------------------------------------------------
inline DerivedFramework derive_framework(const UniverseAxioms& ax) {
    DerivedFramework fw;

    // ---- Method selection (priority cascade) ----

    if (ax.dynamics == Dynamics::CELLULAR_AUTOMATON) {
        fw.method              = DerivedFramework::SimMethod::CELLULAR_AUTOMATON;
        fw.lagrangian_valid    = false;
        fw.hamiltonian_valid   = false;
        fw.energy_conserved    = false;
        fw.time_is_continuous  = false;
        fw.state_dimensions    = 2;  // two discrete angle indices

    } else if (ax.dynamics == Dynamics::DISCRETE_MAP ||
               ax.time == Time::DISCRETE) {
        fw.method              = DerivedFramework::SimMethod::DISCRETE_MAP;
        fw.lagrangian_valid    = false;
        fw.hamiltonian_valid   = false;
        fw.energy_conserved    = ax.conservation.energy;
        fw.time_is_continuous  = false;
        fw.state_dimensions    = 4;

    } else if (ax.dynamics == Dynamics::REACTION_DIFFUSION) {
        fw.method              = DerivedFramework::SimMethod::REACTION_DIFFUSION;
        fw.lagrangian_valid    = false;
        fw.hamiltonian_valid   = false;
        fw.energy_conserved    = false;
        fw.state_dimensions    = 4;  // two concentration fields → two peak coords

    } else if (ax.memory_depth > 0.0 && ax.dynamics == Dynamics::LEAST_ACTION) {
        fw.method              = DerivedFramework::SimMethod::MEMORY_INTEGRO;
        fw.lagrangian_valid    = true;
        fw.hamiltonian_valid   = false;  // history-dependent ≠ symplectic
        fw.energy_conserved    = false;
        fw.state_dimensions    = 4;

    } else if (ax.dynamics == Dynamics::STOCHASTIC_DRIFT ||
               ax.noise_amplitude > 0.5) {
        fw.method              = DerivedFramework::SimMethod::STOCHASTIC_LANGEVIN;
        fw.lagrangian_valid    = true;
        fw.hamiltonian_valid   = false;
        fw.energy_conserved    = false;
        fw.state_dimensions    = 4;

    } else if (ax.space == Space::HYPERBOLIC ||
               ax.space == Space::ANTI_DE_SITTER) {
        fw.method              = DerivedFramework::SimMethod::HYPERBOLIC_GEODESIC;
        fw.lagrangian_valid    = true;
        fw.hamiltonian_valid   = true;
        fw.energy_conserved    = ax.conservation.energy;
        fw.space_is_curved     = true;
        fw.state_dimensions    = 4;

    } else if (ax.space == Space::SPHERICAL) {
        fw.method              = DerivedFramework::SimMethod::SPHERICAL_GEODESIC;
        fw.lagrangian_valid    = true;
        fw.hamiltonian_valid   = true;
        fw.energy_conserved    = ax.conservation.energy;
        fw.space_is_curved     = true;
        fw.state_dimensions    = 4;

    } else if (ax.dynamics == Dynamics::LEAST_ACTION &&
               ax.conservation.energy &&
               (ax.space == Space::EUCLIDEAN || ax.space == Space::TORUS) &&
               ax.dissipation == 0.0) {
        fw.method              = DerivedFramework::SimMethod::HAMILTONIAN_RK4;
        fw.lagrangian_valid    = true;
        fw.hamiltonian_valid   = true;
        fw.energy_conserved    = true;
        fw.state_dimensions    = 4;

    } else if (ax.dynamics == Dynamics::LEAST_ACTION) {
        fw.method              = DerivedFramework::SimMethod::LAGRANGIAN_RK4;
        fw.lagrangian_valid    = true;
        fw.hamiltonian_valid   = ax.dissipation == 0.0;
        fw.energy_conserved    = ax.dissipation == 0.0 && ax.conservation.energy;
        fw.state_dimensions    = 4;

    } else {
        // Fallback: direct Newtonian force law
        fw.method              = DerivedFramework::SimMethod::NEWTONIAN_DIRECT;
        fw.lagrangian_valid    = false;
        fw.hamiltonian_valid   = false;
        fw.energy_conserved    = ax.conservation.energy;
        fw.state_dimensions    = 4;
    }

    fw.time_is_continuous = (ax.time != Time::DISCRETE &&
                              ax.time != Time::IMAGINARY);
    fw.space_is_curved   |= (ax.space_curvature != 0.0);

    // ---- Human-readable description ----
    fw.description  = detail::space_name(ax.space);
    fw.description += " \xC2\xB7 ";  // UTF-8 middle dot
    fw.description += detail::dynamics_name(ax.dynamics);
    fw.description += " \xC2\xB7 ";
    fw.description += detail::interaction_name(ax.interaction);

    if (ax.memory_depth > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "  [memory %.1f s]", ax.memory_depth);
        fw.description += buf;
    }
    if (ax.noise_amplitude > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "  [noise %.2f]", ax.noise_amplitude);
        fw.description += buf;
    }

    return fw;
}

} // namespace Worldline
