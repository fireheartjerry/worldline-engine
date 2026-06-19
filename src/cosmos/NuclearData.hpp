// NuclearData.hpp -- the quantitative backbone of the NUCLEAR tier: nuclear
// masses and radii, the extended semi-empirical mass formula (liquid drop +
// pairing + Wigner), binding and separation energies, the valley of beta
// stability, and the neutron/proton drip lines. Builds on the Bethe-Weizsaecker
// coefficients already in ParticleData.hpp and turns them into the full machinery
// the rest of the nuclear modules (decay, reactions, nucleosynthesis) stand on.
//
// Header-only, pure, deterministic. Energies in MeV, lengths in fm.
//
// Sources:
//   - Bethe-Weizsaecker SEMF coefficients: see cosmos/ParticleData.hpp.
//   - Nuclear radius R = r0 A^(1/3), r0 ~ 1.2 fm.
//   - Masses: m_p = 938.272, m_n = 939.565, m_e = 0.511 MeV (CODATA/PDG).
//   - Valley of stability Z*(A) = A / (1.98 + 0.0155 A^(2/3)) (SEMF minimisation).

#ifndef COSMOS_NUCLEARDATA_HPP
#define COSMOS_NUCLEARDATA_HPP

#include "cosmos/ParticleData.hpp"

#include <cmath>

namespace cosmos {
namespace nuclear {

// --- Fundamental masses and constants (MeV, fm) -----------------------------
inline constexpr double kProtonMass_mev = 938.27208816;
inline constexpr double kNeutronMass_mev = 939.56542052;
inline constexpr double kElectronMass_mev = 0.51099895;
inline constexpr double kAmu_mev = 931.49410242;   // atomic mass unit
inline constexpr double kR0_fm = 1.2;              // nuclear radius constant
inline constexpr double kCoulomb_mev_fm = 1.43996; // e^2 / (4 pi eps0) [MeV*fm]
// The alpha particle (He-4) is doubly magic, so the smooth SEMF badly underbinds
// it; alpha energetics use the measured binding instead.
inline constexpr double kAlphaBinding_mev = 28.295674;

// Neutron-proton-electron mass balance: m_n - m_p - m_e = 0.782 MeV (the energy
// freed in free-neutron beta decay).
inline constexpr double kBetaBalance_mev = kNeutronMass_mev - kProtonMass_mev - kElectronMass_mev;

// --- Nuclear size -----------------------------------------------------------

// Nuclear radius R = r0 A^(1/3). [fm]
inline double nuclear_radius_fm(int A) {
    if (A <= 0)
        return 0.0;
    return kR0_fm * std::cbrt(static_cast<double>(A));
}

inline constexpr double kPi = 3.14159265358979323846;

// Nuclear (saturation) density is roughly A-independent: rho = A / (4/3 pi R^3).
// Returns nucleons per fm^3 (~0.138, the nuclear saturation density).
inline double nucleon_density_per_fm3(int A) {
    const double R = nuclear_radius_fm(A);
    if (R <= 0.0)
        return 0.0;
    return A / ((4.0 / 3.0) * kPi * R * R * R);
}

// --- Binding energy (extended SEMF) -----------------------------------------

// Total nuclear binding energy from the Bethe-Weizsaecker formula plus a small
// Wigner term -|a_W| * |A - 2Z| / A that sharpens the N=Z preference. [MeV]
inline constexpr double kSemf_aW = 10.0; // Wigner coefficient (textbook ~10-47 MeV; modest here)

inline double binding_energy_mev(int Z, int A) {
    if (A <= 0 || Z < 0 || Z > A)
        return 0.0;
    const double base = particles::semf_binding_mev(Z, A);
    const double wigner = -kSemf_aW * std::abs(static_cast<double>(A - 2 * Z)) / A;
    return base + wigner;
}

inline double binding_per_nucleon_mev(int Z, int A) {
    if (A <= 0)
        return 0.0;
    return binding_energy_mev(Z, A) / A;
}

// --- Nuclear mass and mass excess -------------------------------------------

// Ground-state nuclear mass M = Z m_p + N m_n - B(Z,A). [MeV]
inline double nuclear_mass_mev(int Z, int A) {
    const int N = A - Z;
    return Z * kProtonMass_mev + N * kNeutronMass_mev - binding_energy_mev(Z, A);
}

// Mass excess Delta = M(Z,A) - A * u. [MeV]
inline double mass_excess_mev(int Z, int A) {
    return nuclear_mass_mev(Z, A) - A * kAmu_mev;
}

// --- Separation energies ----------------------------------------------------

// One-neutron separation energy S_n = B(Z,A) - B(Z,A-1). [MeV]
inline double neutron_separation_mev(int Z, int A) {
    return binding_energy_mev(Z, A) - binding_energy_mev(Z, A - 1);
}

// One-proton separation energy S_p = B(Z,A) - B(Z-1,A-1). [MeV]
inline double proton_separation_mev(int Z, int A) {
    return binding_energy_mev(Z, A) - binding_energy_mev(Z - 1, A - 1);
}

// Alpha separation energy S_alpha = B(Z,A) - B(Z-2,A-4) - B(alpha). Positive =>
// bound against alpha emission (stable); negative => alpha-unstable (Q_alpha>0).
inline double alpha_separation_mev(int Z, int A) {
    return binding_energy_mev(Z, A) - binding_energy_mev(Z - 2, A - 4) - kAlphaBinding_mev;
}

// --- Valley of beta stability -----------------------------------------------

// Analytic most-stable charge for mass number A (SEMF minimisation):
//   Z*(A) = A / (1.98 + 0.0155 A^(2/3)).
inline double valley_Z_real(int A) {
    const double Ad = static_cast<double>(A);
    return Ad / (1.98 + 0.0155 * std::cbrt(Ad * Ad));
}

// The integer Z that maximises binding for fixed A, found by scanning (the true
// SEMF valley floor). Robust against the analytic approximation.
inline int most_stable_Z(int A) {
    int bestZ = 1;
    double best = -1e30;
    for (int Z = 1; Z < A; ++Z) {
        const double b = binding_energy_mev(Z, A);
        if (b > best) {
            best = b;
            bestZ = Z;
        }
    }
    return bestZ;
}

// --- Drip lines -------------------------------------------------------------

// A nucleus is beyond the neutron drip line when the last neutron is unbound
// (S_n < 0): it cannot hold another neutron.
inline bool beyond_neutron_drip(int Z, int A) {
    return neutron_separation_mev(Z, A) < 0.0;
}

// Beyond the proton drip line when the last proton is unbound (S_p < 0).
inline bool beyond_proton_drip(int Z, int A) {
    return proton_separation_mev(Z, A) < 0.0;
}

} // namespace nuclear
} // namespace cosmos

#endif // COSMOS_NUCLEARDATA_HPP
