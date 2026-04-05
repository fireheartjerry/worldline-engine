#include "seed/MetaSpec.hpp"

#include "seed/Seeder.hpp"

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

const char* geometry_phrase(const SymmetricAnalysis& g,
                            std::uint64_t hash) {
    if (g.anisotropy < 0.12) {
        static constexpr const char* kOptions[] = {
            "almost round",
            "close to isotropic",
            "nearly circular"
        };
        return pick_phrase(kOptions, hash, 1u);
    }
    if (g.anisotropy < 0.35) {
        static constexpr const char* kOptions[] = {
            "gently stretched",
            "mildly anisotropic",
            "tilted into a mild preferred axis"
        };
        return pick_phrase(kOptions, hash, 2u);
    }
    static constexpr const char* kOptions[] = {
        "strongly anisotropic",
        "drawn into a hard preferred axis",
        "severely stretched along one direction"
    };
    return pick_phrase(kOptions, hash, 3u);
}

const char* potential_phrase(const SymmetricAnalysis& v,
                             std::uint64_t hash) {
    const double threshold = 0.18;
    if (std::abs(v.lambda_major) < threshold && std::abs(v.lambda_minor) < threshold) {
        static constexpr const char* kOptions[] = {
            "shallow in both directions",
            "nearly flat at linear order",
            "only weakly shaped at linear order"
        };
        return pick_phrase(kOptions, hash, 4u);
    }
    if (v.lambda_major > threshold && v.lambda_minor > threshold) {
        static constexpr const char* kOptions[] = {
            "bowl-like on both axes",
            "confining in both principal directions",
            "restoring along both principal directions"
        };
        return pick_phrase(kOptions, hash, 5u);
    }
    if (v.lambda_major < -threshold && v.lambda_minor < -threshold) {
        static constexpr const char* kOptions[] = {
            "repelling on both axes",
            "outward-sloping in both principal directions",
            "destabilizing in both principal directions"
        };
        return pick_phrase(kOptions, hash, 6u);
    }
    static constexpr const char* kOptions[] = {
        "saddle-shaped",
        "split between one restoring and one repelling axis",
        "balanced on a linear saddle"
    };
    return pick_phrase(kOptions, hash, 7u);
}

const char* symmetry_phrase(double symmetry_strength,
                            std::uint64_t hash) {
    if (symmetry_strength < 0.15) {
        static constexpr const char* kOptions[] = {
            "only a faint continuous order survives",
            "continuous structure is present but weak",
            "there is only a fragile remnant of symmetry"
        };
        return pick_phrase(kOptions, hash, 8u);
    }
    if (symmetry_strength < 0.55) {
        static constexpr const char* kOptions[] = {
            "a partial conservation structure survives",
            "some continuous order still organizes the motion",
            "the world keeps a partial Noether-like skeleton"
        };
        return pick_phrase(kOptions, hash, 9u);
    }
    static constexpr const char* kOptions[] = {
        "a strong conservation structure cuts through the motion",
        "continuous order remains plainly visible",
        "the dynamics still obey a pronounced organizing symmetry"
    };
    return pick_phrase(kOptions, hash, 10u);
}

const char* time_phrase(double tau_strength,
                        std::uint64_t hash) {
    if (tau_strength < 0.05) {
        static constexpr const char* kOptions[] = {
            "near-reversible",
            "almost reversible",
            "close to reversible"
        };
        return pick_phrase(kOptions, hash, 11u);
    }
    if (tau_strength < 0.18) {
        static constexpr const char* kOptions[] = {
            "directional",
            "noticeably directional",
            "clearly directional"
        };
        return pick_phrase(kOptions, hash, 12u);
    }
    static constexpr const char* kOptions[] = {
        "strongly one-way",
        "strongly one-way at visible scales",
        "strongly one-way even over short runs"
    };
    return pick_phrase(kOptions, hash, 13u);
}

const char* gyroscopic_phrase(double gyro_strength,
                              std::uint64_t hash) {
    if (gyro_strength < 0.05) {
        static constexpr const char* kOptions[] = {
            "barely present",
            "almost absent",
            "too weak to dominate"
        };
        return pick_phrase(kOptions, hash, 14u);
    }
    if (gyro_strength < 0.20) {
        static constexpr const char* kOptions[] = {
            "noticeable",
            "clearly visible",
            "large enough to bend trajectories"
        };
        return pick_phrase(kOptions, hash, 15u);
    }
    static constexpr const char* kOptions[] = {
        "dominant",
        "strong enough to steer motion sideways",
        "a major part of the visible dynamics"
    };
    return pick_phrase(kOptions, hash, 16u);
}

