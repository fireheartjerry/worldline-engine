#pragma once
#include "PendulumState.hpp"
#include "PendulumParams.hpp"
#include "FlowField.hpp"
#include "PendulumDerivatives.hpp"
#include "ResistiveForces.hpp"
#include "../math/Integrators.hpp"
#include "../math/Vec2.hpp"
#include <cmath>
#include <cstring>

class Simulation {
public:
    struct BodyDiagnostics {
        Vec2 position{};
        Vec2 velocity{};
        Vec2 gravity_force{};
        Vec2 drag_force{};
        Vec2 reaction_force{};
        Vec2 net_force{};
        bool constrained = false;
    };

    struct LinkDiagnostics {
        Vec2 position{};
        Vec2 direction{};
        Vec2 drag_force{};
        bool active = false;
        bool taut = false;
    };

    struct VisualDiagnostics {
        BodyDiagnostics bob1{};
        BodyDiagnostics bob2{};
        LinkDiagnostics connector1{};
        LinkDiagnostics connector2{};
        double pivot_torque = 0.0;
        double elbow_torque = 0.0;
        bool pivot_active = false;
        bool elbow_active = false;
    };

    struct RopeState {
        Vec2 bob1{};
        Vec2 bob2{};
        Vec2 vel1{};
        Vec2 vel2{};
        bool taut1 = true;
        bool taut2 = true;
    };

    PendulumState state;
    PendulumParams params;

    explicit Simulation(PendulumState initial = {}, PendulumParams p = {})
        : state(initial), params(p) {
        reset(initial, p);
    }

    void reset(PendulumState initial, PendulumParams p) {
        state = initial;
        params = p;
        elapsed_time = 0.0;
        if (params.connector_mode == ConnectorMode::ROPE) {
            rope = rope_from_state(initial, params);
        }
        mark_cache_dirty();
    }

    // Hot-path variant: re-pins the physics state without copying the 296-byte
    // PendulumParams. Use instead of reset() when params are unchanged.
    void update_state(PendulumState new_state) {
        state = new_state;
        if (params.connector_mode == ConnectorMode::ROPE) {
            rope = rope_from_state(new_state, params);
        }
        mark_cache_dirty();
    }

    void step(double dt) {
        if (params.adaptive_tolerance > 0.0) {
            if (params.connector_mode == ConnectorMode::RIGID) {
                step_rigid_adaptive(dt, 0);
            } else {
                step_rope_adaptive(dt, 0);
            }
        } else if (params.connector_mode == ConnectorMode::RIGID) {
            step_rigid_once(dt);
        } else {
            step_rope(dt);
        }
        elapsed_time += dt;
        mark_cache_dirty();
    }

    Vec2 bob1_pos() const {
        return (params.connector_mode == ConnectorMode::RIGID)
            ? bob1_pos(state, params)
            : rope.bob1;
    }

    Vec2 bob2_pos() const {
        return (params.connector_mode == ConnectorMode::RIGID)
            ? bob2_pos(state, params)
            : rope.bob2;
    }

    Vec2 connector1_com() const {
        return bob1_pos() * 0.5;
    }

    Vec2 connector2_com() const {
        const Vec2 b1 = bob1_pos();
        const Vec2 b2 = bob2_pos();
        return (b1 + b2) * 0.5;
    }

    double theta1() const {
        if (params.connector_mode == ConnectorMode::RIGID) {
            return state.theta1;
        }
        return std::atan2(rope.bob1.x, rope.bob1.y);
    }

    double theta2() const {
        if (params.connector_mode == ConnectorMode::RIGID) {
            return state.theta2;
        }
        const Vec2 rel = rope.bob2 - rope.bob1;
        return std::atan2(rel.x, rel.y);
    }

    double omega1() const {
        if (params.connector_mode == ConnectorMode::RIGID) {
            return state.omega1;
        }
        return angular_velocity(rope.bob1, rope.vel1);
    }

    double omega2() const {
        if (params.connector_mode == ConnectorMode::RIGID) {
            return state.omega2;
        }
        return angular_velocity(rope.bob2 - rope.bob1, rope.vel2 - rope.vel1);
    }

