#include "seed/MetaSpec.hpp"

#include "seed/Seeder.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kNormEpsilon = 1.0e-9;

struct Vec2d {
    double x = 0.0;
    double y = 0.0;
};

struct Mat2 {
    double xx = 0.0;
    double xy = 0.0;
    double yx = 0.0;
    double yy = 0.0;
};

struct Spectral2 {
    double lambda0 = 0.0;
    double lambda1 = 0.0;
    double theta = 0.0;
    Mat2 matrix{};
};

struct SymmetricAnalysis {
    double lambda_major = 0.0;
    double lambda_minor = 0.0;
    double theta_major = 0.0;
    double anisotropy = 0.0;
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double lerp(double a, double b, double u) {
    return a + (b - a) * u;
}

double smoothstep(double x) {
    const double t = clamp01(x);
    return t * t * (3.0 - 2.0 * t);
}

Mat2 multiply(const Mat2& lhs, const Mat2& rhs) {
    return {
        lhs.xx * rhs.xx + lhs.xy * rhs.yx,
        lhs.xx * rhs.xy + lhs.xy * rhs.yy,
        lhs.yx * rhs.xx + lhs.yy * rhs.yx,
        lhs.yx * rhs.xy + lhs.yy * rhs.yy
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

double frob(const Mat2& matrix) {
    return std::sqrt(
        matrix.xx * matrix.xx +
        matrix.xy * matrix.xy +
        matrix.yx * matrix.yx +
        matrix.yy * matrix.yy);
}

double comm_scalar(const Mat2& lhs, const Mat2& rhs) {
    const Mat2 ab = multiply(lhs, rhs);
    const Mat2 ba = multiply(rhs, lhs);
    return ab.xy - ba.xy;
}

Vec2d add(Vec2d lhs, Vec2d rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

Vec2d scale(Vec2d value, double factor) {
    return {value.x * factor, value.y * factor};
}

double dot(Vec2d lhs, Vec2d rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

double length(Vec2d value) {
    return std::sqrt(dot(value, value));
}

Vec2d mul(const Mat2& matrix, Vec2d value) {
    return {
        matrix.xx * value.x + matrix.xy * value.y,
        matrix.yx * value.x + matrix.yy * value.y
    };
}

Vec2d axis_from_theta(double theta) {
    return {std::cos(theta), std::sin(theta)};
}

Vec2d orthogonal(Vec2d value) {
    return {-value.y, value.x};
}

Mat2 make_spectral_matrix(double lambda0, double lambda1, double theta) {
    const double c = std::cos(theta);
    const double s = std::sin(theta);
    const double cc = c * c;
    const double ss = s * s;
    const double cs = c * s;
    return {
        cc * lambda0 + ss * lambda1,
        cs * (lambda0 - lambda1),
        cs * (lambda0 - lambda1),
        ss * lambda0 + cc * lambda1
    };
}

Spectral2 make_spectral(double u0,
                        double u1,
                        double u2,
                        double min_eigenvalue,
                        double max_eigenvalue) {
    Spectral2 spectral;
    spectral.lambda0 = lerp(min_eigenvalue, max_eigenvalue, u0);
    spectral.lambda1 = lerp(min_eigenvalue, max_eigenvalue, u1);
    spectral.theta = kPi * (u2 - 0.5);
    spectral.matrix = make_spectral_matrix(
        spectral.lambda0,
        spectral.lambda1,
        spectral.theta);
    return spectral;
}

Spectral2 make_spectral_from_values(double lambda0,
                                    double lambda1,
                                    double theta) {
    Spectral2 spectral;
    spectral.lambda0 = lambda0;
    spectral.lambda1 = lambda1;
    spectral.theta = theta;
    spectral.matrix = make_spectral_matrix(lambda0, lambda1, theta);
    return spectral;
}

double spectral_anisotropy(double lambda0, double lambda1) {
    return std::abs(lambda0 - lambda1)
        / (std::abs(lambda0) + std::abs(lambda1) + kNormEpsilon);
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

void store_matrix(const Mat2& matrix, double target[2][2]) {
    target[0][0] = matrix.xx;
    target[0][1] = matrix.xy;
    target[1][0] = matrix.yx;
    target[1][1] = matrix.yy;
}

Mat2 load_matrix(const double source[2][2]) {
    return {
        source[0][0],
        source[0][1],
        source[1][0],
        source[1][1]
    };
}

double round_for_hash(double value) {
    return std::llround(value * 1000.0);
}

std::uint64_t mix_hash(std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30u)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27u)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31u);
}

std::uint64_t descriptor_hash(const MetaSpec& ms) {
    const SymmetricAnalysis g = analyze_symmetric(ms.g);
    const SymmetricAnalysis v = analyze_symmetric(ms.V);
    const double symmetry = frob(load_matrix(ms.S));
    const double tau = std::abs(ms.T[0][1]);
    const double gamma = std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]);
    const double coupling = frob(load_matrix(ms.C[0])) + frob(load_matrix(ms.C[1]));
    const double contamination = frob(load_matrix(ms.W));
    const double q_norm = std::sqrt(ms.q0[0] * ms.q0[0] + ms.q0[1] * ms.q0[1]);
    const double v_norm = std::sqrt(ms.qdot0[0] * ms.qdot0[0] + ms.qdot0[1] * ms.qdot0[1]);

    std::uint64_t hash = 0xC4CEB9FE1A85EC53ULL;
    const double invariants[] = {
        g.lambda_major,
        g.lambda_minor,
        g.anisotropy,
        v.lambda_major,
        v.lambda_minor,
        v.anisotropy,
        ms.p,
        symmetry,
        tau,
        gamma,
        coupling,
        contamination,
        q_norm,
        v_norm
    };

    for (double invariant : invariants) {
        const std::uint64_t bits =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(round_for_hash(invariant)));
        hash = mix_hash(hash ^ bits);
    }
    return hash;
}

