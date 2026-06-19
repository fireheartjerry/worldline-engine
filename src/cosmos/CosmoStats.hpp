#pragma once
// CosmoStats — header-only, pure, deterministic astrophysical statistical
// distributions for stars, planets, galaxies, halos, and cosmology. Every
// function here is a closed-form evaluation or an inverse-CDF sampler (no
// rejection loops), so a deterministic generator can draw populations that obey
// observed/standard relations. Inputs are physical quantities (Msun, Mearth,
// Mpc, km/s, ...); outputs are documented per function.
//
// Sources (deep-research appendix):
//   - Kroupa (2001) IMF: broken power law dN/dM ~ M^-alpha,
//       alpha=1.3 for 0.08-0.5 Msun, alpha=2.3 for >0.5 Msun. MNRAS 322, 231.
//   - Eker et al. (2018) mass-luminosity relation (piecewise log L-log M).
//       MNRAS 479, 5491. Anchored to L(1 Msun)=1 Lsun.
//   - Chen & Kipping (2017) Forecaster mass-radius: R ~ M^0.28 (Terran),
//       M^0.59 (Neptunian), M^-0.04 (Jovian). ApJ 834, 17.
//   - Baldry et al. (2012) galaxy stellar mass function (Schechter form).
//       MNRAS 421, 621.
//   - Navarro, Frenk & White (1997) halo density profile. ApJ 490, 493.
//   - Dutton & Maccio (2014) concentration-mass relation, z=0.
//       MNRAS 441, 3359.
//   - Sheth & Tormen (1999) halo multiplicity function. MNRAS 308, 119.
//   - Kormendy & Ho (2013) M-sigma relation. ARA&A 51, 511.
//   - Planck 2018 cosmological parameters (TT,TE,EE+lowE+lensing).
//       A&A 641, A6.

#include "cosmos/Spectrum.hpp"

#include <array>
#include <cmath>

