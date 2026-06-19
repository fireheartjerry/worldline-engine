// QEDScattering.hpp -- the scattering cross sections of quantum electrodynamics:
// the classical electron radius and Thomson limit, the Klein-Nishina formula for
// Compton scattering (and the Compton wavelength shift), Rutherford and Mott
// Coulomb scattering, Mandelstam kinematics, and the relativistic Breit-Wigner
// resonance line shape. This is "how particles actually interact" at the first
// layer -- the QED vertices made quantitative.
//
// Header-only, pure, deterministic. SI for cross sections (m^2); MeV for the
// photon/particle energies that the particle world prefers.
//
// Sources:
//   - Classical electron radius r_e = alpha * lambdabar_C(e) = 2.8179e-15 m.
//   - Thomson cross section sigma_T = (8 pi / 3) r_e^2 = 6.652e-29 m^2.
//   - Klein-Nishina (1929) total Compton cross section.
//   - Compton (1923) wavelength shift d_lambda = (h / m_e c)(1 - cos theta).
//   - Rutherford (1911) and Mott (1929) differential Coulomb cross sections.
//   - Relativistic Breit-Wigner resonance line shape.

#ifndef COSMOS_QEDSCATTERING_HPP
#define COSMOS_QEDSCATTERING_HPP

#include "cosmos/Constants.hpp"
#include "cosmos/QuantumScale.hpp"

#include <cmath>

namespace cosmos {
namespace qed {

using namespace cosmos::constants;

// Electron rest energy in MeV, used as the natural scale of Compton scattering.
inline double electron_rest_mev() {
    return quantum::rest_energy_mev(electron_mass_kg);
}

// ---------------------------------------------------------------------------
// Classical electron radius and the Thomson limit
// ---------------------------------------------------------------------------

// r_e = e^2 / (4 pi eps0 m_e c^2) = alpha * (hbar / m_e c). ~2.818e-15 m.
inline double classical_electron_radius_m() {
    return alpha * hbar / (electron_mass_kg * c);
}

// Thomson cross section sigma_T = (8 pi / 3) r_e^2. The low-energy (Compton ->
// Thomson) limit of photon-electron scattering. ~6.652e-29 m^2.
inline double thomson_cross_section_m2() {
    const double re = classical_electron_radius_m();
    return (8.0 * pi / 3.0) * re * re;
}

// ---------------------------------------------------------------------------
// Compton scattering
// ---------------------------------------------------------------------------

// Compton wavelength shift d_lambda = lambda_C (1 - cos theta), peaking at 2
// lambda_C for back-scatter (theta = pi). [m]
inline double compton_shift_m(double theta_rad) {
    const double lambdaC = quantum::compton_wavelength_m(electron_mass_kg);
    return lambdaC * (1.0 - std::cos(theta_rad));
}

// Scattered-photon energy after Compton scattering off a free electron:
//   E' = E / (1 + (E / m_e c^2)(1 - cos theta)). [MeV in, MeV out]
inline double compton_scattered_energy_mev(double E_mev, double theta_rad) {
    const double eps = E_mev / electron_rest_mev();
    return E_mev / (1.0 + eps * (1.0 - std::cos(theta_rad)));
}

// Klein-Nishina TOTAL cross section as a function of the photon energy. Reduces
// to sigma_T as E -> 0 and falls (roughly ~ ln(E)/E) at high energy. [m^2]
inline double klein_nishina_total_m2(double E_mev) {
    const double re = classical_electron_radius_m();
    const double eps = E_mev / electron_rest_mev();
    if (eps < 1e-9)
        return thomson_cross_section_m2();
    const double a = 1.0 + 2.0 * eps;
    const double term1 = (1.0 + eps) / (eps * eps) * (2.0 * (1.0 + eps) / a - std::log(a) / eps);
    const double term2 = std::log(a) / (2.0 * eps);
    const double term3 = (1.0 + 3.0 * eps) / (a * a);
    return 2.0 * pi * re * re * (term1 + term2 - term3);
}

// ---------------------------------------------------------------------------
// Coulomb scattering: Rutherford and Mott
// ---------------------------------------------------------------------------

// Rutherford differential cross section dsigma/dOmega for a projectile of charge
// Z1 on a target of charge Z2 with kinetic energy E (J), at angle theta:
//   dsigma/dOmega = (Z1 Z2 alpha hbar c / (4 E))^2 / sin^4(theta/2). [m^2/sr]
inline double rutherford_dcs(int Z1, int Z2, double E_J, double theta_rad) {
    const double s = std::sin(theta_rad / 2.0);
    if (s <= 0.0)
        return INFINITY; // forward divergence
    const double coulomb = static_cast<double>(Z1 * Z2) * alpha * hbar * c;
    const double amp = coulomb / (4.0 * E_J);
    return amp * amp / (s * s * s * s);
}

// Mott correction factor for a relativistic spin-1/2 projectile:
//   dsigma_Mott = dsigma_Rutherford * (1 - beta^2 sin^2(theta/2)). In [0,1].
inline double mott_factor(double beta, double theta_rad) {
    const double s = std::sin(theta_rad / 2.0);
    return 1.0 - beta * beta * s * s;
}

// ---------------------------------------------------------------------------
// Relativistic kinematics: Mandelstam invariants
// ---------------------------------------------------------------------------

// For a 2 -> 2 process the Mandelstam invariants satisfy s + t + u = sum m_i^2.
// This helper returns that invariant sum (in MeV^2) for a consistency check.
inline double mandelstam_sum_mev2(double m1, double m2, double m3, double m4) {
    return m1 * m1 + m2 * m2 + m3 * m3 + m4 * m4;
}

// ---------------------------------------------------------------------------
// Resonances: relativistic Breit-Wigner line shape
// ---------------------------------------------------------------------------

// Normalised Breit-Wigner line shape (peak = 1 at E = M), FWHM = Gamma:
//   f(E) = (Gamma/2)^2 / ((E - M)^2 + (Gamma/2)^2).
inline double breit_wigner(double E, double M, double Gamma) {
    const double half = Gamma / 2.0;
    const double d = E - M;
    return (half * half) / (d * d + half * half);
}

// Relativistic Breit-Wigner cross-section shape ~ 1/((s - M^2)^2 + M^2 Gamma^2),
// peaking at the invariant mass s = M^2. Returns the (unnormalised) shape factor.
inline double relativistic_breit_wigner(double s, double M, double Gamma) {
    const double d = s - M * M;
    return 1.0 / (d * d + M * M * Gamma * Gamma);
}

} // namespace qed
} // namespace cosmos

#endif // COSMOS_QEDSCATTERING_HPP