std::size_t pick_index(std::uint64_t hash,
                       std::uint64_t salt,
                       std::size_t size) {
    return static_cast<std::size_t>(mix_hash(hash + salt) % size);
}

template<std::size_t N>
const char* pick_phrase(const char* const (&phrases)[N],
                        std::uint64_t hash,
                        std::uint64_t salt) {
    return phrases[pick_index(hash, salt, N)];
}

std::string format_number(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

double saturate(double value, double scale_value) {
    return value / (scale_value + std::abs(value) + kNormEpsilon);
}

std::uint64_t lane_hash(const std::vector<double>& lanes) {
    std::uint64_t hash = 0xA0761D6478BD642FULL;
    for (double lane : lanes) {
        const std::uint64_t bits =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(round_for_hash(lane)));
        hash = mix_hash(hash ^ bits);
    }
    return hash;
}

struct DescriptorFeatureSet {
    SymmetricAnalysis g{};
    SymmetricAnalysis v{};
    double metric_strength = 0.0;
    double potential_strength = 0.0;
    double symmetry_strength = 0.0;
    double tau_strength = 0.0;
    double gyro_strength = 0.0;
    double coupling0_strength = 0.0;
    double coupling1_strength = 0.0;
    double coupling_strength = 0.0;
    double coupling_imbalance = 0.0;
    double contamination_strength = 0.0;
    double q_radius = 0.0;
    double launch_speed = 0.0;
    double launch_bias = 0.0;
    double velocity_bias = 0.0;
    double misalignment = 0.0;
    double confinement = 0.0;
    double instability = 0.0;
    double directional_bias = 0.0;
    double mode_mixing = 0.0;
    double steering_strength = 0.0;
    double transfer_immediacy = 0.0;
    double launch_intensity = 0.0;
    double structural_order = 0.0;
    double temporal_bias = 0.0;
    double unusualness = 0.0;
    double severity = 0.0;
    double rare_regime_score = 0.0;
    double extreme_factor = 0.0;
};

