#include "physics/LawSpec.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kEpsilon = 1.0e-9;
constexpr double kPotentialNormBound = 1.35;

struct Mat2 {
    double xx = 0.0;
    double xy = 0.0;
    double yx = 0.0;
    double yy = 0.0;
};

struct SymmetricAnalysis {
    double lambda_major = 0.0;
    double lambda_minor = 0.0;
    double theta_major = 0.0;
};

Mat2 load(const double source[2][2]) {
    return {
        source[0][0],
        source[0][1],
        source[1][0],
        source[1][1]
    };
}

void store(const Mat2& matrix, double target[2][2]) {
    target[0][0] = matrix.xx;
    target[0][1] = matrix.xy;
    target[1][0] = matrix.yx;
    target[1][1] = matrix.yy;
}

Mat2 add(const Mat2& lhs, const Mat2& rhs) {
    return {
        lhs.xx + rhs.xx,
        lhs.xy + rhs.xy,
        lhs.yx + rhs.yx,
        lhs.yy + rhs.yy
    };
}

Mat2 scale(const Mat2& matrix, double factor) {
    return {
        matrix.xx * factor,
        matrix.xy * factor,
        matrix.yx * factor,
        matrix.yy * factor
    };
}

Vec2 mul(const Mat2& matrix, Vec2 value) {
    return {
        matrix.xx * value.x + matrix.xy * value.y,
        matrix.yx * value.x + matrix.yy * value.y
    };
}

double determinant(const Mat2& matrix) {
    return matrix.xx * matrix.yy - matrix.xy * matrix.yx;
}

double frob(const Mat2& matrix) {
    return std::sqrt(
        matrix.xx * matrix.xx +
        matrix.xy * matrix.xy +
        matrix.yx * matrix.yx +
        matrix.yy * matrix.yy);
}

double spectral_norm_symmetric(const Mat2& matrix) {
    const double trace = matrix.xx + matrix.yy;
    const double disc = std::sqrt(std::max(
        0.0,
        (matrix.xx - matrix.yy) * (matrix.xx - matrix.yy) + 4.0 * matrix.xy * matrix.xy));
    const double lambda0 = 0.5 * (trace + disc);
    const double lambda1 = 0.5 * (trace - disc);
    return std::max(std::abs(lambda0), std::abs(lambda1));
}

SymmetricAnalysis analyze_symmetric(const Mat2& matrix) {
    const double a = matrix.xx;
    const double b = 0.5 * (matrix.xy + matrix.yx);
    const double d = matrix.yy;
    const double trace = a + d;
    const double disc = std::sqrt(std::max(0.0, (a - d) * (a - d) + 4.0 * b * b));

    SymmetricAnalysis result;
    result.lambda_major = 0.5 * (trace + disc);
    result.lambda_minor = 0.5 * (trace - disc);
    result.theta_major = 0.5 * std::atan2(2.0 * b, a - d);
    return result;
}

Vec2 axis_from_theta(double theta) {
    return {std::cos(theta), std::sin(theta)};
}

Vec2 project(Vec2 value, Vec2 axis) {
    const double denom = axis.length_sq();
    if (denom <= kEpsilon) {
        return {};
    }
    return axis * (value.dot(axis) / denom);
}

double normalized_ratio(double numerator, double denominator_anchor) {
    return numerator / (numerator + denominator_anchor + kEpsilon);
}

double signed_ratio(double value, double scale) {
    return value / (scale + std::abs(value) + kEpsilon);
}

double scalar_angular_momentum(Vec2 q, Vec2 v) {
    return q.x * v.y - q.y * v.x;
}

Vec2 apply_j(Vec2 value) {
    return {value.y, -value.x};
}

bool finite(Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool finite(const LawState& state) {
    return finite(state.q) && finite(state.v) && std::isfinite(state.p);
}

double max_stage_acceleration(const LawState& k1,
                              const LawState& k2,
                              const LawState& k3,
                              const LawState& k4) {
    return std::max(std::max(k1.v.length(), k2.v.length()),
                    std::max(k3.v.length(), k4.v.length()));
}

} // namespace

LawSpec::LawSpec(const MetaSpec& meta_spec)
    : meta_spec_(meta_spec),
      seeded_p_(meta_spec.p) {
    const Mat2 metric = load(meta_spec_.g);
    const double det = determinant(metric);
    const double safe_det =
        (std::abs(det) < kEpsilon) ? (det >= 0.0 ? kEpsilon : -kEpsilon) : det;
    store({
        metric.yy / safe_det,
        -metric.xy / safe_det,
        -metric.yx / safe_det,
        metric.xx / safe_det
    }, metric_inverse_);

    const Mat2 potential = load(meta_spec_.V);
    const Mat2 symmetry = load(meta_spec_.S);
    const Mat2 bounded_potential = add(potential, scale(symmetry, meta_spec_.s_a));
    store(bounded_potential, potential_matrix_);

    const double raw_potential_norm = spectral_norm_symmetric(bounded_potential);
    potential_linear_gain_ = (raw_potential_norm <= kEpsilon)
        ? 1.0
        : std::min(1.0, kPotentialNormBound / raw_potential_norm);
    bounded_potential_spectral_norm_ = raw_potential_norm * potential_linear_gain_;

    const double coupling_norm =
        spectral_norm_symmetric(load(meta_spec_.C[0]))
        + spectral_norm_symmetric(load(meta_spec_.C[1]));
    const double gyro_norm =
        frob(load(meta_spec_.T))
        + frob(load(meta_spec_.G[0]))
        + frob(load(meta_spec_.G[1]));
    const double warp_norm = frob(load(meta_spec_.W));
    acceleration_ceiling_ = std::clamp(
        8.0 + 6.0 * bounded_potential_spectral_norm_
            + 3.0 * coupling_norm
            + 1.5 * gyro_norm
            + 2.0 * warp_norm,
        12.0,
        40.0);
}

