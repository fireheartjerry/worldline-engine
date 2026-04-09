#include "physics/Simulation.hpp"
#include "physics/LawSpec.hpp"
#include "seed/MetaSpec.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool is_finite(double value) {
    return std::isfinite(value);
}

bool is_finite(Vec2 value) {
    return is_finite(value.x) && is_finite(value.y);
}

bool is_finite(const LawState& state) {
    return is_finite(state.q) && is_finite(state.v) && is_finite(state.p);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "physics_verification failed: " << message << '\n';
        std::exit(1);
    }
}

double relative_error(double a, double b) {
    const double scale = std::max(1.0, std::max(std::abs(a), std::abs(b)));
    return std::abs(a - b) / scale;
}

double spectral_norm_symmetric(const double matrix[2][2]) {
    const double trace = matrix[0][0] + matrix[1][1];
    const double disc = std::sqrt(std::max(
        0.0,
        (matrix[0][0] - matrix[1][1]) * (matrix[0][0] - matrix[1][1])
            + 4.0 * matrix[0][1] * matrix[0][1]));
    const double lambda0 = 0.5 * (trace + disc);
    const double lambda1 = 0.5 * (trace - disc);
    return std::max(std::abs(lambda0), std::abs(lambda1));
}

MetaSpec make_law_meta(bool dynamic_p) {
    MetaSpec ms;
    ms.g[0][0] = 1.25; ms.g[0][1] = 0.10;
    ms.g[1][0] = 0.10; ms.g[1][1] = 1.05;

    ms.V[0][0] = 1.60; ms.V[0][1] = 0.20;
    ms.V[1][0] = 0.20; ms.V[1][1] = 0.90;

    ms.C[0][0][0] = 0.28; ms.C[0][0][1] = 0.06;
    ms.C[0][1][0] = 0.06; ms.C[0][1][1] = -0.18;

    ms.C[1][0][0] = 0.12; ms.C[1][0][1] = -0.05;
    ms.C[1][1][0] = -0.05; ms.C[1][1][1] = 0.22;

    ms.S[0][0] = 0.45; ms.S[0][1] = 0.18;
    ms.S[1][0] = 0.18; ms.S[1][1] = -0.22;

    ms.T[0][1] = 0.20;
    ms.T[1][0] = -0.20;

    ms.G[0][0][1] = 0.12;
    ms.G[0][1][0] = -0.12;
    ms.G[1][0][1] = -0.08;
    ms.G[1][1][0] = 0.08;

    ms.W[0][0] = 0.16; ms.W[0][1] = 0.04;
    ms.W[1][0] = 0.04; ms.W[1][1] = -0.10;

    ms.p = 1.35;
    ms.p_dynamic = dynamic_p;
    ms.p_beta = 0.40;
    ms.q0[0] = 0.85;
    ms.q0[1] = 0.10;
    ms.qdot0[0] = 0.15;
    ms.qdot0[1] = 1.20;
    ms.s_a = 0.62;
    ms.s_b = 0.55;
    ms.s_c = 0.38;
    return ms;
}

void require_finite(const Simulation& sim, const std::string& label) {
    require(is_finite(sim.energy()), label + " energy must be finite");
    require(is_finite(sim.dissipation_power()), label + " dissipation power must be finite");
    require(is_finite(sim.bob1_pos()), label + " bob1 position must be finite");
    require(is_finite(sim.bob2_pos()), label + " bob2 position must be finite");
    require(is_finite(sim.theta1()), label + " theta1 must be finite");
    require(is_finite(sim.theta2()), label + " theta2 must be finite");
    require(is_finite(sim.omega1()), label + " omega1 must be finite");
    require(is_finite(sim.omega2()), label + " omega2 must be finite");
}

void test_conservative_rigid_energy_stability() {
    PendulumParams params;
    params.l1 = 1.3;
    params.l2 = 1.1;
    params.m1 = 1.4;
    params.m2 = 1.1;
    params.connector_mode = ConnectorMode::RIGID;
    params.connector1_mass = 0.2;
    params.connector2_mass = 0.15;

    PendulumState state;
    state.theta1 = 2.2;
    state.theta2 = 1.35;
    state.omega1 = -0.4;
    state.omega2 = 0.7;

    Simulation sim(state, params);
    const double initial_energy = sim.energy();
    double max_relative_drift = 0.0;

    for (int i = 0; i < 4000; ++i) {
        sim.step(0.0005);
        require_finite(sim, "conservative rigid");
        max_relative_drift = std::max(max_relative_drift,
            relative_error(sim.energy(), initial_energy));
    }

    require(max_relative_drift < 1.0e-3,
            "conservative rigid energy drift exceeded tolerance");
    require(std::abs(sim.dissipation_power()) < 1.0e-9,
            "conservative rigid dissipation power should remain zero");
}