DescriptorFeatureSet analyze_descriptor_features(const MetaSpec& ms) {
    DescriptorFeatureSet f;
    f.g = analyze_symmetric(ms.g);
    f.v = analyze_symmetric(ms.V);
    f.metric_strength = frob(load_matrix(ms.g));
    f.potential_strength = frob(load_matrix(ms.V));
    f.symmetry_strength = frob(load_matrix(ms.S));
    f.tau_strength = std::abs(ms.T[0][1]);
    f.gyro_strength = std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]);
    f.coupling0_strength = frob(load_matrix(ms.C[0]));
    f.coupling1_strength = frob(load_matrix(ms.C[1]));
    f.coupling_strength = f.coupling0_strength + f.coupling1_strength;
    f.coupling_imbalance = std::abs(f.coupling0_strength - f.coupling1_strength)
        / (f.coupling_strength + kNormEpsilon);
    f.contamination_strength = frob(load_matrix(ms.W));

    const Vec2d v_major_axis = axis_from_theta(f.v.theta_major);
    const Vec2d v_minor_axis = orthogonal(v_major_axis);
    const bool soft_is_major = std::abs(f.v.lambda_major) <= std::abs(f.v.lambda_minor);
    const Vec2d soft_axis = soft_is_major ? v_major_axis : v_minor_axis;
    const Vec2d hard_axis = soft_is_major ? v_minor_axis : v_major_axis;
    const Vec2d q0 = {ms.q0[0], ms.q0[1]};
    const Vec2d qdot0 = {ms.qdot0[0], ms.qdot0[1]};
    const double soft_projection = std::abs(dot(q0, soft_axis));
    const double hard_projection = std::abs(dot(q0, hard_axis));
    const double vel_soft = std::abs(dot(qdot0, soft_axis));
    const double vel_hard = std::abs(dot(qdot0, hard_axis));
    f.q_radius = length(q0);
    f.launch_speed = length(qdot0);
    f.launch_bias = hard_projection / (soft_projection + kNormEpsilon);
    f.velocity_bias = vel_hard / (vel_soft + kNormEpsilon);

    f.misalignment = std::abs(std::sin(2.0 * (f.g.theta_major - f.v.theta_major)));
    f.confinement = clamp01(saturate(std::max(0.0, f.v.lambda_major) + std::max(0.0, f.v.lambda_minor), 1.2));
    f.instability = clamp01(saturate(std::max(0.0, -f.v.lambda_major) + std::max(0.0, -f.v.lambda_minor), 1.2));
    f.structural_order = clamp01(0.55 * (1.0 - f.misalignment) + 0.45 * (1.0 - std::min(1.0, f.contamination_strength / 0.45)));
    f.mode_mixing = clamp01(0.55 * std::min(1.0, f.contamination_strength / 0.24) + 0.45 * f.misalignment);
    f.steering_strength = clamp01(std::min(1.0, f.gyro_strength / 0.42));
    f.transfer_immediacy = clamp01(0.60 * std::min(1.0, f.coupling_strength / 1.0) + 0.40 * f.mode_mixing);
    f.launch_intensity = clamp01(0.55 * std::min(1.0, f.launch_speed / 1.20) + 0.45 * std::min(1.0, f.q_radius / 0.64));
    f.directional_bias = clamp01(0.55 * f.g.anisotropy + 0.45 * std::min(1.0, std::abs(std::log(std::max(1.0e-6, f.launch_bias))) / 1.2));
    const double structural_clarity = clamp01(0.45 * (1.0 - f.mode_mixing)
        + 0.35 * std::min(1.0, f.symmetry_strength / 0.75)
        + 0.20 * (1.0 - f.coupling_imbalance));
    f.temporal_bias = clamp01(std::min(1.0, f.tau_strength / 0.30));
    f.unusualness = clamp01(0.32 * f.steering_strength + 0.28 * f.mode_mixing + 0.20 * f.misalignment
        + 0.20 * std::min(1.0, f.tau_strength / 0.30));
    f.severity = clamp01(0.40 * std::max(f.instability, f.temporal_bias) + 0.35 * f.launch_intensity
        + 0.25 * std::min(1.0, f.coupling_strength / 1.1));
    f.rare_regime_score = clamp01(0.34 * f.unusualness + 0.26 * f.severity + 0.20 * f.coupling_imbalance
        + 0.20 * std::min(1.0, f.metric_strength / 2.2));
    return f;
}

std::string geometry_reading(const DescriptorFeatureSet& f) {
    if (f.g.anisotropy < 0.10) {
        return "The metric is close to isotropic, so inertial cost changes very little with direction and the motion should not inherit a strong geometric preference.";
    }
    if (f.g.anisotropy < 0.28) {
        return "The metric is mildly anisotropic, which means one direction is mechanically favored, but not enough to suppress the competing mode outright.";
    }
    if (f.g.anisotropy < 0.50) {
        return "The metric carries a pronounced directional stretch, so one axis has lower effective cost and the orbit geometry should reveal that preference early.";
    }
    return "The metric is strongly anisotropic, so one axis is dynamically privileged and the motion should organize itself around that direction unless other terms actively disrupt it.";
}

std::string potential_reading(const DescriptorFeatureSet& f) {
    const double threshold = 0.18;
    if (std::abs(f.v.lambda_major) < threshold && std::abs(f.v.lambda_minor) < threshold) {
        return "The potential is shallow at linear order, so there is only weak restoring structure and the initial motion is comparatively unconstrained.";
    }
    if (f.v.lambda_major > threshold && f.v.lambda_minor > threshold) {
        return "The potential forms a restoring basin in both principal directions, so displacement tends to be pulled back inward rather than amplified.";
    }
    if (f.v.lambda_major < -threshold && f.v.lambda_minor < -threshold) {
        return "The potential is destabilizing in both principal directions, so the linear tendency is to amplify displacement rather than correct it.";
    }
    return "The potential has a saddle topology: one principal direction is restoring while the other is repelling, so stability depends strongly on orientation.";
}

std::string symmetry_reading(const DescriptorFeatureSet& f) {
    if (f.symmetry_strength < 0.18) {
        return "Only a weak symmetry structure survives, so there is limited conservation pressure to keep the motion balanced.";
    }
    if (f.symmetry_strength < 0.55) {
        return "A partial symmetry structure remains, enough to preserve some order while still allowing asymmetry and transfer effects to show clearly.";
    }
    return "A strong symmetry structure remains active, so even energetic motion should retain a clear organizing pattern rather than dispersing immediately.";
}