const char* contamination_phrase(double contamination_strength,
                                 std::uint64_t hash) {
    if (contamination_strength < 0.03) {
        static constexpr const char* kOptions[] = {
            "clean modal separation",
            "clean modal separation for a while",
            "clean modal separation before leakage sets in"
        };
        return pick_phrase(kOptions, hash, 17u);
    }
    if (contamination_strength < 0.10) {
        static constexpr const char* kOptions[] = {
            "delayed leakage between directions",
            "delayed leakage between directions after the first sweep",
            "delayed leakage between directions rather than immediate blending"
        };
        return pick_phrase(kOptions, hash, 18u);
    }
    static constexpr const char* kOptions[] = {
        "immediate leakage between directions",
        "immediate leakage between directions from the opening motion",
        "immediate leakage between directions instead of clean mode splitting"
    };
    return pick_phrase(kOptions, hash, 19u);
}

const char* launch_phrase(double launch_bias,
                          std::uint64_t hash) {
    if (launch_bias < 0.45) {
        static constexpr const char* kOptions[] = {
            "close to the soft axis",
            "mostly aligned with the soft axis",
            "biased toward the soft direction"
        };
        return pick_phrase(kOptions, hash, 20u);
    }
    if (launch_bias < 0.95) {
        static constexpr const char* kOptions[] = {
            "between the soft and hard directions",
            "with both principal directions already engaged",
            "at an oblique angle to the soft axis"
        };
        return pick_phrase(kOptions, hash, 21u);
    }
    static constexpr const char* kOptions[] = {
        "already tilted toward the hard axis",
        "strongly committed to the hard direction",
        "more heavily loaded onto the hard axis than the soft one"
    };
    return pick_phrase(kOptions, hash, 22u);
}

const char* transfer_phrase(double contamination_strength,
                            double coupling_strength,
                            std::uint64_t hash) {
    if (contamination_strength < 0.03) {
        static constexpr const char* kOptions[] = {
            "stay delayed until nonlinear coupling accumulates",
            "wait for several swings before it becomes obvious",
            "arrive only after several exchanges have accumulated"
        };
        return pick_phrase(kOptions, hash, 23u);
    }
    if (coupling_strength < 0.25) {
        static constexpr const char* kOptions[] = {
            "show up after several swings",
            "take a few visible cycles to emerge",
            "arrive late rather than in the opening motion"
        };
        return pick_phrase(kOptions, hash, 24u);
    }
    if (coupling_strength < 0.60) {
        static constexpr const char* kOptions[] = {
            "appear near the first reversal",
            "become obvious near the first turnaround",
            "first show itself near the first turning point"
        };
        return pick_phrase(kOptions, hash, 25u);
    }
    static constexpr const char* kOptions[] = {
        "appear inside the opening motion",
        "show itself before the first full swing is over",
        "be visible almost immediately"
    };
    return pick_phrase(kOptions, hash, 26u);
}

} // namespace