    bool connector1_taut() const {
        return (params.connector_mode == ConnectorMode::RIGID) ? true : rope.taut1;
    }

    bool connector2_taut() const {
        return (params.connector_mode == ConnectorMode::RIGID) ? true : rope.taut2;
    }

    double energy() const {
        return (params.connector_mode == ConnectorMode::RIGID)
            ? energy(state, params)
            : rope_energy();
    }

    const VisualDiagnostics& diagnostics() const {
        ensure_cached_diagnostics();
        return cached_diagnostics;
    }

    double dissipation_power() const {
        ensure_cached_diagnostics();
        return cached_dissipation_power;
    }

    bool resistance_enabled() const {
        const bool drag_enabled =
            drag_active(params.bob1_drag)
            || drag_active(params.bob2_drag)
            || drag_active(params.connector1_drag)
            || drag_active(params.connector2_drag);
        const bool joint_enabled =
            params.connector_mode == ConnectorMode::RIGID
            && (joint_resistance_active(params.pivot_resistance)
                || joint_resistance_active(params.elbow_resistance));
        return drag_enabled || joint_enabled;
    }

    double reach() const { return params.l1 + params.l2; }

    bool rigid_connectors() const { return params.connector_mode == ConnectorMode::RIGID; }
    bool connectors_have_mass() const {
        return params.connector1_mass > 1e-6 || params.connector2_mass > 1e-6;
    }
    bool ambient_flow_enabled() const {
        return flow_field_active(params.flow_field);
    }
    Vec2 flow_velocity(Vec2 position) const {
        return flow_velocity_at(position, params.flow_field);
    }

    static Vec2 bob1_pos(const PendulumState& s, const PendulumParams& p) {
        return {
            p.l1 * std::sin(s.theta1),
            p.l1 * std::cos(s.theta1)
        };
    }

    static Vec2 bob2_pos(const PendulumState& s, const PendulumParams& p) {
        const Vec2 b1 = bob1_pos(s, p);
        return {
            b1.x + p.l2 * std::sin(s.theta2),
            b1.y + p.l2 * std::cos(s.theta2)
        };
    }

    static double energy(const PendulumState& s, const PendulumParams& p) {
        const double connector1_mass =
            (p.connector_mode == ConnectorMode::RIGID) ? p.connector1_mass : 0.0;
        const double connector2_mass =
            (p.connector_mode == ConnectorMode::RIGID) ? p.connector2_mass : 0.0;

        const double delta = s.theta1 - s.theta2;
        const double inertia11 = p.l1 * p.l1 *
            (p.m1 + p.m2 + connector2_mass + connector1_mass / 3.0);
        const double inertia22 = p.l2 * p.l2 *
            (p.m2 + connector2_mass / 3.0);
        const double coupling = p.l1 * p.l2 *
            (p.m2 + connector2_mass / 2.0);
        const double kinetic =
            0.5 * inertia11 * s.omega1 * s.omega1
            + 0.5 * inertia22 * s.omega2 * s.omega2
            + coupling * std::cos(delta) * s.omega1 * s.omega2;
        const double potential =
            -p.g * p.l1 * (p.m1 + p.m2 + connector2_mass + connector1_mass / 2.0) * std::cos(s.theta1)
            -p.g * p.l2 * (p.m2 + connector2_mass / 2.0) * std::cos(s.theta2);
        return kinetic + potential;
    }

private:
    RopeState rope{};
    double elapsed_time = 0.0;
    mutable bool cache_valid = false;
    mutable PendulumState cached_state_snapshot{};
    mutable RopeState cached_rope_snapshot{};
    mutable PendulumParams cached_params_snapshot{};
    mutable VisualDiagnostics cached_diagnostics{};
    mutable double cached_dissipation_power = 0.0;

    void mark_cache_dirty() { cache_valid = false; }

