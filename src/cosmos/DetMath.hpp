// DetMath.hpp -- deterministic, cross-platform transcendental functions for the
// simulation hot path. The platform libm implementations of exp/log/pow/sin/cos
// are NOT bit-identical (they differ in the last ulp between glibc, macOS libm
// and the MSVC CRT), so an N-body integrator that calls them diverges across
// platforms after a few hundred chaotic steps. These replacements use only
// IEEE-754 operations that ARE required to be identical everywhere -- +, -, *, /,
// sqrt, and the exact bit-manipulators frexp/ldexp/round -- so the sandbox
// evolves to the same bits on every OS and compiler.
//
// Header-only, pure, deterministic. Accuracy is ~1e-13 relative or better over
// the simulation's operating range, which is far tighter than the dynamics need;
// the point is reproducibility, not beating libm's accuracy.
//
// Sources: standard range-reduction + minimax/Taylor kernels (Cody-Waite
// argument reduction; fdlibm-style splits of ln2 and pi/2).

#ifndef COSMOS_DETMATH_HPP
#define COSMOS_DETMATH_HPP

#include <cmath> // std::sqrt, std::frexp, std::ldexp, std::round (all exact/deterministic)

namespace cosmos {
namespace detmath {

// Two-part (Cody-Waite) constants for accurate argument reduction.
inline constexpr double kLn2Hi = 6.93147180369123816490e-01;
inline constexpr double kLn2Lo = 1.90821492927058770002e-10;
inline constexpr double kLn2 = 6.93147180559945309417e-01;
inline constexpr double kLog2e = 1.44269504088896340736e+00; // 1/ln2
inline constexpr double kPio2Hi = 1.57079632673412561417e+00;
inline constexpr double kPio2Lo = 6.07710050650619224932e-11;
inline constexpr double kTwoOverPi = 6.36619772367581382433e-01; // 2/pi
inline constexpr double kSqrtHalf = 7.07106781186547524401e-01;

// exp(x): range-reduce x = k ln2 + r with |r| <= ln2/2, evaluate exp(r) by a
// Taylor series, then scale by 2^k (exact via ldexp).
inline double exp(double x) {
    if (x >= 709.0)
        return HUGE_VAL; // overflow guard (deterministic)
    if (x <= -745.0)
        return 0.0; // underflow guard (deterministic)
    const double k = std::round(x * kLog2e);
    const double r = (x - k * kLn2Hi) - k * kLn2Lo; // |r| <= ln2/2 ~ 0.3466
    // exp(r) = 1 + r(1 + r/2(1 + r/3(... + r/14))) -- Horner of the Taylor series.
    double s = 1.0;
    for (int n = 14; n >= 1; --n) {
        s = 1.0 + (r / static_cast<double>(n)) * s;
    }
    return std::ldexp(s, static_cast<int>(k));
}

// log(x), x > 0: x = m 2^e with m in [sqrt(1/2), sqrt(2)); log(m) via the atanh
// series in s = (m-1)/(m+1).
inline double log(double x) {
    if (x <= 0.0)
        return (x == 0.0) ? -HUGE_VAL : NAN;
    int e = 0;
    double m = std::frexp(x, &e); // m in [0.5,1), x = m 2^e (exact)
    if (m < kSqrtHalf) {
        m *= 2.0;
        e -= 1;
    }
    const double f = m - 1.0;
    const double s = f / (2.0 + f); // (m-1)/(m+1), |s| < 0.1716
    const double z = s * s;
    // log(m) = 2 s (1 + z/3 + z^2/5 + ... + z^7/15) -- Horner in z.
    double poly = 1.0 / 15.0;
    poly = 1.0 / 13.0 + z * poly;
    poly = 1.0 / 11.0 + z * poly;
    poly = 1.0 / 9.0 + z * poly;
    poly = 1.0 / 7.0 + z * poly;
    poly = 1.0 / 5.0 + z * poly;
    poly = 1.0 / 3.0 + z * poly;
    poly = 1.0 + z * poly;
    const double log_m = 2.0 * s * poly;
    return static_cast<double>(e) * kLn2Hi + (log_m + static_cast<double>(e) * kLn2Lo);
}

// pow(x, y) for x > 0: x^y = exp(y log x). (The simulation only ever raises
// strictly positive bases -- softened squared distances and radii sums.)
inline double pow(double x, double y) {
    if (y == 0.0)
        return 1.0;
    if (x == 1.0)
        return 1.0;
    if (x <= 0.0)
        return (x == 0.0 && y > 0.0) ? 0.0 : NAN;
    return detmath::exp(y * detmath::log(x));
}

namespace detail {

// sin(r), cos(r) for |r| <= pi/4 via Taylor series (Horner in z = r^2).
// sin(r) = r * (1 - z/6 + z^2/120 - z^3/5040 + z^4/362880 - z^5/39916800 + z^6/13!).
inline double sin_kernel(double r, double z) {
    double p = 1.0 / 6227020800.0; // 13!
    p = -1.0 / 39916800.0 + z * p; // 11!
    p = 1.0 / 362880.0 + z * p;    // 9!
    p = -1.0 / 5040.0 + z * p;     // 7!
    p = 1.0 / 120.0 + z * p;       // 5!
    p = -1.0 / 6.0 + z * p;        // 3!
    p = 1.0 + z * p;               // 1
    return r * p;
}
inline double cos_kernel(double z) {
    double p = 1.0 / 479001600.0; // r^12
    p = -1.0 / 3628800.0 + z * p; // r^10
    p = 1.0 / 40320.0 + z * p;    // r^8
    p = -1.0 / 720.0 + z * p;     // r^6
    p = 1.0 / 24.0 + z * p;       // r^4
    p = -1.0 / 2.0 + z * p;       // r^2
    return 1.0 + z * p;
}

} // namespace detail

// sin(x): reduce x to r in [-pi/4, pi/4] plus a quadrant, then pick the kernel.
inline double sin(double x) {
    const double k = std::round(x * kTwoOverPi);
    const double r = (x - k * kPio2Hi) - k * kPio2Lo;
    const double z = r * r;
    const long long q = static_cast<long long>(k) & 3;
    switch (q) {
    case 0:
        return detail::sin_kernel(r, z);
    case 1:
        return detail::cos_kernel(z);
    case 2:
        return -detail::sin_kernel(r, z);
    default:
        return -detail::cos_kernel(z);
    }
}

inline double cos(double x) {
    const double k = std::round(x * kTwoOverPi);
    const double r = (x - k * kPio2Hi) - k * kPio2Lo;
    const double z = r * r;
    const long long q = static_cast<long long>(k) & 3;
    switch (q) {
    case 0:
        return detail::cos_kernel(z);
    case 1:
        return -detail::sin_kernel(r, z);
    case 2:
        return -detail::cos_kernel(z);
    default:
        return detail::sin_kernel(r, z);
    }
}

} // namespace detmath
} // namespace cosmos

#endif // COSMOS_DETMATH_HPP