MetaSpec generate_meta_spec(const std::vector<double>& lanes) {
    if (lanes.size() != 32u) {
        throw std::invalid_argument("generate_meta_spec requires exactly 32 lanes");
    }

    const Spectral2 g = make_spectral(lanes[0], lanes[1], lanes[2], 0.75, 1.85);
    const Spectral2 v = make_spectral(lanes[3], lanes[4], lanes[5], -1.10, 1.10);

    Spectral2 s;
    if (lanes[6] <= 0.18) {
        s = make_spectral_from_values(0.0, 0.0, 0.0);
    } else {
        const double envelope = smoothstep((lanes[6] - 0.18) / 0.82);
        const double lambda0 = envelope * lerp(-0.80, 0.80, lanes[6]);
        const double lambda1 = envelope * lerp(-0.80, 0.80, lanes[7]);
        const double theta = kPi * (lanes[8] - 0.5);
        s = make_spectral_from_values(lambda0, lambda1, theta);
    }

    const Spectral2 c0 = make_spectral(lanes[9], lanes[10], lanes[11], -0.55, 0.55);
    const Spectral2 c1 = make_spectral(lanes[12], lanes[13], lanes[14], -0.55, 0.55);
    const Spectral2 h5 = make_spectral(lanes[15], lanes[16], lanes[17], -1.0, 1.0);
    const Spectral2 h6 = make_spectral(lanes[18], lanes[19], lanes[20], -1.0, 1.0);
    const Spectral2 h7 = make_spectral(lanes[21], lanes[22], lanes[23], -1.0, 1.0);
    const Spectral2 h8 = make_spectral(lanes[24], lanes[25], lanes[26], -1.0, 1.0);
    const Spectral2 h9 = make_spectral(lanes[27], lanes[28], lanes[29], -1.0, 1.0);

    const Mat2 j = {0.0, 1.0, -1.0, 0.0};

    const double gamma0 = 0.75 * comm_scalar(c0.matrix, h5.matrix)
        / (1.0 + frob(c0.matrix) + frob(h5.matrix));
    const double gamma1 = 0.75 * comm_scalar(c1.matrix, h6.matrix)
        / (1.0 + frob(c1.matrix) + frob(h6.matrix));
    const double tau = 0.65 * comm_scalar(h7.matrix, h8.matrix)
        / (1.0 + frob(h7.matrix) + frob(h8.matrix));

    const double driver =
        0.5 * (h9.lambda0 + h9.lambda1)
        + 0.35 * (h9.lambda0 - h9.lambda1) * std::cos(2.0 * (h9.theta - g.theta))
        + 0.15 * std::sin(2.0 * (h9.theta - v.theta));

    const double c_norm = frob(c0.matrix) + frob(c1.matrix);
    const double g_norm = std::abs(gamma0) + std::abs(gamma1);
    const double gyro_ratio = g_norm / (g_norm + c_norm + kNormEpsilon);
    const double contrast =
        0.5 * (
            spectral_anisotropy(g.lambda0, g.lambda1) +
            spectral_anisotropy(v.lambda0, v.lambda1));
    const double misalign = std::abs(std::sin(2.0 * (g.theta - v.theta)));
    const double w_scale = 0.45 * misalign * gyro_ratio;
    const Spectral2 w = make_spectral_from_values(
        w_scale * (1.0 + 0.35 * contrast),
        -w_scale * (1.0 - 0.35 * contrast),
        0.5 * (g.theta + v.theta));

    const double phi = kTwoPi * lanes[30];
    const bool soft_is_first = std::abs(v.lambda0) <= std::abs(v.lambda1);
    const Vec2d e0 = axis_from_theta(v.theta);
    const Vec2d e1 = orthogonal(e0);
    const Vec2d e_soft = soft_is_first ? e0 : e1;
    const Vec2d e_hard = soft_is_first ? e1 : e0;

    const double q_radius = std::clamp(
        (0.35 + 0.40 * lanes[31]) / (1.0 + 0.35 * (frob(v.matrix) + c_norm + g_norm)),
        0.18,
        0.72);
    const double speed = std::clamp(
        (0.55 + 0.65 * (1.0 - lanes[31])) * (1.0 + 0.30 * gyro_ratio + 0.20 * std::abs(tau)),
        0.35,
        1.35);

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
    meta_spec.p = 3.0 * std::tanh(driver);
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
    const SymmetricAnalysis g = analyze_symmetric(ms.g);
    const SymmetricAnalysis v = analyze_symmetric(ms.V);
    const double symmetry_strength = frob(load_matrix(ms.S));
    const double tau_strength = std::abs(ms.T[0][1]);
    const double gyro_strength = std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]);
    const double coupling_strength =
        frob(load_matrix(ms.C[0])) + frob(load_matrix(ms.C[1]));
    const double contamination_strength = frob(load_matrix(ms.W));

    const Vec2d v_major_axis = axis_from_theta(v.theta_major);
    const Vec2d v_minor_axis = orthogonal(v_major_axis);
    const bool soft_is_major = std::abs(v.lambda_major) <= std::abs(v.lambda_minor);
    const Vec2d soft_axis = soft_is_major ? v_major_axis : v_minor_axis;
    const Vec2d hard_axis = soft_is_major ? v_minor_axis : v_major_axis;
    const Vec2d q0 = {ms.q0[0], ms.q0[1]};
    const double soft_projection = std::abs(dot(q0, soft_axis));
    const double hard_projection = std::abs(dot(q0, hard_axis));
    const double launch_bias = hard_projection / (soft_projection + kNormEpsilon);

    const std::uint64_t hash = descriptor_hash(ms);

    static constexpr const char* kSentence1Leads[] = {
        "Configuration space feels",
        "The local metric is",
        "The inertial fabric is"
    };
    static constexpr const char* kSentence2Leads[] = {
        "A hidden order remains",
        "The law set still carries structure",
        "The motion keeps a continuous scaffold"
    };

    const std::string sentence1 =
        std::string(pick_phrase(kSentence1Leads, hash, 101u))
        + " "
        + geometry_phrase(g, hash)
        + ", while the linear landscape is "
        + potential_phrase(v, hash)
        + "; interactions follow a power law with exponent p ~ "
        + format_number(ms.p)
        + ".";

    std::string sentence2;
    if (symmetry_strength == 0.0) {
        sentence2 =
            "No clean conserved quantity is present; the time arrow is "
            + std::string(time_phrase(tau_strength, hash))
            + ", and gyroscopic deflection is "
            + gyroscopic_phrase(gyro_strength, hash)
            + ".";
    } else {
        sentence2 =
            std::string(pick_phrase(kSentence2Leads, hash, 102u))
            + ": "
            + symmetry_phrase(symmetry_strength, hash)
            + "; the time arrow is "
            + time_phrase(tau_strength, hash)
            + ", and gyroscopic deflection is "
            + gyroscopic_phrase(gyro_strength, hash)
            + ".";
    }

    const std::string sentence3 =
        "Watch for "
        + std::string(contamination_phrase(contamination_strength, hash))
        + "; because the launch begins "
        + launch_phrase(launch_bias, hash)
        + ", the first transfer should "
        + transfer_phrase(contamination_strength, coupling_strength, hash)
        + ".";

    return sentence1 + " " + sentence2 + " " + sentence3;
}