void test_rigid_resistance_dissipates_energy() {
    PendulumParams params;
    params.l1 = 1.25;
    params.l2 = 1.05;
    params.m1 = 1.5;
    params.m2 = 0.9;
    params.connector_mode = ConnectorMode::RIGID;
    params.bob1_drag = {0.45, 0.20};
    params.bob2_drag = {0.35, 0.15};
    params.connector1_drag = {{0.02, 0.01}, {0.12, 0.08}};
    params.connector2_drag = {{0.02, 0.01}, {0.10, 0.06}};
    params.pivot_resistance = {0.8, 0.15, 0.03};
    params.elbow_resistance = {0.6, 0.10, 0.02};

    PendulumState state;
    state.theta1 = 2.45;
    state.theta2 = 1.65;
    state.omega1 = 0.8;
    state.omega2 = -0.5;

    Simulation sim(state, params);
    const double initial_energy = sim.energy();

    for (int i = 0; i < 6000; ++i) {
        sim.step(0.0005);
        require_finite(sim, "rigid resistance");
        require(sim.dissipation_power() <= 1.0e-8,
                "rigid resistance should not inject power");
    }

    require(sim.energy() < initial_energy - 0.15,
            "rigid resistance should reduce mechanical energy");
}

void test_rope_drag_dissipates_energy() {
    PendulumParams params;
    params.l1 = 1.2;
    params.l2 = 1.0;
    params.m1 = 1.3;
    params.m2 = 0.95;
    params.connector_mode = ConnectorMode::ROPE;
    params.bob1_drag = {0.30, 0.14};
    params.bob2_drag = {0.40, 0.18};
    params.connector1_drag = {{0.01, 0.01}, {0.14, 0.06}};
    params.connector2_drag = {{0.01, 0.01}, {0.18, 0.08}};

    PendulumState state;
    state.theta1 = 1.9;
    state.theta2 = 0.8;
    state.omega1 = 1.0;
    state.omega2 = -1.1;

    Simulation sim(state, params);
    const double initial_energy = sim.energy();

    for (int i = 0; i < 8000; ++i) {
        sim.step(0.0005);
        require_finite(sim, "rope drag");
        require(sim.dissipation_power() <= 1.0e-8,
                "rope drag should not inject power");
    }

    require(sim.energy() < initial_energy - 0.10,
            "rope drag should reduce mechanical energy");
}


void test_anisotropic_drag_prefers_cross_flow() {
    DirectionalDragCoefficients drag;
    drag.axial = {0.2, 0.1};
    drag.normal = {1.5, 0.6};

    const Vec2 axis = {1.0, 0.0};
    const Vec2 axial_velocity = {2.0, 0.0};
    const Vec2 normal_velocity = {0.0, 2.0};

    const Vec2 axial_force = anisotropic_drag_force(axial_velocity, axis, drag);
    const Vec2 normal_force = anisotropic_drag_force(normal_velocity, axis, drag);

    require(std::abs(axial_force.y) < 1.0e-12,
            "axial drag should not create a transverse component");
    require(std::abs(normal_force.x) < 1.0e-12,
            "normal drag should not create an axial component");
    require(std::abs(normal_force.y) > std::abs(axial_force.x) * 3.0,
            "normal drag should dominate cross-flow");
}