    bool cache_matches_state() const {
        // memcmp is safe: all three types are trivially-copyable POD with no
        // uninitialized padding (value-initialized at construction, snapshots
        // written via memcpy which copies padding bytes too).
        // Also fixes a latent bug: the old same_flow_field() omitted gust_x/y/frequency.
        return cache_valid
            && !std::memcmp(&cached_state_snapshot, &state, sizeof(PendulumState))
            && !std::memcmp(&cached_rope_snapshot,  &rope,  sizeof(RopeState))
            && !std::memcmp(&cached_params_snapshot, &params, sizeof(PendulumParams));
    }

    double compute_dissipation_power_uncached() const {
        if (params.connector_mode == ConnectorMode::RIGID) {
            const GeneralizedForces forces = rigid_resistive_forces(state, params, elapsed_time);
            return forces.q1 * state.omega1 + forces.q2 * state.omega2;
        }

        Vec2 force1{};
        Vec2 force2{};
        accumulate_rope_external_forces(force1, force2, elapsed_time);
        return force1.dot(rope.vel1) + force2.dot(rope.vel2);
    }

    void refresh_cached_diagnostics() const {
        cached_diagnostics = (params.connector_mode == ConnectorMode::RIGID)
            ? rigid_diagnostics()
            : rope_diagnostics();
        cached_dissipation_power = compute_dissipation_power_uncached();
        std::memcpy(&cached_state_snapshot, &state,  sizeof(PendulumState));
        std::memcpy(&cached_rope_snapshot,  &rope,   sizeof(RopeState));
        std::memcpy(&cached_params_snapshot, &params, sizeof(PendulumParams));
        cache_valid = true;
    }

    void ensure_cached_diagnostics() const {
        if (!cache_matches_state()) {
            refresh_cached_diagnostics();
        }
    }

    struct EndpointForces {
        Vec2 first{};
        Vec2 second{};
    };

    static Vec2 tangent_from_angle(double length, double theta) {
        return {
            length * std::cos(theta),
            -length * std::sin(theta)
        };
    }

    static Vec2 rigid_point_acceleration(Vec2 radius,
                                         Vec2 tangent,
                                         double omega,
                                         double alpha) {
        return tangent * alpha - radius * (omega * omega);
    }

    static double angular_velocity(Vec2 radius, Vec2 velocity) {
        const double denom = radius.length_sq();
        if (denom < 1e-9) {
            return 0.0;
        }
        return (radius.y * velocity.x - radius.x * velocity.y) / denom;
    }

    static RopeState rope_from_state(const PendulumState& initial,
                                     const PendulumParams& params) {
        RopeState rope_state;
        rope_state.bob1 = bob1_pos(initial, params);
        rope_state.bob2 = bob2_pos(initial, params);
        rope_state.vel1 = tangent_from_angle(params.l1, initial.theta1) * initial.omega1;
        rope_state.vel2 = rope_state.vel1
            + tangent_from_angle(params.l2, initial.theta2) * initial.omega2;
        rope_state.taut1 = true;
        rope_state.taut2 = true;
        return rope_state;
    }

    double rope_energy() const {
        const double kinetic =
            0.5 * params.m1 * rope.vel1.length_sq()
            + 0.5 * params.m2 * rope.vel2.length_sq();
        const double potential =
            -params.g * (params.m1 * rope.bob1.y + params.m2 * rope.bob2.y);
        return kinetic + potential;
    }

    static Vec2 segment_drag_resultant(Vec2 start_position,
                                       Vec2 end_position,
                                       Vec2 start_velocity,
                                       Vec2 end_velocity,
                                       Vec2 axis_hint,
                                       const DirectionalDragCoefficients& drag,
                                       const FlowFieldParams& flow_field,
                                       double time = 0.0) {
        const EndpointForces f = segment_drag_forces(start_position, end_position,
                                                     start_velocity, end_velocity,
                                                     axis_hint, drag, flow_field, time);
        return f.first + f.second;
    }