std::string time_and_gyro_reading(const DescriptorFeatureSet& f) {
    std::ostringstream out;
    if (f.tau_strength < 0.05) {
        out << "The time-asymmetry term is weak, so forward and backward evolution should remain close to reversible at visible scales. ";
    } else if (f.tau_strength < 0.18) {
        out << "The time-asymmetry term is clearly present, so reversals should not behave like exact mirror images. ";
    } else {
        out << "The time-asymmetry term is strong, so the system should show a pronounced temporal bias rather than approximate reversibility. ";
    }

    if (f.gyro_strength < 0.05) {
        out << "Gyroscopic steering is faint, so curvature and sideways deflection should remain secondary effects.";
    } else if (f.gyro_strength < 0.20) {
        out << "Gyroscopic steering is noticeable, introducing a torsion-like bias that bends trajectories away from purely radial exchange.";
    } else {
        out << "Gyroscopic steering is strong, so lateral deflection and curved transfer paths should become central parts of the observed dynamics.";
    }
    return out.str();
}

std::string mixing_reading(const DescriptorFeatureSet& f) {
    if (f.contamination_strength < 0.03) {
        return "Warp contamination is low, so the dominant modes should remain well separated before noticeable leakage develops.";
    }
    if (f.contamination_strength < 0.10) {
        return "Warp contamination is present but moderate, so the system should open cleanly and then develop cross-mode leakage after the first strong exchange.";
    }
    return "Warp contamination is strong enough to matter immediately, so clean mode separation is likely to break down early through visible cross-talk.";
}

std::string launch_reading(const DescriptorFeatureSet& f) {
    std::ostringstream out;
    if (f.launch_bias < 0.45) {
        out << "The launch vector is biased toward the soft axis, so the opening state is aligned with the mechanically easier channel of the potential. ";
    } else if (f.launch_bias < 0.95) {
        out << "The launch vector sits between the soft and hard axes, so the initial state already excites both principal directions. ";
    } else {
        out << "The launch vector leans into the hard axis, so the initial state is loaded into the more resistant direction of the landscape. ";
    }

    if (f.transfer_immediacy < 0.30) {
        out << "Visible energy transfer should build slowly.";
    } else if (f.transfer_immediacy < 0.62) {
        out << "Visible energy transfer should emerge near the first turnaround.";
    } else {
        out << "Visible energy transfer should appear inside the opening motion.";
    }
    return out.str();
}

std::string universe_character_section(const DescriptorFeatureSet& f,
                                       std::uint64_t hash) {
    static constexpr const char* kOpeners[] = {
        "UNIVERSE CHARACTER\nThis section describes the motion as a viewer is likely to experience it.",
        "UNIVERSE CHARACTER\nThis section summarizes the visible character of the universe before breaking down the underlying tensors.",
        "UNIVERSE CHARACTER\nThis section is a motion-level reading: what the universe is likely to look like before we explain why."
    };

    std::ostringstream out;
    out << pick_phrase(kOpeners, hash, 201u) << " ";
    if (f.structural_order > 0.72 && f.severity > 0.58) {
        out << "The dominant impression should be high structure under significant load: the geometry remains organized, but the energy scale and coupling are strong enough to make the motion look tense rather than gentle. ";
    } else if (f.instability > 0.62) {
        out << "The dominant impression should be instability that is still readable: offsets are likely to be amplified, but the motion should remain interpretable rather than collapsing into noise. ";
    } else if (f.structural_order > 0.64) {
        out << "The dominant impression should be coherent large-scale motion, with the main orbit family staying visible even once secondary effects enter. ";
    } else if (f.unusualness > 0.62) {
        out << "The dominant impression should be a coexistence of several active mechanisms, especially directional bias, gyroscopic steering, and cross-coupling. ";
    } else {
        out << "The dominant impression should be balanced but not featureless, with enough asymmetry to make the motion distinct without obscuring its main structure. ";
    }

    if (f.mode_mixing > 0.68) {
        out << "The opening should show early mode mixing, meaning energy transfer, lateral deflection, and leakage between directions are likely to appear within the same early interval. ";
    } else if (f.structural_order > 0.70) {
        out << "The opening should remain legible, with one dominant mode appearing first and cross-effects arriving later as secondary structure. ";
    } else {
        out << "The opening should balance clarity with buildup: the primary mode should be readable, but secondary effects should accumulate quickly enough to alter the trajectory within the first few swings. ";
    }

    if (f.rare_regime_score > 0.78) {
        out << "This is not a median universe. It sits in a rarer parameter regime where anisotropy, coupling, steering, or leakage are pushed far enough from the center to become unusually pronounced.";
    } else {
        out << "It remains readable, but the important detail should come from the interaction between the main mode, the coupling terms, and the smaller corrections introduced by leakage, steering, or directional bias.";
    }
    return out.str();
}

