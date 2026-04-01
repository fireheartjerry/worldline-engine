#pragma once
#include <cstdint>

// ============================================================================
// UniverseAxioms — the complete ontological description of a seeded universe.
//
// Every field is populated by derive_axioms() in Seed.hpp.  The valid range
// of each continuous parameter is documented inline so you can write your own
// deriver knowing exactly what values downstream code expects.
// ============================================================================

namespace Worldline {

// ---------------------------------------------------------------------------
// Enumerations — each covers every variant the simulation system supports.
// ---------------------------------------------------------------------------

// The geometric space in which the pendulum lives.
enum class Space : uint8_t {
    EUCLIDEAN,           // flat R²  — standard pendulum
    HYPERBOLIC,          // H²       — Poincaré disk; arms follow geodesics
    SPHERICAL,           // S²       — arms follow great-circle geodesics
    TORUS,               // T²       — flat torus with periodic boundaries
    ANTI_DE_SITTER,      // AdS₂     — constant negative curvature, time-like boundary
    CONICAL,             // cone with deficit angle derived from space_curvature
    RIEMANNIAN_DYNAMIC,  // curvature tensor evolves via Ricci flow each step
    FRACTAL,             // dimension between 1 and 2; motion on Sierpinski-like attractor
    WORMHOLE,            // two Euclidean sheets connected by a throat at a seeded radius
    GRAPH,               // discrete graph; bobs occupy vertices, arms are edges
};

// The nature of the time coordinate.
enum class Time : uint8_t {
    CONTINUOUS,          // standard smooth dt — most universes
    DISCRETE,            // integer tick; only valid with DISCRETE_MAP dynamics
    CYCLIC,              // time wraps at period T = time_scale
    MULTI_FINGERED,      // each bob has its own local proper time (post-Newtonian toy)
    EMERGENT_ENTROPY,    // dt proportional to entropy production; high-chaos phases rush
    RETROCAUSAL,         // boundary conditions imposed at t_final; step backwards
    IMAGINARY,           // Wick-rotated; equations become elliptic rather than hyperbolic
    TWO_TIMES,           // two independent time axes; motion is a worldsheet
};

// What the bobs fundamentally are.
enum class BobOntology : uint8_t {
    POINT_MASS,          // standard — dimensionless mass
    EXTENDED_RIGID,      // finite radius; contact interactions between bobs
    EXTENDED_ELASTIC,    // soft body; shape deforms under centripetal acceleration
    VORTEX,              // quantised vortex in a 2-D superfluid; carries circulation
    SOLITON,             // topological soliton; self-reinforcing wave packet
    MAGNETIC_MONOPOLE,   // Dirac monopole; arm carries Berry phase around it
    SPINOR,              // two-component spinor; rotations accumulate global phase
    CHARGE_DISTRIBUTION, // blob of charge in a seeded electric field
};

// What the arms (rods / strings) fundamentally are.
enum class ArmOntology : uint8_t {
    RIGID_ROD,           // fixed length, holonomic constraint
    ELASTIC_SPRING,      // Hookean; rest length and stiffness from axioms
    INEXTENSIBLE_STRING, // flexible; goes slack when tension is negative
    GEODESIC,            // shortest path in the ambient geometry
    VORTEX_FILAMENT,     // induces a Biot-Savart velocity field along its length
    GAUGE_CONNECTION,    // arm is a U(1) gauge field; parallel transport matters
    TOPOLOGICAL,         // knot-like; writhe contributes to the Lagrangian
};

// The variational / dynamical principle governing motion.
enum class Dynamics : uint8_t {
    LEAST_ACTION,            // δS=0; standard Lagrangian mechanics
    MAXIMUM_ENTROPY_PROD,    // dissipative; system follows steepest entropy gradient
    MINIMUM_ENERGY,          // overdamped; kinetic energy immediately dissipated
    GEODESIC_FLOW,           // motion is a geodesic in configuration space
    MEAN_CURVATURE_FLOW,     // configuration space flows toward minimal surface
    RICCI_FLOW,              // metric evolves; bobs are tracers in the flow
    DISCRETE_MAP,            // iterated algebraic map (no ODE; integer or real steps)
    REACTION_DIFFUSION,      // coupled Turing-style PDEs; bobs are concentration peaks
    CELLULAR_AUTOMATON,      // state is a bitfield; evolution by lookup table
    RETROCAUSAL_BOUNDARY,    // two-point boundary value; shooting method per frame
    STOCHASTIC_DRIFT,        // Lagrangian + Langevin noise term; Itô convention
};

// How the two arms / bobs interact at the joint.
enum class Interaction : uint8_t {
    RIGID_JOINT,         // standard pin; transmits any force
    TORSIONAL_SPRING,    // restoring torque proportional to relative angle
    GAUGE_FIELD,         // interaction mediated by a U(1) gauge boson; adds vector potential
    BERRY_PHASE,         // geometric phase accumulates as the joint precesses
    ENTROPIC_FORCE,      // force derived from entropy gradient (polymer-like)
    CHERN_SIMONS,        // topological interaction; strength from space_curvature param
    MAGNETIC_COUPLING,   // magnetic dipole–dipole; orientation-dependent
    NONLOCAL,            // force depends on spatial correlation over memory_depth
    TORSION,             // spacetime torsion at the joint (Cartan gravity toy model)
    DELAYED,             // force uses the state memory_depth seconds in the past
};

// What the most fundamental substance of this universe is.
enum class Fundamentality : uint8_t {
    MATTER,       // mass is primary
    ENERGY,       // energy density is primary; mass is emergent
    INFORMATION,  // Shannon entropy drives dynamics; bobs are information sources
    SYMMETRY,     // motion is entirely determined by symmetry constraints
    RELATIONAL,   // only relative positions exist; no absolute frame
    GEOMETRIC,    // geometry is primary; matter is curvature
};

// ---------------------------------------------------------------------------
// Conservation laws — which quantities this universe preserves.
// ---------------------------------------------------------------------------
struct Conservation {
    bool energy              = true;
    bool angular_momentum    = true;
    bool linear_momentum     = true;
    bool information         = false;  // only true for unitary dynamics
    bool parity              = true;
    bool time_reversal       = true;
    bool topological_charge  = false;  // only for vortex / soliton bobs
};

// ---------------------------------------------------------------------------
// Valid continuous parameter ranges (used by derive_axioms and checked by
// derive_framework).  Keep these in sync if you extend the system.
// ---------------------------------------------------------------------------
namespace ParamRanges {
    constexpr double GRAVITY_EXPONENT_MIN  = 1.5;
    constexpr double GRAVITY_EXPONENT_MAX  = 3.0;
    constexpr double NOISE_AMPLITUDE_MIN   = 0.0;
    constexpr double NOISE_AMPLITUDE_MAX   = 5.0;
    constexpr double MEMORY_DEPTH_MIN      = 0.0;   // seconds of history; 0 = memoryless
    constexpr double MEMORY_DEPTH_MAX      = 10.0;
    constexpr double SPACE_CURVATURE_MIN   = -2.0;  // negative = hyperbolic
    constexpr double SPACE_CURVATURE_MAX   =  2.0;  // positive = spherical
    constexpr double DISSIPATION_MIN       = 0.0;
    constexpr double DISSIPATION_MAX       = 1.0;
    constexpr double TIME_SCALE_MIN        = 0.1;   // seconds (for cyclic / discrete time)
    constexpr double TIME_SCALE_MAX        = 100.0;
} // namespace ParamRanges

// ---------------------------------------------------------------------------
// The axiom bundle — everything that characterises a seeded universe.
// ---------------------------------------------------------------------------
struct UniverseAxioms {
    // Categorical axioms
    Space         space        = Space::EUCLIDEAN;
    Time          time         = Time::CONTINUOUS;
    BobOntology   bob          = BobOntology::POINT_MASS;
    ArmOntology   arm          = ArmOntology::RIGID_ROD;
    Dynamics      dynamics     = Dynamics::LEAST_ACTION;
    Interaction   interaction  = Interaction::RIGID_JOINT;
    Fundamentality fundamentality = Fundamentality::MATTER;
    Conservation  conservation;

    // Continuous parameters — must lie within the ParamRanges bounds above.
    double gravity_exponent  = 2.0;   // [1.5, 3.0]
    double noise_amplitude   = 0.0;   // [0.0, 5.0]
    double memory_depth      = 0.0;   // [0.0, 10.0] seconds
    double space_curvature   = 0.0;   // [-2.0, 2.0]
    double dissipation       = 0.0;   // [0.0, 1.0]  dimensionless damping ratio
    double time_scale        = 1.0;   // [0.1, 100.0] seconds

    // Raw 64-bit seed preserved for downstream reproducibility checks.
    uint64_t seed = 0;
};

} // namespace Worldline
