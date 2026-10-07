// MetaSpecMath: the 2x2 spectral toolkit behind MetaSpec generation. This TU
// includes the header while also linking worldline_seed (whose MetaSpec.cpp
// includes it too), so it doubles as a guard that the header stays ODR-safe
// (all functions inline) -- otherwise this test fails to link.

#include "seed/MetaSpec.hpp"
#include "seed/MetaSpecMath.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

using namespace metaspec_math;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "seed_math_verification failed: " << message << '\n';
        ++g_failures;
    }
}

bool close(double a, double b, double tol) { return std::abs(a - b) <= tol * (1.0 + std::abs(a) + std::abs(b)); }

// Angle difference modulo pi (an eigenvector's direction has no sign).
double axis_gap(double a, double b) {
    double d = std::fmod(std::abs(a - b), kPi);
    return std::min(d, kPi - d);
}

void test_spectral_round_trip() {
    std::mt19937 rng(2024);
    std::uniform_real_distribution<double> L(-3.0, 3.0), T(-kPi / 2.0, kPi / 2.0);
    int bad = 0;
    for (int i = 0; i < 4000; ++i) {
        const double l0 = L(rng), l1 = L(rng), theta = T(rng);
        const Mat2 m = make_spectral_matrix(l0, l1, theta);
        double raw[2][2];
        store_matrix(m, raw);
        const SymmetricAnalysis a = analyze_symmetric(raw);
        const double hi = std::max(l0, l1), lo = std::min(l0, l1);
        if (!close(a.lambda_major, hi, 1e-12) || !close(a.lambda_minor, lo, 1e-12)) ++bad;
        // The major axis is the eigenvector of the larger eigenvalue.
        if (std::abs(l0 - l1) > 1e-6) {
            const double major_theta = (l0 >= l1) ? theta : theta + kPi / 2.0;
            if (axis_gap(a.theta_major, major_theta) > 1e-6) ++bad;
        }
        if (!(a.anisotropy >= 0.0 && a.anisotropy <= 1.0)) ++bad;
    }
    check(bad == 0, "analyze_symmetric must invert make_spectral_matrix (" + std::to_string(bad) + " bad)");
}

void test_matrix_identities() {
    const Mat2 a{1.0, 2.0, 3.0, 4.0};
    const Mat2 id{1.0, 0.0, 0.0, 1.0};
    const Mat2 p = multiply(a, id);
    check(p.xx == a.xx && p.xy == a.xy && p.yx == a.yx && p.yy == a.yy, "A * I == A");
    check(close(frob(a), std::sqrt(30.0), 1e-15), "Frobenius norm of [[1,2],[3,4]] is sqrt(30)");
    check(comm_scalar(a, a) == 0.0, "a matrix commutes with itself");
    check(comm_scalar(a, id) == 0.0, "everything commutes with the identity");
    const Mat2 b{0.0, 1.0, 0.0, 0.0};
    check(comm_scalar(a, b) == -comm_scalar(b, a), "the commutator is antisymmetric");
    double raw[2][2];
    store_matrix(a, raw);
    const Mat2 back = load_matrix(raw);
    check(back.xx == 1.0 && back.xy == 2.0 && back.yx == 3.0 && back.yy == 4.0, "store/load round-trip");
    const Vec2d v = mul(a, {1.0, -1.0});
    check(v.x == -1.0 && v.y == -1.0, "matrix-vector product");
    const Vec2d ax = axis_from_theta(0.3);
    check(close(length(ax), 1.0, 1e-15) && dot(ax, orthogonal(ax)) == 0.0, "unit axis and its orthogonal");
}

void test_scalar_helpers() {
    check(clamp01(-1.0) == 0.0 && clamp01(2.0) == 1.0 && clamp01(0.25) == 0.25, "clamp01");
    check(lerp(2.0, 4.0, 0.5) == 3.0 && lerp(2.0, 4.0, 0.0) == 2.0 && lerp(2.0, 4.0, 1.0) == 4.0, "lerp");
    check(smoothstep(-1.0) == 0.0 && smoothstep(2.0) == 1.0 && smoothstep(0.5) == 0.5, "smoothstep");
    check(spectral_anisotropy(1.0, 1.0) == 0.0, "equal eigenvalues are isotropic");
    check(close(spectral_anisotropy(1.0, 0.0), 1.0, 1e-8), "one zero eigenvalue is fully anisotropic");
    const Spectral2 s = make_spectral(0.0, 1.0, 0.5, -2.0, 2.0);
    check(s.lambda0 == -2.0 && s.lambda1 == 2.0 && s.theta == 0.0, "make_spectral maps the unit cube");
}

void test_metaspec_is_still_linkable() {
    // Same TU uses the header and the library: proves there are no duplicate
    // definitions, and the generator still produces a finite spec.
    const MetaSpec ms = generate_meta_spec(std::string("odr-guard"));
    check(std::isfinite(ms.g[0][0]) && std::isfinite(ms.V[1][1]), "generated MetaSpec is finite");
    const SymmetricAnalysis g = analyze_symmetric(ms.g);
    check(g.lambda_major >= g.lambda_minor, "eigenvalues are ordered major >= minor");
}

} // namespace

int main() {
    test_spectral_round_trip();
    test_matrix_identities();
    test_scalar_helpers();
    test_metaspec_is_still_linkable();
    if (g_failures > 0) {
        std::cerr << g_failures << " seed math check(s) failed\n";
        return EXIT_FAILURE;
    }
    return 0;
}