    static EndpointForces segment_drag_forces(Vec2 start_position,
                                             Vec2 end_position,
                                             Vec2 start_velocity,
                                             Vec2 end_velocity,
                                             Vec2 axis_hint,
                                             const DirectionalDragCoefficients& drag,
                                             const FlowFieldParams& flow_field,
                                             double time = 0.0) {
        EndpointForces forces;
        if (!drag_active(drag)) {
            return forces;
        }

        const Vec2 delta_position = end_position - start_position;
        const Vec2 delta_velocity = end_velocity - start_velocity;
        const double segment_length = delta_position.length();
        if (segment_length <= 1e-9) {
            return forces;
        }
        integrate_unit_interval_gauss5([&](double s_unit, double weight) {
            const Vec2 position = start_position + delta_position * s_unit;
            const Vec2 velocity = start_velocity + delta_velocity * s_unit;
            const Vec2 density_force =
                anisotropic_drag_force(
                    velocity - flow_velocity_at(position, flow_field, time),
                    axis_hint,
                    drag);
            const double line_weight = weight * segment_length;
            forces.first += density_force * (line_weight * (1.0 - s_unit));
            forces.second += density_force * (line_weight * s_unit);
        });
        return forces;
    }

    void accumulate_rope_external_forces(Vec2& force1, Vec2& force2,
                                         double time = 0.0) const {
        force1 += drag_force(
            rope.vel1 - flow_velocity_at(rope.bob1, params.flow_field, time),
            params.bob1_drag);
        force2 += drag_force(
            rope.vel2 - flow_velocity_at(rope.bob2, params.flow_field, time),
            params.bob2_drag);

        if (rope.taut1) {
            const EndpointForces connector1 =
                segment_drag_forces({},
                                    rope.bob1,
                                    {},
                                    rope.vel1,
                                    rope.bob1,
                                    params.connector1_drag,
                                    params.flow_field,
                                    time);
            force1 += connector1.second;
        }

        if (rope.taut2) {
            const EndpointForces connector2 =
                segment_drag_forces(rope.bob1,
                                    rope.bob2,
                                    rope.vel1,
                                    rope.vel2,
                                    rope.bob2 - rope.bob1,
                                    params.connector2_drag,
                                    params.flow_field,
                                    time);
            force1 += connector2.first;
            force2 += connector2.second;
        }
    }

    VisualDiagnostics rigid_diagnostics() const {
        VisualDiagnostics diagnostics;

        const Vec2 r1 = bob1_pos(state, params);
        const Vec2 r2 = {
            params.l2 * std::sin(state.theta2),
            params.l2 * std::cos(state.theta2)
        };
        const Vec2 b2 = r1 + r2;
        const Vec2 t1 = tangent_from_angle(params.l1, state.theta1);
        const Vec2 t2 = tangent_from_angle(params.l2, state.theta2);
        const Vec2 vel1 = t1 * state.omega1;
        const Vec2 vel2 = vel1 + t2 * state.omega2;
        const Vec2 fluid1 = flow_velocity_at(r1, params.flow_field, elapsed_time);
        const Vec2 fluid2 = flow_velocity_at(b2, params.flow_field, elapsed_time);
        const PendulumState deriv = pendulum_derivatives(state, params, elapsed_time);
        const Vec2 acc1 = rigid_point_acceleration(r1, t1, state.omega1, deriv.omega1);
        const Vec2 acc2 = acc1 + rigid_point_acceleration(r2, t2, state.omega2, deriv.omega2);

        diagnostics.bob1.position = r1;
        diagnostics.bob1.velocity = vel1;
        diagnostics.bob1.gravity_force = {0.0, params.m1 * params.g};
        diagnostics.bob1.drag_force = drag_force(vel1 - fluid1, params.bob1_drag);
        diagnostics.bob1.net_force = acc1 * params.m1;
        diagnostics.bob1.reaction_force =
            diagnostics.bob1.net_force - diagnostics.bob1.gravity_force - diagnostics.bob1.drag_force;
        diagnostics.bob1.constrained = true;

        diagnostics.bob2.position = b2;
        diagnostics.bob2.velocity = vel2;
        diagnostics.bob2.gravity_force = {0.0, params.m2 * params.g};
        diagnostics.bob2.drag_force = drag_force(vel2 - fluid2, params.bob2_drag);
        diagnostics.bob2.net_force = acc2 * params.m2;
        diagnostics.bob2.reaction_force =
            diagnostics.bob2.net_force - diagnostics.bob2.gravity_force - diagnostics.bob2.drag_force;
        diagnostics.bob2.constrained = true;

        diagnostics.connector1.position = r1 * 0.5;
        diagnostics.connector1.direction = r1;
        diagnostics.connector1.drag_force =
            segment_drag_resultant({},
                                   r1,
                                   {},
                                   vel1,
                                   r1,
                                   params.connector1_drag,
                                   params.flow_field,
                                   elapsed_time);
        diagnostics.connector1.active = drag_active(params.connector1_drag);
        diagnostics.connector1.taut = true;

        diagnostics.connector2.position = r1 + r2 * 0.5;
        diagnostics.connector2.direction = r2;
        diagnostics.connector2.drag_force =
            segment_drag_resultant(r1,
                                   b2,
                                   vel1,
                                   vel2,
                                   r2,
                                   params.connector2_drag,
                                   params.flow_field,
                                   elapsed_time);
        diagnostics.connector2.active = drag_active(params.connector2_drag);
        diagnostics.connector2.taut = true;
        diagnostics.pivot_torque =
            joint_resistive_torque(state.omega1, params.pivot_resistance);
        diagnostics.elbow_torque =
            joint_resistive_torque(state.omega2 - state.omega1,
                                   params.elbow_resistance);
        diagnostics.pivot_active =
            joint_resistance_active(params.pivot_resistance);
        diagnostics.elbow_active =
            joint_resistance_active(params.elbow_resistance);

        return diagnostics;
    }

