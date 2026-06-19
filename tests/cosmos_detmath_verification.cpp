// Verifies cosmos/DetMath.hpp: the deterministic transcendentals reproduce the
// standard library functions to high accuracy across their operating range, obey
// the expected identities, and are pure/deterministic (the whole point -- they
// must give the same bits on every platform, which we approximate here by
// checking exact repeatability and tight agreement with std::).

#include "cosmos/DetMath.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace dm = cosmos::detmath;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_detmath_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_detmath_verification FAILED: " << what << " got=" << got
                  << " want=" << want << " err=" << std::abs(got - want) << "\n";
        ++g_failures;
    }
}
// Relative-or-absolute tolerance suited to comparing against std::.
void approx(double got, double want, const std::string &what, double rel = 1e-12) {
    const double tol = rel * std::max(1.0, std::abs(want));
    close(got, want, tol, what);
}
} // namespace

int main() {
    // --- exp ----------------------------------------------------------------
    for (double x = -20.0; x <= 20.0; x += 0.37) {
        approx(dm::exp(x), std::exp(x), "exp matches std over [-20,20]", 1e-11);
    }
    close(dm::exp(0.0), 1.0, 1e-15, "exp(0) = 1");
    check(dm::exp(-1000.0) == 0.0, "exp underflows to 0 deterministically");

    // --- log ----------------------------------------------------------------
    for (double x = 0.01; x <= 100.0; x += 0.13) {
        approx(dm::log(x), std::log(x), "log matches std over (0,100]", 1e-12);
    }
    close(dm::log(1.0), 0.0, 1e-15, "log(1) = 0");
    // exp/log are inverses.
    for (double x = 0.05; x <= 50.0; x += 0.7) {
        approx(dm::exp(dm::log(x)), x, "exp(log(x)) = x", 1e-11);
    }

    // --- pow ----------------------------------------------------------------
    // The exact exponents the N-body force law uses.
    for (double b = 0.1; b <= 8.0; b += 0.21) {
        approx(dm::pow(b, -1.5), std::pow(b, -1.5), "pow(.,-1.5) matches std", 1e-11);
        approx(dm::pow(b, 4.0), std::pow(b, 4.0), "pow(.,4) matches std", 1e-10);
        approx(dm::pow(b, -3.0), std::pow(b, -3.0), "pow(.,-3) matches std", 1e-10);
        approx(dm::pow(b, -1.25), std::pow(b, -1.25), "pow(.,-1.25) matches std", 1e-11);
    }
    close(dm::pow(2.0, 0.0), 1.0, 1e-15, "pow(x,0) = 1");
    close(dm::pow(9.0, 0.5), 3.0, 1e-12, "pow(9,0.5) = 3");

    // --- sin / cos ----------------------------------------------------------
    for (double x = -7.0; x <= 7.0; x += 0.05) {
        approx(dm::sin(x), std::sin(x), "sin matches std over [-7,7]", 1e-12);
        approx(dm::cos(x), std::cos(x), "cos matches std over [-7,7]", 1e-12);
    }
    // Pythagorean identity holds tightly.
    for (double x = 0.0; x <= 6.28; x += 0.11) {
        const double s = dm::sin(x), c = dm::cos(x);
        close(s * s + c * c, 1.0, 1e-12, "sin^2 + cos^2 = 1");
    }
    close(dm::sin(0.0), 0.0, 1e-15, "sin(0) = 0");
    close(dm::cos(0.0), 1.0, 1e-15, "cos(0) = 1");

    // --- Determinism (exact repeatability) ----------------------------------
    check(dm::exp(1.234) == dm::exp(1.234), "exp bit-repeatable");
    check(dm::log(5.678) == dm::log(5.678), "log bit-repeatable");
    check(dm::pow(2.5, -1.5) == dm::pow(2.5, -1.5), "pow bit-repeatable");
    check(dm::sin(2.0) == dm::sin(2.0) && dm::cos(2.0) == dm::cos(2.0), "sin/cos bit-repeatable");

    if (g_failures != 0) {
        std::cerr << "cosmos_detmath_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_detmath_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
