#pragma once
#include "../math/Vec2.hpp"
#include "../seed/UniverseAxioms.hpp"
#include "../seed/DerivedFramework.hpp"
#include <memory>
#include <string>

// ============================================================================
// RenderState — the universal rendering contract.
//
// Every Universe subclass, regardless of its internal physics representation,
// must be able to produce a RenderState.  The Renderer reads only this struct;
// it is entirely unaware of which simulation method is running underneath.
// ============================================================================
struct RenderState {
    Vec2   pivot;             // always {0,0} in world space; screen pivot set by Renderer
    Vec2   bob1;              // world-space position of bob 1
    Vec2   bob2;              // world-space position of bob 2 (tip of the chain)
    double angular_velocity;  // effective omega for trail colour encoding
    double energy;            // total mechanical energy if defined, else 0
    double chaos_estimate;    // local finite-time Lyapunov exponent approximation
    bool   energy_defined;    // false for CELLULAR_AUTOMATON, REACTION_DIFFUSION, etc.
};

// ============================================================================
// Universe — abstract base for all seeded simulation types.
// ============================================================================
class Universe {
public:
    virtual ~Universe() = default;

    // Advance the simulation by dt seconds (or one tick for discrete types).
    virtual void step(double dt) = 0;

    // Produce the current rendering state.
    virtual RenderState extract() const = 0;

    // Human-readable description of this universe's physics.
    virtual std::string describe() const = 0;

    // Factory — selects and constructs the correct subclass from axioms + framework.
    // Include UniverseFactory.hpp to get the definition of this function.
    // Do NOT call this from any Universe subclass header.
    static std::unique_ptr<Universe> create(const Worldline::UniverseAxioms& ax,
                                             const Worldline::DerivedFramework& fw);
};