std::string physical_reading_section(const DescriptorFeatureSet& f) {
    std::ostringstream out;
    out << "PHYSICAL READING\n";
    out << geometry_reading(f) << " ";
    out << potential_reading(f) << "\n\n";
    out << symmetry_reading(f) << " ";
    out << time_and_gyro_reading(f) << "\n\n";
    out << mixing_reading(f) << " ";
    out << launch_reading(f) << "\n\n";

    out << "Taken together, these features suggest ";
    if (f.structural_order > 0.68 && f.unusualness < 0.48) {
        out << "a coherent universe whose nontrivial behavior should emerge in an ordered sequence rather than all at once.";
    } else if (f.structural_order > 0.62 && f.unusualness > 0.58) {
        out << "a coherent but unusual universe, where stable large-scale geometry coexists with noticeable steering, leakage, or timing bias.";
    } else if (f.mode_mixing > 0.65 && f.steering_strength > 0.55) {
        out << "a highly active universe in which cross-coupling, warp contamination, and gyroscopic steering are all dynamically relevant at the same time.";
    } else if (f.instability > 0.62) {
        out << "a severe universe in which instability, directional bias, and timing asymmetry are likely to matter more than calm restoration.";
    } else {
        out << "a layered universe whose visible behavior should emerge from the competition between confinement, transfer, and smaller corrective or destabilizing terms.";
    }
    return out.str();
}

} // namespace