void test_adaptive_stepper_matches_fine_reference() {
    PendulumParams adaptive_params;
    adaptive_params.l1 = 1.15;
    adaptive_params.l2 = 0.9;
    adaptive_params.m1 = 1.35;
    adaptive_params.m2 = 1.05;
    adaptive_params.connector_mode = ConnectorMode::RIGID;
    adaptive_params.bob1_drag = {0.8, 0.4};
    adaptive_params.bob2_drag = {0.7, 0.35};
    adaptive_params.connector1_drag = {{0.04, 0.02}, {0.25, 0.12}};
    adaptive_params.connector2_drag = {{0.04, 0.02}, {0.22, 0.10}};
    adaptive_params.pivot_resistance = {1.8, 0.35, 0.04};
    adaptive_params.elbow_resistance = {1.5, 0.30, 0.03};
    adaptive_params.adaptive_tolerance = 2.0e-7;
    adaptive_params.min_substep = 5.0e-6;

    PendulumParams reference_params = adaptive_params;
    reference_params.adaptive_tolerance = 0.0;

    PendulumState state;
    state.theta1 = 2.3;
    state.theta2 = 1.2;
    state.omega1 = 1.4;
    state.omega2 = -1.1;

    Simulation adaptive(state, adaptive_params);
    Simulation reference(state, reference_params);

    for (int i = 0; i < 250; ++i) {
        adaptive.step(0.002);
        for (int j = 0; j < 16; ++j) {
            reference.step(0.002 / 16.0);
        }
    }

    require(relative_error(adaptive.bob2_pos().x, reference.bob2_pos().x) < 3.0e-3,
            "adaptive integrator should track a fine-step reference (bob2.x)");
    require(relative_error(adaptive.bob2_pos().y, reference.bob2_pos().y) < 3.0e-3,
            "adaptive integrator should track a fine-step reference (bob2.y)");
    require(relative_error(adaptive.energy(), reference.energy()) < 5.0e-3,
            "adaptive integrator should track a fine-step reference (energy)");
}

void test_joint_resistance_is_rigid_only() {
    PendulumParams base;
    base.l1 = 1.1;
    base.l2 = 0.95;
    base.m1 = 1.0;
    base.m2 = 0.85;
    base.connector_mode = ConnectorMode::ROPE;

    PendulumState state;
    state.theta1 = 2.1;
    state.theta2 = 1.4;
    state.omega1 = 0.6;
    state.omega2 = -0.9;

    PendulumParams with_joint_loss = base;
    with_joint_loss.pivot_resistance = {5.0, 2.0, 1.0};
    with_joint_loss.elbow_resistance = {4.0, 1.5, 0.8};

    Simulation reference(state, base);
    Simulation candidate(state, with_joint_loss);

    for (int i = 0; i < 3000; ++i) {
        reference.step(0.0005);
        candidate.step(0.0005);
    }

    require(relative_error(reference.bob1_pos().x, candidate.bob1_pos().x) < 1.0e-9,
            "rope mode should ignore pivot/elbow resistance (bob1.x)");
    require(relative_error(reference.bob1_pos().y, candidate.bob1_pos().y) < 1.0e-9,
            "rope mode should ignore pivot/elbow resistance (bob1.y)");
    require(relative_error(reference.bob2_pos().x, candidate.bob2_pos().x) < 1.0e-9,
            "rope mode should ignore pivot/elbow resistance (bob2.x)");
    require(relative_error(reference.bob2_pos().y, candidate.bob2_pos().y) < 1.0e-9,
            "rope mode should ignore pivot/elbow resistance (bob2.y)");
}

void test_visual_diagnostics_expose_resistive_loads() {
    PendulumParams params;
    params.l1 = 1.2;
    params.l2 = 0.85;
    params.m1 = 1.1;
    params.m2 = 0.95;
    params.connector_mode = ConnectorMode::RIGID;
    params.bob1_drag = {0.35, 0.12};
    params.bob2_drag = {0.25, 0.10};
    params.connector1_drag = {{0.02, 0.01}, {0.18, 0.09}};
    params.connector2_drag = {{0.02, 0.01}, {0.14, 0.07}};
    params.pivot_resistance = {0.9, 0.18, 0.04};
    params.elbow_resistance = {0.7, 0.12, 0.03};

    PendulumState state;
    state.theta1 = 2.0;
    state.theta2 = 1.15;
    state.omega1 = 0.75;
    state.omega2 = -0.45;

    Simulation sim(state, params);
    const Simulation::VisualDiagnostics diagnostics = sim.diagnostics();

    require(diagnostics.bob1.drag_force.length() > 1.0e-6,
            "visual diagnostics should expose bob drag");
    require(diagnostics.connector1.active,
            "visual diagnostics should flag active connector drag");
    require(diagnostics.connector1.drag_force.length() > 1.0e-6,
            "visual diagnostics should expose connector drag");
    require(diagnostics.pivot_active && diagnostics.elbow_active,
            "visual diagnostics should flag active joint resistance");
    require(std::abs(diagnostics.pivot_torque) > 1.0e-6,
            "visual diagnostics should expose pivot torque");
    require(std::abs(diagnostics.elbow_torque) > 1.0e-6,
            "visual diagnostics should expose elbow torque");
}

