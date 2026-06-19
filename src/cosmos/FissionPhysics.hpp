// FissionPhysics.hpp -- fission in depth, beyond the bare fissility parameter:
// the asymmetric fragment mass distribution (pinned by shell closures), prompt
// and delayed neutron emission, the energy partition of the ~200 MeV release, the
// double-humped fission barrier, and reactor physics -- the four- and six-factor
// formulas and the criticality condition.
//
// Header-only, pure, deterministic. Energies in MeV.
//
// Sources:
//   - Asymmetric thermal fission of U-235: light peak ~95, heavy peak ~139
//       (the heavy peak fixed near the N=82 / Z=50 shells).
//   - Prompt neutron multiplicity nu-bar (U-235 ~2.42, Pu-239 ~2.88).
//   - Delayed neutron fraction beta (U-235 ~0.0065, Pu-239 ~0.0021).
//   - Energy partition (fragment KE ~169, neutrons ~5, gammas ~7, betas ~7,
//       neutrinos ~10, delayed ~6 MeV) -- total ~200-205 MeV.
//   - Four-factor k_inf = eta*epsilon*p*f; six-factor k_eff with leakage.

#ifndef COSMOS_FISSIONPHYSICS_HPP
#define COSMOS_FISSIONPHYSICS_HPP

#include <cmath>

namespace cosmos {
namespace fission {

// --- Fragment mass distribution ---------------------------------------------

struct FragmentPeaks {
    int light;
    int heavy;
};

// Most-probable light/heavy fragment masses for a fissioning compound nucleus of
// mass A_c. The heavy fragment clusters near A~139 (shell-stabilised); the light
// fragment carries the rest after ~nu prompt neutrons leave.
inline FragmentPeaks fragment_peaks(int A_compound, double nu_bar = 2.4) {
    const int heavy = 139;
    const int light = A_compound - heavy - static_cast<int>(nu_bar + 0.5);
    return {light, heavy};
}

// Fission is asymmetric when the two peaks differ (true for low-energy actinide
// fission); high excitation drives it toward symmetric splitting.
inline bool is_asymmetric(const FragmentPeaks &p) {
    return std::abs(p.heavy - p.light) > 10;
}

// --- Neutron emission -------------------------------------------------------

inline constexpr double kNubar_U235 = 2.42;  // thermal
inline constexpr double kNubar_U238 = 2.45;  // fast
inline constexpr double kNubar_Pu239 = 2.88; // thermal

// Delayed-neutron fractions (beta): the tiny precursor-emitted fraction that
// makes reactor control possible.
inline constexpr double kBeta_U235 = 0.0065;
inline constexpr double kBeta_U238 = 0.0157;
inline constexpr double kBeta_Pu239 = 0.0021;

// --- Energy partition (MeV) -------------------------------------------------

struct EnergyPartition {
    double fragments_ke; // kinetic energy of the fission fragments
    double prompt_neutrons;
    double prompt_gammas;
    double beta_particles;
    double antineutrinos; // escape the reactor
    double delayed;       // delayed gammas + betas
    double total() const {
        return fragments_ke + prompt_neutrons + prompt_gammas + beta_particles + antineutrinos +
               delayed;
    }
    // Energy actually recoverable as heat (everything but the escaping neutrinos).
    double recoverable() const {
        return total() - antineutrinos;
    }
};

// The canonical U-235 thermal-fission energy partition (~200 MeV total).
inline EnergyPartition u235_energy_partition() {
    return {169.1, 4.8, 7.0, 6.5, 8.8, 6.3};
}

// --- Double-humped fission barrier ------------------------------------------

// Actinide fission barriers have two humps (inner E_A, outer E_B) with a second
// minimum between them where shape isomers live. Returns whether a nucleus of
// fissility x shows a (positive) barrier at all.
inline double barrier_height_estimate(double fissility_x, double surface_energy_mev) {
    if (fissility_x >= 1.0)
        return 0.0;
    const double f = 1.0 - fissility_x;
    return surface_energy_mev * 0.38 * f * f * f; // vanishes smoothly as x -> 1
}

// --- Reactor physics: criticality -------------------------------------------

// Four-factor formula k_inf = eta * epsilon * p * f for an infinite medium:
//   eta = neutrons per absorption in fuel, epsilon = fast-fission factor,
//   p = resonance-escape probability, f = thermal utilisation.
inline double four_factor(double eta, double epsilon, double p, double f) {
    return eta * epsilon * p * f;
}

// Six-factor formula: k_eff = k_inf * P_FNL * P_TNL (fast & thermal non-leakage).
inline double six_factor(double k_inf, double P_fast_nonleak, double P_thermal_nonleak) {
    return k_inf * P_fast_nonleak * P_thermal_nonleak;
}

enum class Criticality { Subcritical, Critical, Supercritical };

inline Criticality classify_criticality(double k_eff, double tol = 1e-3) {
    if (k_eff < 1.0 - tol)
        return Criticality::Subcritical;
    if (k_eff > 1.0 + tol)
        return Criticality::Supercritical;
    return Criticality::Critical;
}

// Reactivity rho = (k_eff - 1) / k_eff, the fractional departure from critical.
inline double reactivity(double k_eff) {
    if (k_eff <= 0.0)
        return -INFINITY;
    return (k_eff - 1.0) / k_eff;
}

// Reproduction factor eta = nu * sigma_f / (sigma_f + sigma_gamma): fission
// neutrons produced per neutron absorbed in the fuel.
inline double reproduction_factor(double nu_bar, double sigma_fission, double sigma_capture) {
    const double denom = sigma_fission + sigma_capture;
    return denom > 0.0 ? nu_bar * sigma_fission / denom : 0.0;
}

} // namespace fission
} // namespace cosmos

#endif // COSMOS_FISSIONPHYSICS_HPP