    VisualDiagnostics rope_diagnostics() const {
        VisualDiagnostics diagnostics;

        const Vec2 bob_drag1 = drag_force(
            rope.vel1 - flow_velocity_at(rope.bob1, params.flow_field, elapsed_time),
            params.bob1_drag);
        const Vec2 bob_drag2 = drag_force(
            rope.vel2 - flow_velocity_at(rope.bob2, params.flow_field, elapsed_time),
            params.bob2_drag);

        // Compute connector drag once via segment_drag_forces and reuse the result
        // for both the physics (external force accumulation) and the display overlay.
        // Eliminates two redundant Gauss5 calls that segment_drag_resultant would add.
        Vec2 external1 = bob_drag1;
        Vec2 external2 = bob_drag2;
        Vec2 connector1_resultant{};
        Vec2 connector2_resultant{};

        if (rope.taut1) {
            const EndpointForces c1 = segment_drag_forces({}, rope.bob1, {}, rope.vel1,
                                                           rope.bob1, params.connector1_drag,
                                                           params.flow_field, elapsed_time);
            external1 += c1.second;
            connector1_resultant = c1.first + c1.second;
        }
        if (rope.taut2) {
            const Vec2 rel2 = rope.bob2 - rope.bob1;
            const EndpointForces c2 = segment_drag_forces(rope.bob1, rope.bob2,
                                                           rope.vel1, rope.vel2,
                                                           rel2,
                                                           params.connector2_drag,
                                                           params.flow_field, elapsed_time);
            external1 += c2.first;
            external2 += c2.second;
            connector2_resultant = c2.first + c2.second;
        }

        const Vec2 gravity1 = {0.0, params.m1 * params.g};
        const Vec2 gravity2 = {0.0, params.m2 * params.g};

        Vec2 accel1 = gravity1 / params.m1 + external1 / params.m1;
        Vec2 accel2 = gravity2 / params.m2 + external2 / params.m2;
        Simulation probe = *this;
        probe.solve_rope_constraints(accel1, accel2);

        diagnostics.bob1.position = rope.bob1;
        diagnostics.bob1.velocity = rope.vel1;
        diagnostics.bob1.gravity_force = gravity1;
        diagnostics.bob1.drag_force = bob_drag1;
        diagnostics.bob1.net_force = accel1 * params.m1;
        diagnostics.bob1.reaction_force =
            diagnostics.bob1.net_force - diagnostics.bob1.gravity_force - diagnostics.bob1.drag_force;
        diagnostics.bob1.constrained = rope.taut1 || rope.taut2;

        diagnostics.bob2.position = rope.bob2;
        diagnostics.bob2.velocity = rope.vel2;
        diagnostics.bob2.gravity_force = gravity2;
        diagnostics.bob2.drag_force = bob_drag2;
        diagnostics.bob2.net_force = accel2 * params.m2;
        diagnostics.bob2.reaction_force =
            diagnostics.bob2.net_force - diagnostics.bob2.gravity_force - diagnostics.bob2.drag_force;
        diagnostics.bob2.constrained = rope.taut2;

        diagnostics.connector1.position = rope.bob1 * 0.5;
        diagnostics.connector1.direction = rope.bob1;
        diagnostics.connector1.drag_force = connector1_resultant;
        diagnostics.connector1.active = rope.taut1 && drag_active(params.connector1_drag);
        diagnostics.connector1.taut = rope.taut1;

        const Vec2 rel = rope.bob2 - rope.bob1;
        diagnostics.connector2.position = rope.bob1 + rel * 0.5;
        diagnostics.connector2.direction = rel;
        diagnostics.connector2.drag_force = connector2_resultant;
        diagnostics.connector2.active = rope.taut2 && drag_active(params.connector2_drag);
        diagnostics.connector2.taut = rope.taut2;

        return diagnostics;
    }