void test_lawspec_binds_linear_potential_gain() {
    MetaSpec ms = make_law_meta(false);
    ms.V[0][0] = 12.0;
    ms.V[1][1] = 9.0;
    ms.V[0][1] = 0.0;
    ms.V[1][0] = 0.0;
    ms.S[0][0] = 0.7;
    ms.S[1][1] = -0.4;
    ms.S[0][1] = ms.S[1][0] = 0.0;
    ms.s_a = 0.75;

    const LawSpec law(ms);
    require(law.potential_linear_gain() <= 1.0,
            "LawSpec must not amplify the linear potential gain");
    require(law.bounded_potential_spectral_norm() <= 1.35 + 1.0e-9,
            "LawSpec must clamp the potential linear spectral norm");
    require(law.bounded_potential_spectral_norm()
                <= law.potential_linear_gain() * (spectral_norm_symmetric(ms.V) + ms.s_a * spectral_norm_symmetric(ms.S)) + 1.0,
            "bounded potential norm should remain tied to the underlying symmetric spectra");
}

void test_lawspec_state_evolution_stays_finite() {
    const LawSpec law(generate_meta_spec("worldline-law-engine"));
    LawState state = law.initial_state();

    require(is_finite(law.derivative(state)),
            "LawSpec derivative must be finite at the seeded initial state");
    for (int i = 0; i < 512; ++i) {
        state = law.step(state, 0.01);
        require(is_finite(state),
                "LawSpec integration must remain finite across repeated RK4 steps");
    }
}

void test_lawspec_dynamic_p_evolves_but_stays_clamped() {
    const LawSpec law(make_law_meta(true));
    LawState state = law.initial_state();
    const double seeded_p = state.p;
    bool changed = false;

    for (int i = 0; i < 128; ++i) {
        state = law.step(state, 0.04);
        require(is_finite(state), "dynamic-p LawSpec state must remain finite");
        require(state.p >= seeded_p - 0.5 - 1.0e-9
                    && state.p <= seeded_p + 0.5 + 1.0e-9,
                "dynamic p must stay clamped around its seeded value");
        if (std::abs(state.p - seeded_p) > 1.0e-3) {
            changed = true;
        }
    }

    require(changed,
            "dynamic p should respond to angular momentum over time");
}

void test_lawspec_static_p_snaps_back_to_seed() {
    const LawSpec law(make_law_meta(false));
    LawState state = law.initial_state();
    state.p = law.seeded_p() + 0.35;

    state = law.step(state, 0.05);
    require(std::abs(state.p - law.seeded_p()) < 1.0e-12,
            "static p should remain fixed at the seeded value");
}

void test_lawspec_runtime_halving_handles_large_acceleration() {
    MetaSpec ms = make_law_meta(true);
    ms.p = 4.0;
    ms.V[0][0] = 14.0;
    ms.V[1][1] = 10.0;
    ms.C[0][0][0] = 8.0;
    ms.C[0][1][1] = -7.0;
    ms.q0[0] = 10.0;
    ms.q0[1] = -8.0;
    ms.qdot0[0] = 3.0;
    ms.qdot0[1] = 2.5;

    const LawSpec law(ms);
    const LawState initial = law.initial_state();
    require(law.acceleration(initial).length() > law.acceleration_ceiling(),
            "test setup should exceed the acceleration ceiling and trigger step refinement");

    const LawState advanced = law.step(initial, 0.5);
    require(is_finite(advanced),
            "adaptive LawSpec stepping must remain finite under large acceleration");
    require(advanced.p >= law.seeded_p() - 0.5 - 1.0e-9
                && advanced.p <= law.seeded_p() + 0.5 + 1.0e-9,
            "adaptive stepping must preserve the dynamic p clamp");
}

} // namespace
int main() {
    test_conservative_rigid_energy_stability();
    test_rigid_resistance_dissipates_energy();
    test_rope_drag_dissipates_energy();
    test_anisotropic_drag_prefers_cross_flow();
    test_adaptive_stepper_matches_fine_reference();
    test_joint_resistance_is_rigid_only();
    test_visual_diagnostics_expose_resistive_loads();
    test_lawspec_binds_linear_potential_gain();
    test_lawspec_state_evolution_stays_finite();
    test_lawspec_dynamic_p_evolves_but_stays_clamped();
    test_lawspec_static_p_snaps_back_to_seed();
    test_lawspec_runtime_halving_handles_large_acceleration();
    std::cout << "physics_verification passed\n";
    return 0;
}


