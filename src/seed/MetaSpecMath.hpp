#pragma once
// MetaSpec — 2D linear-algebra and spectral toolkit used by the generator
// (vectors, 2x2 matrices, spectral/eigen analysis, matrix load/store).
#include <cmath>
#include <cstdint>

namespace metaspec_math {

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

double normalized_commutator(const Mat2& lhs, const Mat2& rhs) {
    return comm_scalar(lhs, rhs) / (1.0 + frob(lhs) + frob(rhs));
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


} // namespace metaspec_math
