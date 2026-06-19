// NuclearMatter.hpp -- nuclear matter in bulk and its most extreme realisation,
// the neutron star. The saturation point and binding of symmetric nuclear matter,
// the nuclear incompressibility, the symmetry energy that costs you to make
// matter neutron-rich, the equation of state and its pressure, beta-equilibrium
// neutronisation, and the neutron-star mass-radius end-points. This is the
// nuclear force scaled up to a 20-km nucleus held together by gravity.
//
// Header-only, pure, deterministic. Densities in nucleons/fm^3, energies in MeV.
//
// Sources:
//   - Nuclear saturation n0 ~ 0.16 /fm^3, E/A ~ -16 MeV, K ~ 240 MeV.
//   - Symmetry energy S0 ~ 32 MeV (energy cost of proton-neutron asymmetry).
//   - Observed neutron-star maximum mass ~2 Msun, radius ~12 km (NICER / GW170817).

#ifndef COSMOS_NUCLEARMATTER_HPP
#define COSMOS_NUCLEARMATTER_HPP

#include <cmath>

namespace cosmos {
namespace nsmatter {

// --- Saturation properties of symmetric nuclear matter ----------------------
inline constexpr double kSatDensity_fm3 = 0.16;         // n0 [1/fm^3]
inline constexpr double kSatDensity_kg_m3 = 2.7e17;     // ~ nuclear density
inline constexpr double kSatBinding_mev = -16.0;        // E/A at saturation
inline constexpr double kIncompressibility_mev = 240.0; // K
inline constexpr double kSymmetryEnergy_mev = 32.0;     // S0
inline constexpr double kSymmetrySlope_mev = 60.0;      // L

// --- Equation of state ------------------------------------------------------

// Energy per nucleon of symmetric matter near saturation (parabolic in density):
//   E/A(n) = E_sat + (K/18) ((n - n0)/n0)^2.  Minimised at n0 with value E_sat.
inline double symmetric_energy_per_nucleon(double n_fm3) {
    const double dn = (n_fm3 - kSatDensity_fm3) / kSatDensity_fm3;
    return kSatBinding_mev + (kIncompressibility_mev / 18.0) * dn * dn;
}

// Symmetry-energy term added for a proton fraction x (isospin asymmetry):
//   E_sym(n, x) = S(n) (1 - 2x)^2, with a simple S(n) = S0 (n/n0)^(2/3).
inline double symmetry_term(double n_fm3, double x_proton) {
    const double S = kSymmetryEnergy_mev * std::pow(n_fm3 / kSatDensity_fm3, 2.0 / 3.0);
    const double delta = 1.0 - 2.0 * x_proton;
    return S * delta * delta;
}

// Total energy per nucleon E/A(n, x). [MeV]
inline double energy_per_nucleon(double n_fm3, double x_proton) {
    return symmetric_energy_per_nucleon(n_fm3) + symmetry_term(n_fm3, x_proton);
}

// Pressure P = n^2 d(E/A)/dn (numerical derivative). [MeV/fm^3]
inline double pressure(double n_fm3, double x_proton) {
    const double h = 1e-5;
    const double dEdn =
        (energy_per_nucleon(n_fm3 + h, x_proton) - energy_per_nucleon(n_fm3 - h, x_proton)) /
        (2.0 * h);
    return n_fm3 * n_fm3 * dEdn;
}

// --- Beta equilibrium / neutronisation --------------------------------------

// In beta equilibrium the proton fraction is small and set by the symmetry
// energy: pure neutron matter has x ~ 0, with a few percent protons. We return a
// schematic equilibrium proton fraction that rises gently with density.
inline double equilibrium_proton_fraction(double n_fm3) {
    // Crude: x grows with the symmetry energy's density dependence, capped low.
    const double x = 0.04 * std::pow(n_fm3 / kSatDensity_fm3, 0.5);
    return x < 0.0 ? 0.0 : (x > 0.5 ? 0.5 : x);
}

// Neutron-drip density: above ~4e11 g/cm^3 neutrons drip out of nuclei into a
// free neutron gas (the inner crust of a neutron star). [g/cm^3]
inline constexpr double kNeutronDrip_g_cm3 = 4.3e11;

// --- Neutron-star end-points ------------------------------------------------

// Observed/inferred bulk numbers for a canonical neutron star.
inline constexpr double kTypicalMass_Msun = 1.4;
inline constexpr double kTypicalRadius_km = 12.0;
inline constexpr double kMaxMass_Msun = 2.2;      // TOV maximum (stiff EOS, observed ~2)
inline constexpr double kCentralDensity_n0 = 5.0; // ~ several times saturation

// A neutron star is, in effect, a single nucleus of ~10^57 nucleons bound by
// gravity rather than the strong force. Returns that nucleon count for a given
// mass in solar masses (M_sun ~ 2e30 kg, m_n ~ 1.675e-27 kg).
inline double nucleon_count(double mass_Msun) {
    return mass_Msun * 1.989e30 / 1.6749e-27;
}

} // namespace nsmatter
} // namespace cosmos

#endif // COSMOS_NUCLEARMATTER_HPP
