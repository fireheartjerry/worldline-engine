#pragma once
// ============================================================================
// UniverseFactory.hpp — defines Universe::create().
//
// Include this header (not Universe.hpp directly) from any translation unit
// that calls Universe::create().  This header pulls in all concrete subclasses
// to avoid circular include chains between Universe.hpp and the subclasses.
// ============================================================================
#include "Universe.hpp"
#include "Simulation.hpp"           // StandardPendulumUniverse
#include "HyperbolicPendulum.hpp"
#include "DiscretePendulum.hpp"
#include "StochasticPendulum.hpp"
#include "MemoryPendulum.hpp"
#include <stdexcept>

// ============================================================================
// StandardPendulumUniverse adapter — wraps Simulation as a Universe subclass.
// ============================================================================
class StandardPendulumUniverseImpl : public Universe {
public:
    explicit StandardPendulumUniverseImpl(const Worldline::UniverseAxioms& ax)
        : inner(ax) {}

    explicit StandardPendulumUniverseImpl()
        : inner(default_axioms()) {}

    void step(double dt) override { inner.step(dt); }

    RenderState extract() const override {
        return RenderState{
            {0.0, 0.0},
            inner.bob1(),
            inner.bob2(),
            inner.angular_velocity(),
            inner.energy(),
            0.0,
            true
        };
    }

    std::string describe() const override { return inner.describe(); }

private:
    StandardPendulumUniverse inner;

    static Worldline::UniverseAxioms default_axioms() {
        Worldline::UniverseAxioms ax;
        // All defaults — Euclidean, Lagrangian, standard gravity
        return ax;
    }
};

// ============================================================================
// Universe::create — full implementation
// ============================================================================
inline std::unique_ptr<Universe>
Universe::create(const Worldline::UniverseAxioms& ax,
                 const Worldline::DerivedFramework& fw) {
    using SM = Worldline::DerivedFramework::SimMethod;
    switch (fw.method) {
        case SM::HAMILTONIAN_RK4:
        case SM::LAGRANGIAN_RK4:
        case SM::NEWTONIAN_DIRECT:
            return std::make_unique<StandardPendulumUniverseImpl>(ax);

        case SM::HYPERBOLIC_GEODESIC:
            return std::make_unique<HyperbolicPendulum>(ax);

        case SM::DISCRETE_MAP:
            return std::make_unique<DiscretePendulum>(ax);

        case SM::STOCHASTIC_LANGEVIN:
            return std::make_unique<StochasticPendulum>(ax);

        case SM::MEMORY_INTEGRO:
            return std::make_unique<MemoryPendulum>(ax);

        // Not yet implemented — fall back gracefully.
        case SM::SPHERICAL_GEODESIC:
        case SM::REACTION_DIFFUSION:
        case SM::CELLULAR_AUTOMATON:
            return std::make_unique<StandardPendulumUniverseImpl>(ax);
    }
    return std::make_unique<StandardPendulumUniverseImpl>(ax);
}

// Convenience factory for the debug mode — bypasses seed system entirely.
inline std::unique_ptr<Universe> make_debug_universe() {
    return std::make_unique<StandardPendulumUniverseImpl>();
}
