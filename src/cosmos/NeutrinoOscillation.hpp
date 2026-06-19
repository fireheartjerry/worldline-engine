// NeutrinoOscillation.hpp -- neutrinos change flavour as they fly, the only
// laboratory proof that neutrinos have mass and the cleanest macroscopic quantum
// interference in the Standard Model. Two- and three-flavour oscillation
// probabilities, the PMNS mixing angles and mass-squared splittings, oscillation
// lengths, and the matter (MSW) resonance. A quantum-coherence phenomenon of the
// first layer that stretches over hundreds of kilometres.
//
// Header-only, pure, deterministic. Standard experimental units: L in km, E in
// GeV, dm^2 in eV^2 (the famous 1.267 factor packages hbar, c and the unit
// conversions).
//
// Sources (NuFIT 5.2 / PDG 2024, normal ordering):
//   - Oscillation phase 1.267 * dm^2[eV^2] * L[km] / E[GeV].
//   - Mixing angles: theta12 ~ 33.4 deg, theta23 ~ 49 deg, theta13 ~ 8.6 deg.
//   - Splittings: dm21^2 ~ 7.42e-5 eV^2, dm31^2 ~ 2.51e-3 eV^2.
//   - Mikheyev-Smirnov-Wolfenstein matter resonance.

#ifndef COSMOS_NEUTRINOOSCILLATION_HPP
#define COSMOS_NEUTRINOOSCILLATION_HPP

#include <cmath>

namespace cosmos {
namespace nu {

inline constexpr double kPi = 3.14159265358979323846;

// The kinematic phase factor: phase = kOscFactor * dm2[eV^2] * L[km] / E[GeV].
// kOscFactor = 1.267 (= 1/(4 hbar c) in these mixed units).
inline constexpr double kOscFactor = 1.267;

// --- PMNS mixing angles (radians) and mass-squared splittings (eV^2) ----------
inline constexpr double kTheta12 = 0.5829;   // ~33.4 deg (solar)
inline constexpr double kTheta23 = 0.8552;   // ~49.0 deg (atmospheric)
inline constexpr double kTheta13 = 0.1501;   // ~8.6 deg (reactor)
inline constexpr double kDm21_eV2 = 7.42e-5; // solar splitting
inline constexpr double kDm31_eV2 = 2.51e-3; // atmospheric splitting

// ---------------------------------------------------------------------------
// Two-flavour oscillation
// ---------------------------------------------------------------------------

// Transition probability P(nu_a -> nu_b), a != b, in vacuum:
//   P = sin^2(2 theta) sin^2(1.267 dm2 L / E).
inline double transition_prob(double theta, double dm2_ev2, double L_km, double E_gev) {
    if (E_gev <= 0.0)
        return 0.0;
    const double s2t = std::sin(2.0 * theta);
    const double phase = kOscFactor * dm2_ev2 * L_km / E_gev;
    const double sp = std::sin(phase);
    return s2t * s2t * sp * sp;
}

// Survival probability P(nu_a -> nu_a) = 1 - P(transition). Two-flavour unitarity.
inline double survival_prob(double theta, double dm2_ev2, double L_km, double E_gev) {
    return 1.0 - transition_prob(theta, dm2_ev2, L_km, E_gev);
}

// Oscillation length (one full 2 pi cycle of the phase):
//   L_osc = pi E / (1.267 dm2) [km], for E in GeV and dm2 in eV^2.
inline double oscillation_length_km(double dm2_ev2, double E_gev) {
    if (dm2_ev2 <= 0.0)
        return INFINITY;
    return kPi * E_gev / (kOscFactor * dm2_ev2);
}

// Distance of the FIRST oscillation maximum (phase = pi/2): half an osc. length.
inline double first_maximum_km(double dm2_ev2, double E_gev) {
    return 0.5 * oscillation_length_km(dm2_ev2, E_gev);
}

// ---------------------------------------------------------------------------
// Three-flavour PMNS amplitudes
// ---------------------------------------------------------------------------

// |U_e2|^2 = cos^2(theta13) sin^2(theta12): the electron-neutrino content of the
// second mass eigenstate (drives the solar deficit). Part of a unitary row.
inline double Ue2_sq() {
    const double c13 = std::cos(kTheta13);
    const double s12 = std::sin(kTheta12);
    return c13 * c13 * s12 * s12;
}
inline double Ue1_sq() {
    const double c13 = std::cos(kTheta13);
    const double c12 = std::cos(kTheta12);
    return c13 * c13 * c12 * c12;
}
inline double Ue3_sq() {
    const double s13 = std::sin(kTheta13);
    return s13 * s13;
}

// The electron row of |U|^2 is unitary: |U_e1|^2 + |U_e2|^2 + |U_e3|^2 = 1.
inline double electron_row_sum() {
    return Ue1_sq() + Ue2_sq() + Ue3_sq();
}

// ---------------------------------------------------------------------------
// Matter effects (MSW)
// ---------------------------------------------------------------------------

// MSW resonance condition: the matter term equals the vacuum oscillation term,
//   resonant density ~ dm2 cos(2 theta) / (2 sqrt2 G_F E).
// We return the dimensionless resonance factor cos(2 theta) (>0 for the normal
// hierarchy in the solar sector) -- positive means a matter resonance exists for
// neutrinos (rather than antineutrinos).
inline double msw_resonance_sign(double theta) {
    return std::cos(2.0 * theta);
}

} // namespace nu
} // namespace cosmos

#endif // COSMOS_NEUTRINOOSCILLATION_HPP