    void step_rigid_once(double dt) {
        const double t = elapsed_time;
        state = Integrators::rk4<PendulumState, PendulumParams>(
            state, params, dt,
            [t](const PendulumState& s, const PendulumParams& p) {
                return pendulum_derivatives(s, p, t);
            });
    }

    static double rigid_state_error(const PendulumState& coarse,
                                    const PendulumState& fine) {
        auto rel = [](double a, double b) {
            return std::abs(a - b) / (1.0 + std::max(std::abs(a), std::abs(b)));
        };
        return std::max(std::max(rel(coarse.theta1, fine.theta1),
                                 rel(coarse.theta2, fine.theta2)),
                        std::max(rel(coarse.omega1, fine.omega1),
                                 rel(coarse.omega2, fine.omega2)));
    }

    static double rope_state_error(const RopeState& coarse,
                                   const RopeState& fine) {
        auto rel = [](double a, double b) {
            return std::abs(a - b) / (1.0 + std::max(std::abs(a), std::abs(b)));
        };

        double error = 0.0;
        error = std::max(error, rel(coarse.bob1.x, fine.bob1.x));
        error = std::max(error, rel(coarse.bob1.y, fine.bob1.y));
        error = std::max(error, rel(coarse.bob2.x, fine.bob2.x));
        error = std::max(error, rel(coarse.bob2.y, fine.bob2.y));
        error = std::max(error, rel(coarse.vel1.x, fine.vel1.x));
        error = std::max(error, rel(coarse.vel1.y, fine.vel1.y));
        error = std::max(error, rel(coarse.vel2.x, fine.vel2.x));
        error = std::max(error, rel(coarse.vel2.y, fine.vel2.y));
        return error;
    }

    void step_rigid_adaptive(double dt, int depth) {
        if (dt <= params.min_substep || depth >= params.max_refinements) {
            step_rigid_once(dt);
            return;
        }

        // Save only the 32-byte physics state — not the full Simulation (cache, rope, etc.)
        const PendulumState s0 = state;

        step_rigid_once(dt);
        const PendulumState s_coarse = state;

        state = s0;
        step_rigid_once(dt * 0.5);
        step_rigid_once(dt * 0.5);
        // state now holds the fine result

        if (rigid_state_error(s_coarse, state) <= params.adaptive_tolerance) {
            return;
        }

        state = s0;
        step_rigid_adaptive(dt * 0.5, depth + 1);
        step_rigid_adaptive(dt * 0.5, depth + 1);
    }

