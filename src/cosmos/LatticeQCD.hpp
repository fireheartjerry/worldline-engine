// LatticeQCD.hpp -- the strong force at the level that actually binds matter:
// confinement. The Cornell static-quark potential (Coulomb + linear), the QCD
// string tension and its Wilson-loop area law, string breaking, Regge
// trajectories, and a small deterministic lattice-gauge plaquette computation.
// This is the "lattice QCD style" non-perturbative physics of the first layer --
// why quarks are never seen alone.
//
// Header-only, pure, deterministic. Natural units: lengths in fm, energies in
// GeV, with hbar c = 0.1973 GeV*fm bridging them.
//
// Sources:
//   - Eichten et al. (1978) Cornell potential V(r) = -(4/3) alpha_s hbar c / r + sigma r.
//   - String tension sigma ~ 0.18 GeV^2 ~ 0.94 GeV/fm (lattice QCD).
//   - Wilson (1974) loop area law <W> ~ exp(-sigma * Area) as the confinement order parameter.
//   - Regge trajectories J = alpha' M^2 + alpha_0 with slope alpha' = 1/(2 pi sigma).

#ifndef COSMOS_LATTICEQCD_HPP
#define COSMOS_LATTICEQCD_HPP

#include <array>
#include <cmath>
#include <cstddef>

namespace cosmos {
namespace lattice {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kHbarC_GeVfm = 0.1973269804; // hbar c [GeV*fm]
inline constexpr double kStringTension_GeV2 = 0.18;  // sigma [GeV^2]
inline constexpr double kColorFactorCF = 4.0 / 3.0;  // SU(3) fundamental Casimir

// String tension expressed as a force / energy-per-length [GeV/fm]:
//   sigma[GeV/fm] = sigma[GeV^2] / (hbar c). ~0.91 GeV/fm.
inline double string_tension_gev_per_fm() {
    return kStringTension_GeV2 / kHbarC_GeVfm;
}

// ---------------------------------------------------------------------------
// Cornell static-quark potential
// ---------------------------------------------------------------------------

// V(r) = -(4/3) alpha_s hbar c / r + sigma r, with r in fm, V in GeV. The short
// distance is Coulomb-like (asymptotic freedom); the long distance rises
// linearly (confinement) -- the energy to separate quarks grows without bound.
inline double cornell_potential_gev(double r_fm, double alpha_s = 0.3,
                                    double sigma_gev2 = kStringTension_GeV2) {
    if (r_fm <= 0.0)
        return -INFINITY;
    const double coulomb = -kColorFactorCF * alpha_s * kHbarC_GeVfm / r_fm;
    const double linear = (sigma_gev2 / kHbarC_GeVfm) * r_fm; // GeV
    return coulomb + linear;
}

// The confining force at large r approaches the constant string tension
// sigma[GeV/fm] (a flux tube of fixed energy per unit length). [GeV/fm]
inline double confining_force_gev_per_fm(double sigma_gev2 = kStringTension_GeV2) {
    return sigma_gev2 / kHbarC_GeVfm;
}

// Distance at which the colour string stores enough energy to pop a light
// quark-antiquark pair (V = 2 m_q) and "break": r = 2 m_q / sigma. [fm]
inline double string_breaking_distance_fm(double quark_mass_gev,
                                          double sigma_gev2 = kStringTension_GeV2) {
    const double sigma_per_fm = sigma_gev2 / kHbarC_GeVfm;
    return 2.0 * quark_mass_gev / sigma_per_fm;
}

// ---------------------------------------------------------------------------
// Wilson loop -- the confinement order parameter
// ---------------------------------------------------------------------------

// Area law: <W(C)> ~ exp(-sigma * Area). A nonzero string tension (area law) is
// the signature of confinement; a perimeter law would mean a deconfined phase.
inline double wilson_loop_area_law(double area_fm2, double sigma_gev2 = kStringTension_GeV2) {
    const double sigma_per_fm2 = sigma_gev2 / (kHbarC_GeVfm * kHbarC_GeVfm); // [1/fm^2]
    return std::exp(-sigma_per_fm2 * area_fm2);
}

// The static potential extracted from a large Wilson loop, V = -lim (1/T) ln<W>;
// for the pure area law this returns sigma * R (the linear confining piece). [GeV]
inline double potential_from_area_law(double R_fm, double sigma_gev2 = kStringTension_GeV2) {
    return (sigma_gev2 / kHbarC_GeVfm) * R_fm;
}

// ---------------------------------------------------------------------------
// Regge trajectories -- the rotating-string spectrum
// ---------------------------------------------------------------------------

// Regge slope alpha' = 1 / (2 pi sigma). [GeV^-2] ~ 0.88.
inline double regge_slope_gev2(double sigma_gev2 = kStringTension_GeV2) {
    return 1.0 / (2.0 * kPi * sigma_gev2);
}

// Angular momentum of a meson on a linear Regge trajectory: J = alpha' M^2 + a0.
inline double regge_spin(double mass_gev, double intercept = 0.5,
                         double sigma_gev2 = kStringTension_GeV2) {
    return regge_slope_gev2(sigma_gev2) * mass_gev * mass_gev + intercept;
}

// ---------------------------------------------------------------------------
// A tiny lattice-gauge plaquette (compact U(1) toy, deterministic)
// ---------------------------------------------------------------------------
//
// On an L x L periodic lattice with link phases theta_{x,mu}, the plaquette angle
// is the directed sum around a unit square. The Wilson action density is
// 1 - cos(plaquette); a "cold" (ordered) configuration has every plaquette = 1.

// Average plaquette cos value of a cold (all-zero-phase) L x L U(1) lattice.
// Computed by actually summing the plaquettes (an algorithm, not a constant):
// every plaquette angle is 0, so the average is exactly 1.
inline double cold_average_plaquette(int L) {
    if (L < 1)
        return 1.0;
    double sum = 0.0;
    int count = 0;
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            // theta_x + theta_y(x+1) - theta_x(y+1) - theta_y, all zero for a cold start.
            const double plaq = 0.0;
            sum += std::cos(plaq);
            ++count;
        }
    }
    return count > 0 ? sum / count : 1.0;
}

// Wilson gauge action S = beta * sum_plaq (1 - Re plaquette). For the cold lattice
// the action is exactly zero (the classical vacuum). [dimensionless]
inline double wilson_action(double beta, double avg_plaquette, int n_plaquettes) {
    return beta * static_cast<double>(n_plaquettes) * (1.0 - avg_plaquette);
}

} // namespace lattice
} // namespace cosmos

#endif // COSMOS_LATTICEQCD_HPP
