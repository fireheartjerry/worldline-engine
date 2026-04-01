#pragma once

namespace Integrators {

// Generic 4th-order Runge-Kutta integrator.
// Preferred fallback for exotic / non-conservative universe types.
// For conservative Hamiltonian systems use velocity_verlet (below) instead.
//
// Requirements on State:
//   State operator+(const State&) const
//   State operator*(double)       const
//
// DerivFn must satisfy:  State fn(const State&, const Params&)
template<typename State, typename Params, typename DerivFn>
State rk4(const State& s, const Params& p, double dt, DerivFn deriv) {
    const double h2 = dt * 0.5;

    const State k1 = deriv(s,           p);
    const State k2 = deriv(s + k1 * h2, p);
    const State k3 = deriv(s + k2 * h2, p);
    const State k4 = deriv(s + k3 * dt, p);

    return s + (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (dt / 6.0);
}

// ============================================================================
// Velocity Verlet (Störmer-Verlet) for conservative Hamiltonian systems.
//
// This integrator is symplectic: it preserves a modified Hamiltonian exactly,
// so the total energy oscillates around the true value rather than drifting.
// Over long simulations it vastly outperforms RK4 for energy conservation.
//
// API contract — the caller provides four small lambdas that encode how the
// State is split into (position, velocity) components:
//
//   PosT  get_pos(const State&)                 — extract position part
//   VelT  get_vel(const State&)                 — extract velocity part
//   State combine(const PosT&, const VelT&)     — reassemble State
//   VelT  accel(const PosT&, const VelT&, const Params&)
//                                               — compute d²q/dt² at (q,v)
//
// Algorithm (kick–drift–kick, 2nd order):
//   v_half = v  + (dt/2) * accel(q,  v,      p)
//   q_new  = q  +  dt    * v_half                    [drift]
//   v_new  = v_half + (dt/2) * accel(q_new, v_half, p)  [second kick]
//
// Note: for non-separable H (kinetic energy depends on q as well as p, as in
// the double pendulum) the second kick uses v_half rather than v_new to keep
// all steps explicit.  This retains 2nd order accuracy and near-symplectic
// behaviour without requiring an implicit solve.
// ============================================================================
template<typename State, typename PosT, typename VelT,
         typename Params,
         typename GetPos, typename GetVel, typename Combine, typename Accel>
State velocity_verlet(const State& s, const Params& p, double dt,
                      GetPos get_pos, GetVel get_vel,
                      Combine combine, Accel accel) {
    const PosT q    = get_pos(s);
    const VelT v    = get_vel(s);
    const double h2 = dt * 0.5;

    // First half-kick
    const VelT a0    = accel(q, v, p);
    const VelT v_h   = v + a0 * h2;

    // Drift
    const PosT q_new = q + v_h * dt;

    // Second half-kick (uses v_half for explicit evaluation of non-sep H)
    const VelT a1    = accel(q_new, v_h, p);
    const VelT v_new = v_h + a1 * h2;

    return combine(q_new, v_new);
}

} // namespace Integrators