    void step_rope_adaptive(double dt, int depth) {
        if (dt <= params.min_substep || depth >= params.max_refinements) {
            step_rope(dt);
            return;
        }

        // Save only mutable physics state (~82 bytes) — not the full Simulation
        const RopeState r0 = rope;
        const PendulumState s0 = state;

        step_rope(dt);
        const RopeState r_coarse = rope;

        rope = r0; state = s0;
        step_rope(dt * 0.5);
        step_rope(dt * 0.5);
        // rope + state now hold the fine result

        if (rope_state_error(r_coarse, rope) <= params.adaptive_tolerance) {
            return;
        }

        rope = r0; state = s0;
        step_rope_adaptive(dt * 0.5, depth + 1);
        step_rope_adaptive(dt * 0.5, depth + 1);
    }

    void step_rope(double dt) {
        constexpr double POS_EPS = 1e-6;
        constexpr double VEL_EPS = 1e-5;

        auto release_if_slacking = [&](bool& taut, Vec2 radius, Vec2 velocity) {
            if (!taut) {
                return;
            }
            if (radius.dot(velocity) < -VEL_EPS) {
                taut = false;
            }
        };

        release_if_slacking(rope.taut1, rope.bob1, rope.vel1);
        release_if_slacking(rope.taut2, rope.bob2 - rope.bob1, rope.vel2 - rope.vel1);

        const Vec2 gravity = {0.0, params.g};
        Vec2 force1{};
        Vec2 force2{};
        accumulate_rope_external_forces(force1, force2, elapsed_time);

        Vec2 accel1 = gravity + force1 / params.m1;
        Vec2 accel2 = gravity + force2 / params.m2;

        solve_rope_constraints(accel1, accel2);

        rope.vel1 += accel1 * dt;
        rope.vel2 += accel2 * dt;
        rope.bob1 += rope.vel1 * dt;
        rope.bob2 += rope.vel2 * dt;

        project_rope_constraint1(POS_EPS, VEL_EPS);
        project_rope_constraint2(POS_EPS, VEL_EPS);
        project_rope_constraint1(POS_EPS, VEL_EPS);

        state.theta1 = theta1();
        state.theta2 = theta2();
        state.omega1 = omega1();
        state.omega2 = omega2();
    }

