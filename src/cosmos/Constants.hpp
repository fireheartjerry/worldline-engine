#pragma once
// Real physical constants (SI). Used for DISPLAY-side computed physics only —
// the dimensionless N-body sandbox dynamics are unaffected.
//
// Values are CODATA-2022 / PDG-2024 (see Appendix B of the design plan). Post-
// 2019-SI, c/h/e/k_B are exact by definition; G and alpha carry the largest
// uncertainties. Planck units are the hard floor of the SUBATOMIC scale tier:
// below them classical spacetime is not meaningful, so they bound the smallest
// scale a deterministic generator should ever sample.

namespace cosmos {
namespace constants {

// ── Fundamental constants (SI) ──────────────────────────────────────────────
constexpr double G = 6.67430e-11;         // gravitational constant, m^3 kg^-1 s^-2
constexpr double c = 2.99792458e8;        // speed of light, m/s (exact)
constexpr double c2 = c * c;              // c^2, m^2/s^2
constexpr double kB = 1.380649e-23;       // Boltzmann constant, J/K (exact)
constexpr double h = 6.62607015e-34;      // Planck constant, J s (exact)
constexpr double hbar = 1.054571817e-34;  // reduced Planck constant, J s
constexpr double e = 1.602176634e-19;     // elementary charge, C (exact)
constexpr double alpha = 7.2973525643e-3; // fine-structure constant (~1/137.036)
constexpr double pi = 3.14159265358979323846;

// ── Planck units (the floor of scale) ───────────────────────────────────────
// Defining formulas: l_P=sqrt(hbar*G/c^3); t_P=l_P/c; m_P=sqrt(hbar*c/G);
// E_P=m_P*c^2; T_P=E_P/kB. We store the CODATA-2022 values directly (std::sqrt is
// not constexpr in C++17); cosmos_physics_verification re-derives them from the
// formulas above to guarantee self-consistency.
constexpr double planck_length_m = 1.616255e-35; // m
constexpr double planck_time_s = 5.391247e-44;   // s
constexpr double planck_mass_kg = 2.176434e-8;   // kg
constexpr double planck_energy_J = 1.956114e9;   // J  (= 1.220910e19 GeV)
constexpr double planck_temp_K = 1.416784e32;    // K

// ── Elementary rest masses (SI) ─────────────────────────────────────────────
// CODATA 2018/2022. The lightest stable matter particles — the rulers of the
// quantum tier: every Compton wavelength, Bohr radius and binding energy below
// is set by these three numbers.
constexpr double electron_mass_kg = 9.1093837015e-31; // m_e
constexpr double proton_mass_kg = 1.67262192369e-27;  // m_p
constexpr double neutron_mass_kg = 1.67492749804e-27; // m_n  (m_n > m_p: free n decays)

// ── Atomic-scale anchors (SI) ───────────────────────────────────────────────
// Derived from m_e, c and alpha; stored here for the same reason the Planck
// units are (constexpr can't call std::sqrt in C++17). The quantum verification
// re-derives them from a_0 = hbar/(m_e c alpha) and Ry = alpha^2 m_e c^2 / 2.
constexpr double electron_volt_J = 1.602176634e-19;      // 1 eV (exact; = e)
constexpr double bohr_radius_m = 5.29177210903e-11;      // a_0
constexpr double rydberg_energy_J = 2.1798723611035e-18; // Ry (= 13.605693 eV)

// ── Dimensionless ratios that characterize a universe ───────────────────────
constexpr double proton_electron_mass_ratio = 1836.152673;
constexpr double alpha_inv = 137.035999;               // 1/alpha
constexpr double grav_coupling_electron = 1.75181e-45; // alpha_G = G*m_e^2/(hbar*c)

// Relative coupling strengths (proton-scale convention; order-of-magnitude, the
// couplings "run" with energy). strong : EM : weak : gravity.
constexpr double strength_strong = 1.0;
constexpr double strength_em = 1.0e-2; // ~ alpha
constexpr double strength_weak = 1.0e-6;
constexpr double strength_gravity = 1.0e-38;

} // namespace constants
} // namespace cosmos
