// NuclearStructure.hpp -- collective nuclear structure, the physics beyond the
// single-particle shell model: quadrupole deformation, rotational bands and
// their moments of inertia, vibrational phonon spectra, the rotor-vs-vibrator
// signature R_4/2, and the giant dipole resonance. This is how whole nuclei
// rotate, vibrate and deform as coherent bodies.
//
// Header-only, pure, deterministic. Energies in MeV (or keV where noted),
// lengths in fm; hbar c = 197.327 MeV*fm.
//
// Sources:
//   - Bohr & Mottelson, "Nuclear Structure" (collective model).
//   - Rotational band E(J) = (hbar^2 / 2I) J(J+1); R_4/2 = 10/3 (rotor), 2 (vibrator).
//   - Giant dipole resonance E_GDR ~ 79 A^(-1/3) MeV (Goldhaber-Teller / Steinwedel-Jensen).

#ifndef COSMOS_NUCLEARSTRUCTURE_HPP
#define COSMOS_NUCLEARSTRUCTURE_HPP

#include <cmath>

namespace cosmos {
namespace structure {

inline constexpr double kHbarC_MeV_fm = 197.3269804;
inline constexpr double kAmu_MeV = 931.49410242;

// --- Rotational bands -------------------------------------------------------

// Energy of the J member of a ground-state rotational band, anchored to the
// measured 2+ energy: E(J) = E(2+) * J(J+1) / 6 (since 2*3 = 6). [same units as E2]
inline double rotational_energy(int J, double E2plus) {
    if (J < 0)
        return 0.0;
    return E2plus * (static_cast<double>(J) * (J + 1)) / 6.0;
}

// The rotational constant A = hbar^2 / 2I for a rigid-body moment of inertia
// I = (2/5) M R^2 (M = A_mass * u, R = 1.2 A_mass^(1/3) fm). [MeV]
inline double rotational_constant_mev(int A_mass) {
    if (A_mass <= 0)
        return 0.0;
    const double M = A_mass * kAmu_MeV;               // MeV/c^2
    const double R = 1.2 * std::cbrt((double)A_mass); // fm
    const double I_rigid = (2.0 / 5.0) * M * R * R;   // MeV*fm^2 (c=1)
    return kHbarC_MeV_fm * kHbarC_MeV_fm / (2.0 * I_rigid);
}

// The classic rotor signature: E(4+)/E(2+) = 20/6 = 10/3 ~ 3.33.
inline double R42_rotor() {
    return 10.0 / 3.0;
}
// A harmonic quadrupole vibrator instead gives E(4+)/E(2+) = 2 (two-phonon).
inline double R42_vibrator() {
    return 2.0;
}

enum class Collective { Vibrational, Transitional, Rotational };

// Classify a nucleus from its measured R_4/2 ratio.
inline Collective classify_collective(double R42) {
    if (R42 > 3.0)
        return Collective::Rotational;
    if (R42 < 2.4)
        return Collective::Vibrational;
    return Collective::Transitional;
}

// --- Vibrational spectra ----------------------------------------------------

// Energy of an n-phonon state of a harmonic vibrator: E_n = n * hbar_omega.
inline double vibrational_energy(int n_phonons, double hbar_omega) {
    if (n_phonons < 0)
        return 0.0;
    return n_phonons * hbar_omega;
}

// --- Quadrupole deformation -------------------------------------------------

// Intrinsic quadrupole moment for a uniformly-charged spheroid of deformation
// beta2: Q0 = (3 / sqrt(5 pi)) Z R^2 beta2 (1 + ...). [e*fm^2]
inline double intrinsic_quadrupole(int Z, int A_mass, double beta2) {
    const double R = 1.2 * std::cbrt((double)A_mass);
    return (3.0 / std::sqrt(5.0 * 3.14159265358979323846)) * Z * R * R * beta2;
}

inline bool is_prolate(double beta2) {
    return beta2 > 0.0;
}
inline bool is_oblate(double beta2) {
    return beta2 < 0.0;
}
inline bool is_spherical(double beta2) {
    return std::abs(beta2) < 1e-3;
}

// --- Giant dipole resonance -------------------------------------------------

// The GDR centroid energy, where protons and neutrons oscillate against each
// other: E_GDR ~ 79 A^(-1/3) MeV (Pb-208 -> ~13.3 MeV). [MeV]
inline double giant_dipole_energy(int A_mass) {
    if (A_mass <= 0)
        return 0.0;
    return 79.0 / std::cbrt((double)A_mass);
}

// Energy-weighted (Thomas-Reiche-Kuhn) dipole sum rule: ~60 N Z / A MeV*mb. The
// total integrated photoabsorption strength. [MeV*mb]
inline double trk_sum_rule(int Z, int A_mass) {
    if (A_mass <= 0)
        return 0.0;
    const int N = A_mass - Z;
    return 60.0 * static_cast<double>(N) * Z / A_mass;
}

} // namespace structure
} // namespace cosmos

#endif // COSMOS_NUCLEARSTRUCTURE_HPP