    void solve_rope_constraints(Vec2& accel1, Vec2& accel2) {
        const double inv1 = 1.0 / params.m1;
        const double inv2 = 1.0 / params.m2;
        const Vec2 free1 = accel1;
        const Vec2 free2 = accel2;

        bool active1 = rope.taut1 && std::abs(rope.bob1.length() - params.l1) < 1e-3;
        bool active2 = rope.taut2 && std::abs((rope.bob2 - rope.bob1).length() - params.l2) < 1e-3;

        auto solve_single_1 = [&]() {
            const double radius_sq = rope.bob1.length_sq();
            if (radius_sq < 1e-9) {
                rope.taut1 = false;
                accel1 = free1;
                accel2 = free2;
                return;
            }
            const double rhs = -(rope.bob1.dot(free1) + rope.vel1.length_sq());
            const double lambda = rhs / (radius_sq * inv1);
            if (lambda > 0.0) {
                rope.taut1 = false;
                accel1 = free1;
                accel2 = free2;
                return;
            }
            accel1 = free1 + rope.bob1 * (lambda * inv1);
            accel2 = free2;
            rope.taut1 = true;
        };

        auto solve_single_2 = [&]() {
            const Vec2 d = rope.bob2 - rope.bob1;
            const double dist_sq = d.length_sq();
            const Vec2 rel_vel = rope.vel2 - rope.vel1;
            if (dist_sq < 1e-9) {
                rope.taut2 = false;
                accel1 = free1;
                accel2 = free2;
                return;
            }
            const double rhs = -(d.dot(free2 - free1) + rel_vel.length_sq());
            const double lambda = rhs / (dist_sq * (inv1 + inv2));
            if (lambda > 0.0) {
                rope.taut2 = false;
                accel1 = free1;
                accel2 = free2;
                return;
            }
            accel1 = free1 - d * (lambda * inv1);
            accel2 = free2 + d * (lambda * inv2);
            rope.taut2 = true;
        };

        if (active1 && active2) {
            const Vec2 d = rope.bob2 - rope.bob1;
            const Vec2 rel_vel = rope.vel2 - rope.vel1;

            const double a11 = rope.bob1.length_sq() * inv1;
            const double a12 = -rope.bob1.dot(d) * inv1;
            const double a22 = d.length_sq() * (inv1 + inv2);
            const double b1 = -(rope.bob1.dot(free1) + rope.vel1.length_sq());
            const double b2 = -(d.dot(free2 - free1) + rel_vel.length_sq());
            const double det = a11 * a22 - a12 * a12;

            if (std::abs(det) > 1e-9) {
                const double lambda1 = (b1 * a22 - a12 * b2) / det;
                const double lambda2 = (a11 * b2 - a12 * b1) / det;

                if (lambda1 <= 0.0 && lambda2 <= 0.0) {
                    accel1 = free1 + rope.bob1 * (lambda1 * inv1) - d * (lambda2 * inv1);
                    accel2 = free2 + d * (lambda2 * inv2);
                    rope.taut1 = true;
                    rope.taut2 = true;
                    return;
                }

                if (lambda1 > 0.0 && lambda2 <= 0.0) {
                    rope.taut1 = false;
                    solve_single_2();
                    return;
                }
                if (lambda2 > 0.0 && lambda1 <= 0.0) {
                    rope.taut2 = false;
                    solve_single_1();
                    return;
                }
            }

            rope.taut1 = false;
            rope.taut2 = false;
            accel1 = free1;
            accel2 = free2;
            solve_single_1();
            if (!rope.taut1) {
                solve_single_2();
            }
            return;
        }

        accel1 = free1;
        accel2 = free2;
        if (active1) {
            solve_single_1();
        }
        if (active2) {
            solve_single_2();
        }
    }

    void project_rope_constraint1(double pos_eps, double vel_eps) {
        const double length = rope.bob1.length();
        if (length <= 1e-9) {
            rope.taut1 = false;
            return;
        }

        const Vec2 normal = rope.bob1 / length;
        const bool stretched = length > params.l1 + pos_eps;
        const bool near_boundary = std::abs(length - params.l1) <= pos_eps;
        const double radial = rope.vel1.dot(normal);

        if (stretched || (rope.taut1 && near_boundary)) {
            rope.bob1 = normal * params.l1;
            if (radial > vel_eps || rope.taut1) {
                rope.vel1 -= normal * radial;
            }
            rope.taut1 = true;
        } else if (length < params.l1 - pos_eps) {
            rope.taut1 = false;
        }
    }

    void project_rope_constraint2(double pos_eps, double vel_eps) {
        Vec2 delta = rope.bob2 - rope.bob1;
        const double length = delta.length();
        if (length <= 1e-9) {
            rope.taut2 = false;
            return;
        }

        const Vec2 normal = delta / length;
        const bool stretched = length > params.l2 + pos_eps;
        const bool near_boundary = std::abs(length - params.l2) <= pos_eps;
        const Vec2 rel_vel = rope.vel2 - rope.vel1;
        const double radial = rel_vel.dot(normal);

        if (stretched || (rope.taut2 && near_boundary)) {
            const double correction = length - params.l2;
            const double total_mass = params.m1 + params.m2;
            rope.bob1 += normal * (correction * (params.m2 / total_mass));
            rope.bob2 -= normal * (correction * (params.m1 / total_mass));

            if (radial > vel_eps || rope.taut2) {
                const double impulse = radial / ((1.0 / params.m1) + (1.0 / params.m2));
                rope.vel1 += normal * (impulse / params.m1);
                rope.vel2 -= normal * (impulse / params.m2);
            }
            rope.taut2 = true;
        } else if (length < params.l2 - pos_eps) {
            rope.taut2 = false;
        }
    }
};