LawState LawSpec::initial_state() const {
    return clamp_state({
        {meta_spec_.q0[0], meta_spec_.q0[1]},
        {meta_spec_.qdot0[0], meta_spec_.qdot0[1]},
        seeded_p_
    });
}

LawState LawSpec::clamp_state(LawState state) const {
    if (!meta_spec_.p_dynamic) {
        state.p = seeded_p_;
        return state;
    }

    state.p = std::clamp(state.p, seeded_p_ - 0.5, seeded_p_ + 0.5);
    return state;
}

LawState LawSpec::derivative(const LawState& state) const {
    const LawState current = clamp_state(state);
    const Mat2 metric = load(meta_spec_.g);
    const Mat2 metric_inverse = load(metric_inverse_);
    const Mat2 potential = load(potential_matrix_);
    const Mat2 symmetry = load(meta_spec_.S);
    const Mat2 coupling0 = load(meta_spec_.C[0]);
    const Mat2 coupling1 = load(meta_spec_.C[1]);
    const Mat2 time_skew = load(meta_spec_.T);
    const Mat2 gyro0 = load(meta_spec_.G[0]);
    const Mat2 gyro1 = load(meta_spec_.G[1]);
    const Mat2 warp = load(meta_spec_.W);

    const Vec2 gq = mul(metric, current.q);
    const Vec2 gv = mul(metric, current.v);
    const double q_metric = std::sqrt(std::max(0.0, current.q.dot(gq)));
    const double v_metric = std::sqrt(std::max(0.0, current.v.dot(gv)));
    const double q_ratio = normalized_ratio(q_metric, 1.0);
    const double nonlinear_base = 0.75 + 0.5 * q_ratio;
    const double potential_weight = std::pow(nonlinear_base, current.p);
    const double position_weight = std::pow(nonlinear_base, std::abs(current.p));
    const double velocity_weight = normalized_ratio(v_metric, 0.5 + q_metric);
    const double angular_momentum = scalar_angular_momentum(current.q, current.v);
    const double angular_gate =
        signed_ratio(angular_momentum, q_metric * v_metric + 1.0);

    Vec2 force{};
    force -= mul(potential, current.q) * (potential_linear_gain_ * potential_weight);
    force += mul(coupling0, current.q) * position_weight;
    force += mul(coupling1, current.v) * velocity_weight;
    force += mul(time_skew, current.v);
    force += mul(gyro0, current.v) * position_weight;
    force += mul(gyro1, current.v) * velocity_weight;
    force += mul(warp, current.q) * angular_gate;

    const Vec2 symmetry_response = mul(symmetry, current.q);
    force += apply_j(symmetry_response) * meta_spec_.s_c;

    if (meta_spec_.s_b > 0.0) {
        Vec2 symmetry_axis = symmetry_response;
        if (symmetry_axis.length_sq() <= kEpsilon) {
            symmetry_axis = axis_from_theta(analyze_symmetric(symmetry).theta_major);
        }
        const Vec2 symmetry_preserving = project(force, symmetry_axis);
        const Vec2 symmetry_breaking = force - symmetry_preserving;
        force -= symmetry_breaking * meta_spec_.s_b;
    }

    const Vec2 accel = mul(metric_inverse, force);
    double p_dot = 0.0;
    if (meta_spec_.p_dynamic) {
        const double relax = 0.45 + 0.75 * meta_spec_.p_beta;
        p_dot = meta_spec_.p_beta * angular_gate - relax * (current.p - seeded_p_);
    }

    return {current.v, accel, p_dot};
}

Vec2 LawSpec::acceleration(const LawState& state) const {
    return derivative(state).v;
}

LawState LawSpec::step_recursive(const LawState& state, double dt, int depth) const {
    if (dt == 0.0) {
        return clamp_state(state);
    }

    const double half_dt = 0.5 * dt;
    const LawState k1 = derivative(state);
    const LawState k2 = derivative(state + k1 * half_dt);
    const LawState k3 = derivative(state + k2 * half_dt);
    const LawState k4 = derivative(state + k3 * dt);

    const double stage_accel = max_stage_acceleration(k1, k2, k3, k4);
    if (stage_accel > acceleration_ceiling_
        && std::abs(dt) > min_substep_
        && depth < max_halvings_) {
        const LawState midpoint = step_recursive(state, half_dt, depth + 1);
        return step_recursive(midpoint, half_dt, depth + 1);
    }

    const LawState advanced = clamp_state(
        state + (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (dt / 6.0));
    if (!finite(advanced)
        && std::abs(dt) > min_substep_
        && depth < max_halvings_) {
        const LawState midpoint = step_recursive(state, half_dt, depth + 1);
        return step_recursive(midpoint, half_dt, depth + 1);
    }

    return advanced;
}

LawState LawSpec::step(const LawState& state, double dt) const {
    return step_recursive(clamp_state(state), dt, 0);
}

void LawSpec::step_in_place(LawState& state, double dt) const {
    state = step(state, dt);
}