namespace cosmos {
namespace cstat {

// =====================================================================
// Cosmological constants — Planck 2018 (A&A 641, A6, base-LCDM).
// =====================================================================
constexpr double kH0          = 67.4;     // Hubble constant [km/s/Mpc]
constexpr double kOmegaM      = 0.315;    // total matter density
constexpr double kOmegaLambda = 0.685;    // dark energy density
constexpr double kOmegaB      = 0.0493;   // baryon density
constexpr double kOmegaC      = 0.265;    // cold dark matter density
constexpr double kSigma8      = 0.811;    // rms density fluctuation, 8 h^-1 Mpc
constexpr double kNs          = 0.965;    // scalar spectral index
constexpr double kAgeGyr      = 13.797;   // age of universe [Gyr]
constexpr double kTcmb        = 2.725;    // CMB temperature [K]
constexpr double kRdragMpc    = 147.09;   // sound horizon at drag epoch [Mpc]
constexpr double kDeltaC      = 1.686;    // linear collapse threshold

// =====================================================================
// STELLAR
// =====================================================================

// Kroupa (2001) IMF inverse-CDF sampler over [m_lo, m_hi] (Msun).
// dN/dM ~ M^-alpha with alpha=1.3 on [0.08,0.5] and alpha=2.3 on [0.5,inf).
// The two segments are continuous in dN/dM at the 0.5 Msun break. We integrate
// each segment analytically, normalise over the requested [m_lo,m_hi] window,
// and invert the piecewise CDF for u01 in [0,1). No rejection.
inline double sample_imf_mass(double u01, double m_lo = 0.08, double m_hi = 120.0) {
    u01 = clampd(u01, 0.0, 1.0 - 1e-15);
    if (m_hi <= m_lo) return m_lo;
    const double mbreak = 0.5;
    const double a1 = 1.3, a2 = 2.3;
    // Continuity of dN/dM at the break: k1 * mbreak^-a1 = k2 * mbreak^-a2.
    // Take k1 = 1, then k2 = mbreak^(a2-a1).
    const double k1 = 1.0;
    const double k2 = std::pow(mbreak, a2 - a1);

    // Indefinite integral of k * M^-alpha dM = k * M^(1-alpha)/(1-alpha).
    auto antideriv = [](double m, double k, double alpha) {
        return k * std::pow(m, 1.0 - alpha) / (1.0 - alpha);
    };

    // Effective segment bounds clipped to [m_lo,m_hi].
    const double lo1 = m_lo, hi1 = (m_hi < mbreak ? m_hi : mbreak);
    const double lo2 = (m_lo > mbreak ? m_lo : mbreak), hi2 = m_hi;

    double i1 = 0.0, i2 = 0.0;
    if (hi1 > lo1) i1 = antideriv(hi1, k1, a1) - antideriv(lo1, k1, a1);
    if (hi2 > lo2) i2 = antideriv(hi2, k2, a2) - antideriv(lo2, k2, a2);
    const double total = i1 + i2;
    if (total <= 0.0) return m_lo;

    const double target = u01 * total;
    // Invert M^(1-alpha) within the segment that contains `target`.
    if (target <= i1 && i1 > 0.0) {
        // Solve antideriv(M)-antideriv(lo1) = target on segment 1.
        const double base = antideriv(lo1, k1, a1);
        const double val = (base + target) * (1.0 - a1) / k1; // = M^(1-a1)
        return std::pow(val, 1.0 / (1.0 - a1));
    } else {
        const double rem = target - i1;
        const double base = antideriv(lo2, k2, a2);
        const double val = (base + rem) * (1.0 - a2) / k2; // = M^(1-a2)
        return std::pow(val, 1.0 / (1.0 - a2));
    }
}

// Piecewise mass-luminosity (Lsun, normalised so L(1)=1). Eker et al. (2018)
// style segmentation with Eddington flattening for the most massive stars.
inline double stellar_luminosity(double M) {
    if (M <= 0.0) return 0.0;
    // Build a continuous piecewise power law by chaining normalisations at the
    // breakpoints 0.43, 2, 20 so L is continuous; anchor L(1)=1 in the 0.43-2
    // segment (exponent 4, normalisation 1).
    const double c_mid = 1.0;                          // L = c_mid * M^4 on [0.43,2]
    const double c_low = c_mid * std::pow(0.43, 4.0 - 2.3); // continuity at 0.43
    const double c_hi  = c_mid * std::pow(2.0, 4.0 - 3.5);  // continuity at 2
    const double c_vh  = c_hi * std::pow(20.0, 3.5 - 1.0);  // continuity at 20
    if (M < 0.43) return c_low * std::pow(M, 2.3);
    if (M < 2.0)  return c_mid * std::pow(M, 4.0);
    if (M < 20.0) return c_hi  * std::pow(M, 3.5);
    return c_vh * std::pow(M, 1.0); // Eddington-flattened high-mass tail
}

// Main-sequence lifetime [Gyr]. Computed as fuel/burn-rate t = 10 * M / L(M),
// so it stays consistent with whatever stellar_luminosity() returns. The Sun
// (M=L=1) gives ~10 Gyr by construction.
inline double stellar_lifetime_gyr(double M) {
    const double L = stellar_luminosity(M);
    if (L <= 0.0) return 0.0;
    return 10.0 * M / L;
}

// Schwarzschild radius [km] for a mass in solar masses (2GM/c^2 ~ 2.95 km/Msun).
inline double schwarzschild_radius_km(double M_sun) {
    return 2.95 * M_sun;
}

// M-sigma relation, Kormendy & Ho (2013):
//   M_BH/1e9 Msun = 0.31 * (sigma/200 km/s)^4.38.  Returns M_BH in Msun.
inline double m_sigma_bh_mass(double sigma_kms) {
    return 0.31e9 * std::pow(sigma_kms / 200.0, 4.38);
}

// =====================================================================
// PLANETARY
// =====================================================================

// Chen & Kipping (2017) Forecaster mass-radius (Earth units, R(1)=1).
//   R ~ M^0.28  for M < 2.04        (Terran)
//   R ~ M^0.59  for 2.04 < M < 132  (Neptunian)
//   R ~ M^-0.04 for 132 < M < 2.66e4 (Jovian)
// Normalisations chained so R is continuous at the breakpoints; anchored R(1)=1.
inline double planet_radius_earth(double M_earth) {
    if (M_earth <= 0.0) return 0.0;
    const double b1 = 2.04, b2 = 132.0;
    const double c_terran = 1.0;                                  // R(1)=1
    const double c_nept   = c_terran * std::pow(b1, 0.28 - 0.59); // continuity at b1
    const double c_jov    = c_nept   * std::pow(b2, 0.59 - (-0.04)); // continuity at b2
    if (M_earth < b1) return c_terran * std::pow(M_earth, 0.28);
    if (M_earth < b2) return c_nept   * std::pow(M_earth, 0.59);
    return c_jov * std::pow(M_earth, -0.04);
}

// =====================================================================
// GALACTIC
// =====================================================================

// Schechter (1976) function, number density per unit mass:
//   phi(M) dM = phi_star * (M/Mstar)^alpha * exp(-M/Mstar) / Mstar dM.
// Baldry et al. (2012) parameterisation for the galaxy stellar mass function.
inline double schechter_phi(double M, double Mstar, double alpha, double phi_star) {
    const double x = M / Mstar;
    return phi_star * std::pow(x, alpha) * std::exp(-x) / Mstar;
}

// NFW (1997) density profile: rho(r) = rho_s / ((r/r_s) * (1+r/r_s)^2).
inline double nfw_density(double r, double rho_s, double r_s) {
    const double x = r / r_s;
    const double t = 1.0 + x;
    return rho_s / (x * t * t);
}

// Dutton & Maccio (2014) z=0 concentration-mass relation:
//   log10 c = 0.905 - 0.101 * log10(M200 / 1e12 h^-1 Msun).
// Pass M200_h = M200 / 1e12 (so M200_h=1 -> c~8.0).
inline double nfw_concentration(double M200_h) {
    const double log_c = 0.905 - 0.101 * std::log10(M200_h);
    return std::pow(10.0, log_c);
}

// =====================================================================
// COSMOLOGY
// =====================================================================

// Proportional matter power spectrum shape:
//   P(k) ~ k^ns           for k < k_turn
//   P(k) ~ k^(ns-4)       for k > k_turn
// Normalised so the two branches join continuously at k_turn.
inline double power_spectrum_pk(double k, double k_turn = 0.018) {
    if (k <= 0.0) return 0.0;
    if (k < k_turn) return std::pow(k, kNs);
    // Continuity at k_turn: c * k_turn^(ns-4) = k_turn^ns  ->  c = k_turn^4.
    const double c = std::pow(k_turn, 4.0);
    return c * std::pow(k, kNs - 4.0);
}

// Two-point correlation function (power-law form), xi(5 h^-1 Mpc) = 1:
//   xi(r) = (r/5)^-1.8.
inline double correlation_xi(double r_hMpc) {
    if (r_hMpc <= 0.0) return 0.0;
    return std::pow(r_hMpc / 5.0, -1.8);
}

// Sheth & Tormen (1999) halo multiplicity function in nu = deltaC/sigma:
//   f(nu) = A*sqrt(2/pi)*(1+(a*nu^2)^-p)*sqrt(a)*nu*exp(-a*nu^2/2),
//   A=0.322, a=0.707, p=0.3.
inline double sheth_tormen_f(double nu) {
    const double A = 0.322, a = 0.707, p = 0.3;
    const double an2 = a * nu * nu;
    const double pref = A * std::sqrt(2.0 / 3.14159265358979323846);
    return pref * (1.0 + std::pow(an2, -p)) * std::sqrt(a) * nu * std::exp(-an2 / 2.0);
}

} // namespace cstat
} // namespace cosmos