MetaSpec generate_meta_spec(const std::vector<double>& lanes) {
    if (lanes.size() != 32u) {
        throw std::invalid_argument("generate_meta_spec requires exactly 32 lanes");
    }

    const std::uint64_t seed_hash = lane_hash(lanes);
    const double hash_u = static_cast<double>(seed_hash & 0xFFFFFFu) / static_cast<double>(0x1000000u);
    double lane_sum = 0.0;
    double lane_sq_sum = 0.0;
    double lane_contrast = 0.0;
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        lane_sum += lanes[i];
        lane_sq_sum += lanes[i] * lanes[i];
        lane_contrast += std::abs(lanes[i] - lanes[(i + 11) % lanes.size()]);
    }
    const double lane_mean = lane_sum / static_cast<double>(lanes.size());
    const double lane_variance = std::max(0.0, lane_sq_sum / static_cast<double>(lanes.size()) - lane_mean * lane_mean);
    const double structural_signal = clamp01(
        0.55 * std::min(1.0, lane_contrast / 8.0)
        + 0.45 * std::min(1.0, lane_variance / 0.12));
    const double rare_gate = smoothstep((hash_u - 0.965) / 0.035);
    const double extreme_factor = clamp01(rare_gate * (0.90 + 0.45 * structural_signal));

    std::array<double, 32> u{};
    for (int i = 0; i < 32; ++i) {
        double value = 0.0;
        if (i <= 2) {
            value = 0.78 * lanes[i]
                + 0.14 * lanes[(i + 9) % 32]
                + 0.08 * lanes[(i + 21) % 32];
        } else if (i <= 5) {
            value = 0.74 * lanes[i]
                + 0.16 * lanes[(i + 12) % 32]
                + 0.10 * lanes[(i + 25) % 32];
        } else if (i <= 8) {
            value = 0.70 * lanes[i]
                + 0.18 * lanes[(i + 5) % 32]
                + 0.12 * lanes[(i + 17) % 32];
        } else if (i <= 14) {
            value = 0.76 * lanes[i]
                + 0.14 * lanes[(i + 7) % 32]
                + 0.10 * lanes[(i + 19) % 32];
        } else if (i <= 29) {
            value = 0.72 * lanes[i]
                + 0.16 * lanes[(i + 11) % 32]
                + 0.12 * lanes[(i + 23) % 32];
        } else {
            value = 0.82 * lanes[i]
                + 0.12 * lanes[(i + 3) % 32]
                + 0.06 * lanes[(i + 13) % 32];
        }
        u[i] = clamp01(value);
    }

    const double g_min = 0.70 - 0.05 * extreme_factor;
    const double g_max = 1.95 + 0.22 * extreme_factor;
    const double v_span = 1.25 + 0.65 * extreme_factor;
    const Spectral2 g = make_spectral(u[0], u[1], u[2], g_min, g_max);
    const Spectral2 v = make_spectral(u[3], u[4], u[5], -v_span, v_span);

    Spectral2 s;
    const double s_envelope = std::clamp(
        0.10 + 0.90 * smoothstep((u[6] - 0.10) / 0.90),
        0.10,
        1.0);
    const double s_lambda0 = s_envelope * lerp(-0.80, 0.80, u[6]);
    const double s_lambda1 = s_envelope * lerp(-0.80, 0.80, u[7]);
    const double s_theta = kPi * (u[8] - 0.5);
    s = make_spectral_from_values(s_lambda0, s_lambda1, s_theta);

    const double c0_span = 0.65 + 0.40 * extreme_factor;
    const double c1_min = -0.50 - 0.18 * extreme_factor;
    const double c1_max = 0.70 + 0.52 * extreme_factor;
    const Spectral2 c0 = make_spectral(u[9], u[10], u[11], -c0_span, c0_span);
    const Spectral2 c1 = make_spectral(u[12], u[13], u[14], c1_min, c1_max);
    const Spectral2 h5 = make_spectral(u[15], u[16], u[17], -1.0, 1.0);
    const Spectral2 h6 = make_spectral(u[18], u[19], u[20], -1.0, 1.0);
    const Spectral2 h7 = make_spectral(u[21], u[22], u[23], -1.0, 1.0);
    const Spectral2 h8 = make_spectral(u[24], u[25], u[26], -1.0, 1.0);
    const Spectral2 h9 = make_spectral(u[27], u[28], u[29], -1.0, 1.0);

    const Mat2 j = {0.0, 1.0, -1.0, 0.0};

    const double gamma_scale = 0.90 + 0.34 * extreme_factor;
    const double tau_scale = 0.80 + 0.36 * extreme_factor;
    const double gamma0 = gamma_scale * comm_scalar(c0.matrix, h5.matrix)
        / (1.0 + frob(c0.matrix) + frob(h5.matrix));
    const double gamma1 = gamma_scale * comm_scalar(c1.matrix, h6.matrix)
        / (1.0 + frob(c1.matrix) + frob(h6.matrix));
    const double tau = tau_scale * comm_scalar(h7.matrix, h8.matrix)
        / (1.0 + frob(h7.matrix) + frob(h8.matrix));

    const double c_norm = frob(c0.matrix) + frob(c1.matrix);
    const double symmetry_strength = frob(s.matrix);
    const double g_norm = std::abs(gamma0) + std::abs(gamma1);
    const double gyro_ratio = g_norm / (g_norm + c_norm + kNormEpsilon);
    const double contrast =
        0.5 * (
            spectral_anisotropy(g.lambda0, g.lambda1) +
            spectral_anisotropy(v.lambda0, v.lambda1));
    const double misalign = std::abs(std::sin(2.0 * (g.theta - v.theta)));
    const double symmetry_ratio = symmetry_strength / (symmetry_strength + c_norm + 1.0);
    const double coupling_ratio = c_norm / (1.0 + c_norm);
    const double direct_w = 0.24 * smoothstep((u[29] - 0.22) / 0.78);
    const double base_w = 0.025 * smoothstep((u[28] - 0.15) / 0.85);
    const double w_source = base_w
        + 0.08 * direct_w
        + 0.18 * misalign * (0.35 + 0.65 * gyro_ratio)
        + 0.10 * contrast * coupling_ratio
        + 0.06 * symmetry_ratio * misalign
        + extreme_factor * (0.09 + 0.14 * misalign + 0.10 * gyro_ratio + 0.06 * coupling_ratio);
    const double w_scale =
        w_source <= 1.0e-8 ? 0.0 : std::clamp(w_source, 0.015, 0.38 + 0.12 * extreme_factor);
    const Spectral2 w = make_spectral_from_values(
        w_scale * (1.0 + 0.30 * contrast),
        -w_scale * (0.65 - 0.20 * gyro_ratio),
        0.5 * (g.theta + v.theta));

    const double frob_h5 = frob(h5.matrix);
    const double frob_h6 = frob(h6.matrix);
    const double frob_ratio = (frob_h5 - frob_h6)
        / (frob_h5 + frob_h6 + kNormEpsilon);
    const double lane_fold = u[15] - 0.5 * u[20] + 0.25 * u[25] - 0.5;
    const double driver =
        0.30 * (0.5 * (h9.lambda0 + h9.lambda1)
                + 0.35 * (h9.lambda0 - h9.lambda1) * std::cos(2.0 * (h9.theta - g.theta))
                + 0.15 * std::sin(2.0 * (h9.theta - v.theta)))
        + 0.25 * (tau / tau_scale)
        + 0.20 * frob_ratio
        + 0.15 * lane_fold
        + 0.10 * misalign;

    const double phi = kTwoPi * u[30];
    const bool soft_is_first = std::abs(v.lambda0) <= std::abs(v.lambda1);
    const Vec2d e0 = axis_from_theta(v.theta);
    const Vec2d e1 = orthogonal(e0);
    const Vec2d e_soft = soft_is_first ? e0 : e1;
    const Vec2d e_hard = soft_is_first ? e1 : e0;

    const double q_radius = std::clamp(
        (0.35 + (0.40 + 0.12 * extreme_factor) * u[31]) / (1.0 + 0.35 * (frob(v.matrix) + c_norm + g_norm)),
        0.16,
        0.64 + 0.05 * extreme_factor);
    const double speed = std::clamp(
        (0.55 + (0.65 + 0.18 * extreme_factor) * (1.0 - u[31])) * (1.0 + 0.30 * gyro_ratio + (0.20 + 0.12 * extreme_factor) * std::abs(tau)),
        0.42,
        1.20 + 0.10 * extreme_factor);

    const Vec2d q0 = scale(
        add(scale(e_soft, std::cos(phi)),
            scale(e_hard, 0.65 * std::sin(phi))),
        q_radius);

    const Vec2d qdot0 = add(
        add(
            scale(
                add(scale(e_soft, -std::sin(phi)),
                    scale(e_hard, 0.65 * std::cos(phi))),
                speed),
            scale(mul(j, q0), 0.25 * tau)),
        scale(e_hard, 0.15 * (gamma0 + gamma1)));

    MetaSpec meta_spec;
    store_matrix(g.matrix, meta_spec.g);
    store_matrix(v.matrix, meta_spec.V);
    store_matrix(c0.matrix, meta_spec.C[0]);
    store_matrix(c1.matrix, meta_spec.C[1]);
    store_matrix(s.matrix, meta_spec.S);
    store_matrix(scale(j, tau), meta_spec.T);
    store_matrix(scale(j, gamma0), meta_spec.G[0]);
    store_matrix(scale(j, gamma1), meta_spec.G[1]);
    store_matrix(w.matrix, meta_spec.W);
    meta_spec.p = 4.5 * std::tanh(driver);
    meta_spec.p_dynamic = (u[17] + u[22] + u[27]) / 3.0 > 0.58;
    meta_spec.p_beta = 0.15 + 0.25 * u[16];
    meta_spec.q0[0] = q0.x;
    meta_spec.q0[1] = q0.y;
    meta_spec.qdot0[0] = qdot0.x;
    meta_spec.qdot0[1] = qdot0.y;
    return meta_spec;
}

