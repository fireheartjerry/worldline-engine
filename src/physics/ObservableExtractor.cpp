#include "physics/ObservableExtractor.hpp"

#include "app/AppFields.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kEpsilon = 1.0e-9;
constexpr double kThetaLimit = 0.96 * 3.14159265358979323846;
constexpr double kOmegaLimit = 0.72 * APP_MAX_OMEGA_DEG * APP_DEG_TO_RAD;

struct SymmetricAnalysis {
    double lambda_major = 0.0;
    double lambda_minor = 0.0;
    double theta_major = 0.0;
    double anisotropy = 0.0;
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

double smoothstep(double x) {
    const double t = clamp01(x);
    return t * t * (3.0 - 2.0 * t);
}

double center_envelope(double x, double pinch = 0.72) {
    return 0.5 + (smoothstep(x) - 0.5) * pinch;
}

double map_full(double u, double low, double high) {
    return lerp(low, high, smoothstep(u));
}

double map_shaped(double u, double low, double high, double pinch = 0.72) {
    return lerp(low, high, center_envelope(u, pinch));
}

double normalized_ratio(double value, double anchor) {
    return value / (value + anchor + kEpsilon);
}

double signed_ratio(double value, double scale) {
    return value / (scale + std::abs(value) + kEpsilon);
}

double spectral_anisotropy(double lambda0, double lambda1) {
    return std::abs(lambda0 - lambda1)
        / (std::abs(lambda0) + std::abs(lambda1) + kEpsilon);
}

SymmetricAnalysis analyze_symmetric(const double matrix[2][2]) {
    const double a = matrix[0][0];
    const double b = 0.5 * (matrix[0][1] + matrix[1][0]);
    const double d = matrix[1][1];
    const double trace = a + d;
    const double disc = std::sqrt(std::max(0.0, (a - d) * (a - d) + 4.0 * b * b));

    SymmetricAnalysis result;
    result.lambda_major = 0.5 * (trace + disc);
    result.lambda_minor = 0.5 * (trace - disc);
    result.theta_major = 0.5 * std::atan2(2.0 * b, a - d);
    result.anisotropy = spectral_anisotropy(result.lambda_major, result.lambda_minor);
    return result;
}

double frob(const double matrix[2][2]) {
    return std::sqrt(
        matrix[0][0] * matrix[0][0] +
        matrix[0][1] * matrix[0][1] +
        matrix[1][0] * matrix[1][0] +
        matrix[1][1] * matrix[1][1]);
}

Vec2 axis_from_theta(double theta) {
    return {std::cos(theta), std::sin(theta)};
}

Vec2 orthogonal(Vec2 value) {
    return {-value.y, value.x};
}

double dot(Vec2 lhs, Vec2 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

double shaped_signed(double value, double scale, double pinch) {
    const double ratio = signed_ratio(value, scale);
    const double unit = 0.5 + 0.5 * ratio;
    return 2.0 * center_envelope(unit, pinch) - 1.0;
}

double major_magnitude(const SymmetricAnalysis& analysis) {
    return std::max(std::abs(analysis.lambda_major), std::abs(analysis.lambda_minor));
}

double minor_magnitude(const SymmetricAnalysis& analysis) {
    return std::min(std::abs(analysis.lambda_major), std::abs(analysis.lambda_minor));
}

} // namespace

ObservableExtractor::ObservableExtractor(const LawSpec& law_spec)
    : law_spec_(law_spec) {
    const MetaSpec& meta = law_spec_.meta_spec();
    const SymmetricAnalysis g = analyze_symmetric(meta.g);
    const SymmetricAnalysis v = analyze_symmetric(meta.V);
    const SymmetricAnalysis w = analyze_symmetric(meta.W);

    g_major_axis_ = axis_from_theta(g.theta_major);
    g_minor_axis_ = orthogonal(g_major_axis_);

    const Vec2 v_major_axis = axis_from_theta(v.theta_major);
    const Vec2 v_minor_axis = orthogonal(v_major_axis);
    const bool soft_is_major = std::abs(v.lambda_major) <= std::abs(v.lambda_minor);
    v_soft_axis_ = soft_is_major ? v_major_axis : v_minor_axis;
    v_hard_axis_ = soft_is_major ? v_minor_axis : v_major_axis;

    const double g_trace = g.lambda_major + g.lambda_minor;
    const double g_major_share = g.lambda_major / (g_trace + kEpsilon);
    const double g_minor_share = g.lambda_minor / (g_trace + kEpsilon);
    const double g_strength = normalized_ratio(g_trace, 1.8);

    draft_.l1 = map_full(
        clamp01(0.58 * g_strength + 0.42 * g_major_share),
        APP_MIN_LENGTH,
        APP_MAX_LENGTH);
    const double l2_candidate = map_full(
        clamp01(0.52 * g_strength + 0.48 * g_minor_share),
        APP_MIN_LENGTH,
        APP_MAX_LENGTH);
    draft_.l2 = std::min(l2_candidate, lerp(APP_MIN_LENGTH, draft_.l1, 0.92));

    draft_.m1 = map_full(
        clamp01(0.46 * g_strength + 0.34 * g_major_share + 0.20 * g.anisotropy),
        APP_MIN_MASS,
        APP_MAX_MASS);
    const double m2_candidate = map_full(
        clamp01(0.44 * g_strength + 0.36 * g_minor_share + 0.20 * (1.0 - g.anisotropy)),
        APP_MIN_MASS,
        APP_MAX_MASS);
    draft_.m2 = std::min(m2_candidate, lerp(APP_MIN_MASS, draft_.m1, 0.90));

    const double v_major_mag = major_magnitude(v);
    const double v_minor_mag = minor_magnitude(v);
    const double v_total_mag = v_major_mag + v_minor_mag;
    const double v_strength = normalized_ratio(v_total_mag, 1.0);
    const double v_hard_share = v_major_mag / (v_total_mag + kEpsilon);
    const double v_soft_share = v_minor_mag / (v_total_mag + kEpsilon);

    draft_.connector1_mass = map_full(
        clamp01(0.62 * v_strength + 0.38 * v_hard_share),
        APP_MIN_CONNECTOR_MASS,
        APP_MAX_CONNECTOR_MASS);
    const double connector2_candidate = map_full(
        clamp01(0.54 * v_strength + 0.46 * v_soft_share),
        APP_MIN_CONNECTOR_MASS,
        APP_MAX_CONNECTOR_MASS);
    draft_.connector2_mass = std::min(
        connector2_candidate,
        lerp(APP_MIN_CONNECTOR_MASS, draft_.connector1_mass, 0.88));

    const double tau_strength = std::abs(meta.T[0][1]);
    const double gamma_strength = std::abs(meta.G[0][0][1]) + std::abs(meta.G[1][0][1]);
    const double tau_u = normalized_ratio(tau_strength, 0.18);
    const double gamma_u = normalized_ratio(gamma_strength, 0.22);
    const double disagreement = std::abs(tau_strength - gamma_strength)
        / (tau_strength + gamma_strength + kEpsilon);
    const double pivot_base = clamp01(0.68 * tau_u + 0.32 * gamma_u);
    const double elbow_base = clamp01(0.34 * tau_u + 0.66 * gamma_u);

    draft_.pivot_viscous = map_shaped(pivot_base, APP_MIN_JOINT_VISCOUS, APP_MAX_JOINT_VISCOUS, 0.66);
    draft_.pivot_quadratic = map_shaped(
        clamp01(0.72 * pivot_base * (0.60 + 0.40 * tau_u)),
        APP_MIN_JOINT_QUADRATIC,
        APP_MAX_JOINT_QUADRATIC,
        0.66);
    draft_.pivot_coulomb = map_shaped(
        clamp01(0.35 * pivot_base + 0.65 * disagreement),
        APP_MIN_JOINT_COULOMB,
        APP_MAX_JOINT_COULOMB,
        0.66);

    draft_.elbow_viscous = std::min(
        map_shaped(elbow_base, APP_MIN_JOINT_VISCOUS, APP_MAX_JOINT_VISCOUS, 0.66),
        lerp(APP_MIN_JOINT_VISCOUS, draft_.pivot_viscous, 0.94));
    draft_.elbow_quadratic = std::min(
        map_shaped(clamp01(0.72 * elbow_base * (0.55 + 0.45 * gamma_u)),
                   APP_MIN_JOINT_QUADRATIC,
                   APP_MAX_JOINT_QUADRATIC,
                   0.66),
        lerp(APP_MIN_JOINT_QUADRATIC, draft_.pivot_quadratic, 0.94));
    draft_.elbow_coulomb = std::min(
        map_shaped(clamp01(0.30 * elbow_base + 0.70 * disagreement),
                   APP_MIN_JOINT_COULOMB,
                   APP_MAX_JOINT_COULOMB,
                   0.66),
        lerp(APP_MIN_JOINT_COULOMB, draft_.pivot_coulomb, 0.94));

    const double w_strength = normalized_ratio(frob(meta.W), 0.22);
    const double normal_bias = clamp01(0.56 + 0.34 * w.anisotropy);
    const double axial_bias = clamp01(0.26 + 0.18 * (1.0 - w.anisotropy));
    const double connector_base = center_envelope(clamp01(0.72 * w_strength + 0.28 * w.anisotropy), 0.64);
    const double bob_base = center_envelope(clamp01(0.52 * w_strength + 0.18 * w.anisotropy), 0.64);

    draft_.connector1_normal_linear_drag = map_shaped(
        clamp01(connector_base * normal_bias),
        APP_MIN_LINEAR_DRAG,
        APP_MAX_LINEAR_DRAG,
        0.62);
    draft_.connector1_normal_quadratic_drag = map_shaped(
        clamp01(0.72 * connector_base * normal_bias),
        APP_MIN_QUADRATIC_DRAG,
        APP_MAX_QUADRATIC_DRAG,
        0.62);
    draft_.connector1_axial_linear_drag = map_shaped(
        clamp01(0.55 * connector_base * axial_bias),
        APP_MIN_LINEAR_DRAG,
        APP_MAX_LINEAR_DRAG,
        0.62);
    draft_.connector1_axial_quadratic_drag = map_shaped(
        clamp01(0.42 * connector_base * axial_bias),
        APP_MIN_QUADRATIC_DRAG,
        APP_MAX_QUADRATIC_DRAG,
        0.62);

    draft_.connector2_normal_linear_drag = std::min(
        map_shaped(clamp01(0.84 * connector_base * normal_bias),
                   APP_MIN_LINEAR_DRAG,
                   APP_MAX_LINEAR_DRAG,
                   0.62),
        lerp(APP_MIN_LINEAR_DRAG, draft_.connector1_normal_linear_drag, 0.92));
    draft_.connector2_normal_quadratic_drag = std::min(
        map_shaped(clamp01(0.62 * connector_base * normal_bias),
                   APP_MIN_QUADRATIC_DRAG,
                   APP_MAX_QUADRATIC_DRAG,
                   0.62),
        lerp(APP_MIN_QUADRATIC_DRAG, draft_.connector1_normal_quadratic_drag, 0.92));
    draft_.connector2_axial_linear_drag = std::min(
        map_shaped(clamp01(0.46 * connector_base * axial_bias),
                   APP_MIN_LINEAR_DRAG,
                   APP_MAX_LINEAR_DRAG,
                   0.62),
        lerp(APP_MIN_LINEAR_DRAG, draft_.connector1_axial_linear_drag, 0.92));
    draft_.connector2_axial_quadratic_drag = std::min(
        map_shaped(clamp01(0.34 * connector_base * axial_bias),
                   APP_MIN_QUADRATIC_DRAG,
                   APP_MAX_QUADRATIC_DRAG,
                   0.62),
        lerp(APP_MIN_QUADRATIC_DRAG, draft_.connector1_axial_quadratic_drag, 0.92));

    draft_.bob1_linear_drag = map_shaped(
        clamp01(0.68 * bob_base),
        APP_MIN_LINEAR_DRAG,
        APP_MAX_LINEAR_DRAG,
        0.62);
    draft_.bob1_quadratic_drag = map_shaped(
        clamp01(0.50 * bob_base),
        APP_MIN_QUADRATIC_DRAG,
        APP_MAX_QUADRATIC_DRAG,
        0.62);
    draft_.bob2_linear_drag = std::min(
        map_shaped(clamp01(0.52 * bob_base),
                   APP_MIN_LINEAR_DRAG,
                   APP_MAX_LINEAR_DRAG,
                   0.62),
        lerp(APP_MIN_LINEAR_DRAG, draft_.bob1_linear_drag, 0.90));
    draft_.bob2_quadratic_drag = std::min(
        map_shaped(clamp01(0.38 * bob_base),
                   APP_MIN_QUADRATIC_DRAG,
                   APP_MAX_QUADRATIC_DRAG,
                   0.62),
        lerp(APP_MIN_QUADRATIC_DRAG, draft_.bob1_quadratic_drag, 0.90));

    draft_.rigid_connectors = true;
    draft_.connector_mass_enabled =
        draft_.connector1_mass > 1.0e-3 || draft_.connector2_mass > 1.0e-3;

    const Vec2 q0 = {meta.q0[0], meta.q0[1]};
    const Vec2 v0 = {meta.qdot0[0], meta.qdot0[1]};
    q_scale_major_ = std::max(0.22, std::abs(dot(q0, g_major_axis_)) + 0.03 * draft_.l1 + meta.drift_amp);
    q_scale_minor_ = std::max(0.22, std::abs(dot(q0, g_minor_axis_)) + 0.03 * draft_.l2 + meta.drift_amp);
    v_scale_major_ = std::max(0.30, std::abs(dot(v0, g_major_axis_)) + 0.06 * draft_.l1);
    v_scale_minor_ = std::max(0.30, std::abs(dot(v0, g_minor_axis_)) + 0.06 * draft_.l2);
}

PendulumState ObservableExtractor::observe(const LawState& state, double dt) {
    const MetaSpec& meta = law_spec_.meta_spec();
    if (meta.time_varying && dt != 0.0) {
        drift_phase_ += dt * meta.drift_omega;
    }

    Vec2 q_effective = state.q;
    if (meta.time_varying && meta.drift_amp > 0.0) {
        q_effective += v_soft_axis_ * (meta.drift_amp * std::cos(drift_phase_));
        q_effective += v_hard_axis_ * (0.58 * meta.drift_amp * std::sin(drift_phase_));
    }

    const double q_major = dot(q_effective, g_major_axis_);
    const double q_minor = dot(q_effective, g_minor_axis_);
    const double v_major = dot(state.v, g_major_axis_);
    const double v_minor = dot(state.v, g_minor_axis_);

    // Adaptive envelope: track the running maximum of each projected component.
    // Only update during actual simulation steps (dt > 0) so that static queries
    // and artificial drift-phase advances do not alter the mapping.
    // Monotonically expanding — scale grows to cover new peaks and never shrinks,
    // so the display always uses its full angular range without jumping or saturating.
    if (dt > 0.0) {
        q_scale_major_ = std::max(q_scale_major_, std::max(std::abs(q_major), 0.22));
        q_scale_minor_ = std::max(q_scale_minor_, std::max(std::abs(q_minor), 0.22));
        v_scale_major_ = std::max(v_scale_major_, std::max(std::abs(v_major), 0.30));
        v_scale_minor_ = std::max(v_scale_minor_, std::max(std::abs(v_minor), 0.30));
    }

    PendulumState observed;
    observed.theta1 = kThetaLimit * shaped_signed(q_major, q_scale_major_, 0.96);
    observed.theta2 = kThetaLimit * shaped_signed(q_minor, q_scale_minor_, 0.96);
    observed.omega1 = kOmegaLimit * shaped_signed(v_major, v_scale_major_, 0.65);
    observed.omega2 = kOmegaLimit * shaped_signed(v_minor, v_scale_minor_, 0.65);
    return observed;
}