MetaSpec generate_meta_spec(const std::string& seed) {
    return generate_meta_spec(generate(seed));
}

std::string generate_descriptor(const MetaSpec& ms) {
    DescriptorFeatureSet f = analyze_descriptor_features(ms);
    const std::uint64_t hash = descriptor_hash(ms);

    std::ostringstream out;
    out << universe_character_section(f, hash);

    out << "\n\n";
    out << physical_reading_section(f);

    out << "\n\n";
    out << "CONSTRUCTION PATH\n";
    out << "Here is the plain-language version of what happened. Your seed text is first turned into bytes, then spread across a 512-cell tape so that the original characters stop acting like isolated symbols and start acting like a shared pattern. That expanded tape is read by a self-editing machine with 32 internal registers. As it runs, the machine keeps changing some of its own operations, which is why two seeds that look similar as text can still grow into noticeably different outputs. After that process settles, the machine emits 32 normalized lane values between 0 and 1. Those 32 numbers are the compact fingerprint of this universe.";

    out << "\n\n";
    out << "Those 32 lane values are then assigned jobs in groups. Lanes 0 to 2 build the metric tensor g, which decides how distance and direction are measured. Lanes 3 to 5 build the potential tensor V, which shapes the energy landscape. Lanes 6 to 8 control the symmetry tensor S, which can either stay weak or become a real balancing influence depending on how strongly that lane group activates. Lanes 9 to 14 build the two coupling tensors C0 and C1, which determine how strongly one side of the pendulum system can push on the other.";

    out << "\n\n";
    out << "The next block, lanes 15 to 29, is used more indirectly. Those lanes create several helper matrices inside the metaspec builder. The code compares and mixes those helpers to derive the time-skew term T, the two gyroscopic terms G0 and G1, the contamination or warp tensor W, and the scalar power term p. In other words, those middle lanes do not just fill in a matrix directly. They create the secondary structure that tells the simulation how much twist, handedness, cross-coupling, and directional bias should exist in the final physics.";

    out << "\n\n";
    out << "The last two lanes control the launch conditions. Lane 30 sets the angular phase used to choose where the system starts around its preferred axes. Lane 31 helps set both the starting radius and the initial speed, with the final result adjusted by the strength of the potential, coupling, and gyro terms. That is how the same seed influences not only the laws of the universe, but also the exact way motion begins inside those laws.";

    out << "\n\n";
    out << "For this specific universe, the metric strength is about " << format_number(f.metric_strength)
        << ", the potential strength is about " << format_number(f.potential_strength)
        << ", the combined coupling strength is about " << format_number(f.coupling_strength)
        << ", the time-arrow strength is about " << format_number(f.tau_strength)
        << ", and the warp contamination strength is about " << format_number(f.contamination_strength)
        << ". The power exponent p is " << format_number(ms.p)
        << ", the starting position q0 is (" << format_number(ms.q0[0]) << ", " << format_number(ms.q0[1])
        << "), and the starting velocity qdot0 is (" << format_number(ms.qdot0[0]) << ", " << format_number(ms.qdot0[1]) << ").";

    out << "\n\n";
    out << "Looking at those numbers more closely, this universe has a metric anisotropy of about "
        << format_number(f.g.anisotropy)
        << " and a potential anisotropy of about "
        << format_number(f.v.anisotropy)
        << ". In plain terms, that tells you how uneven the rules are across different directions. Lower anisotropy means motion feels more even and rotationally balanced. Higher anisotropy means one direction or mode of motion is favored, so swings and transfers can develop a visible preferred orientation instead of spreading evenly across the system.";

    out << "\n\n";
    out << "The launch state also has a readable structure. The starting radius is about "
        << format_number(f.q_radius)
        << " and the starting speed is about "
        << format_number(f.launch_speed)
        << ". The position is "
        << (f.launch_bias < 0.45 ? "biased toward the softer direction of the potential" :
            (f.launch_bias < 0.95 ? "shared between the soft and hard directions" :
             "biased toward the harder direction of the potential"))
        << ", while the initial velocity is "
        << (f.velocity_bias < 0.45 ? "more aligned with the softer mode" :
            (f.velocity_bias < 0.95 ? "split across both main modes" :
             "more committed to the harder mode"))
        << ". That combination matters because the simulation does not only care about where it starts. It also cares about whether the initial motion agrees with the easiest path offered by the potential, resists it, or cuts across it.";

    out << "\n\n";
    out << "If you read the full metaspec as a layered control system, the metric g defines the shape of the stage, the potential V defines where energy naturally wants to go, the symmetry term S decides how much balanced structure survives, the coupling pair C0 and C1 decide how readily one mode hands energy to the other, the time-skew tensor T biases the order and feel of that exchange, the gyroscopic terms G0 and G1 add sideways turning pressure, and the warp tensor W measures how much these effects stay cleanly separated versus bleeding into mixed behavior. When several of those terms are strong at once, the universe feels richer because motion is being guided by several competing instructions rather than one dominant rule.";

    out << "\n\n";
    out << "That is why the 32 emitted lane values are so important. They are not just thirty-two unrelated sliders. They are organized into a recipe. Early lanes define the main geometry and energy landscape. Middle lanes produce the hidden helper structures that create time direction, rotational bias, and contamination. Final lanes choose how the system enters that world. The resulting universe summary is therefore a readable explanation of how a compact machine fingerprint becomes a full physical setup with distances, forces, couplings, asymmetries, and initial motion.";

    out << "\n\n";
    out << "A useful way to read the tensor vault is this: g tells the system what counts as an easy or costly direction of motion, V tells it where energy wells and ridges live, S adds or removes balance, C0 and C1 control cross-talk between the two degrees of freedom, T and G bend the motion with directional or rotational bias, and W measures how much those structures bleed into one another instead of staying cleanly separated. The result is not random decoration. It is a layered translation from seed text, to machine output, to tensors, to motion.";

    out << "\n\n";
    out << "GLOSSARY\n";
    out << "Anisotropy: Uneven behavior across different directions. If anisotropy is high, one direction or axis matters more than another.\n\n";
    out << "Restoring basin: An energy shape that pulls motion back inward after it is displaced. A deeper basin usually means stronger return toward a central region.\n\n";
    out << "Saddle topology: A mixed energy shape where one principal direction is restoring and another is destabilizing. Motion can be guided in one direction and released in another.\n\n";
    out << "Principal directions: The main axes picked out by the metric or potential. They are the directions along which the system's behavior becomes easiest to describe.\n\n";
    out << "Symmetry structure: The part of the laws that preserves balance or repeated pattern. Stronger symmetry usually means more visible order.\n\n";
    out << "Time asymmetry: A built-in preference for forward evolution over exact reversal. If time asymmetry is strong, running the motion backward would not look like the same story in reverse.\n\n";
    out << "Gyroscopic steering: Sideways deflection caused by rotational or torsion-like terms. Instead of only moving inward and outward, the path begins to curve or twist.\n\n";
    out << "Cross-coupling: The mechanism that lets one mode or direction of motion feed energy into another. Strong cross-coupling usually makes transfer happen sooner.\n\n";
    out << "Mode mixing: Leakage between motion patterns that would otherwise stay more separate. Stronger mixing makes the system look more entangled and less cleanly partitioned.\n\n";
    out << "Warp contamination: The degree to which otherwise distinct structures bleed into each other. In the readout, higher warp contamination usually means earlier cross-talk and less clean separation.\n\n";
    out << "Launch vector: The starting position and direction of motion encoded by q0 and qdot0. It determines how the system enters the landscape before the deeper tensor structure fully reveals itself.\n\n";
    out << "Phase alignment: How the launch is oriented relative to the main directions favored by the potential. Better alignment with a soft direction usually produces a gentler opening; alignment with a hard direction usually produces more immediate tension.";

    return out.str();
}
