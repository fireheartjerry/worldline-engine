// Verifies cosmos/NuclearMatter.hpp: the saturation point and binding of
// symmetric nuclear matter, the symmetry-energy cost of asymmetry, the equation
// of state and its pressure, beta-equilibrium neutronisation, and the neutron-
// star end-points.

#include "cosmos/NuclearMatter.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::nsmatter;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nuclearmatter_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_nuclearmatter_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Saturation properties ----------------------------------------------
    close(kSatDensity_fm3, 0.16, 1e-9, "saturation density n0 = 0.16/fm^3");
    close(kSatBinding_mev, -16.0, 1e-9, "saturation binding -16 MeV/nucleon");
    check(kSymmetryEnergy_mev > 28.0 && kSymmetryEnergy_mev < 36.0, "symmetry energy ~ 32 MeV");
    check(kIncompressibility_mev > 200.0 && kIncompressibility_mev < 280.0, "K ~ 240 MeV");

    // --- Equation of state --------------------------------------------------
    // Symmetric matter E/A is minimised at saturation, with value E_sat.
    close(symmetric_energy_per_nucleon(kSatDensity_fm3), kSatBinding_mev, 1e-9,
          "E/A minimum at saturation = -16 MeV");
    check(symmetric_energy_per_nucleon(0.5 * kSatDensity_fm3) > kSatBinding_mev,
          "below saturation costs energy");
    check(symmetric_energy_per_nucleon(1.5 * kSatDensity_fm3) > kSatBinding_mev,
          "above saturation costs energy (incompressibility)");

    // Symmetry term: zero for symmetric matter (x=0.5), positive and largest for
    // pure neutron matter (x=0).
    close(symmetry_term(kSatDensity_fm3, 0.5), 0.0, 1e-9, "no symmetry cost at x=1/2");
    close(symmetry_term(kSatDensity_fm3, 0.0), kSymmetryEnergy_mev, 1e-9,
          "pure neutron matter pays full symmetry energy");
    check(energy_per_nucleon(kSatDensity_fm3, 0.0) > energy_per_nucleon(kSatDensity_fm3, 0.5),
          "neutron matter less bound than symmetric");

    // Pressure: ~zero at saturation for symmetric matter, positive above it.
    check(std::abs(pressure(kSatDensity_fm3, 0.5)) < 1e-3, "pressure ~ 0 at saturation");
    check(pressure(2.0 * kSatDensity_fm3, 0.5) > 0.0, "compressed matter has positive pressure");

    // --- Neutronisation -----------------------------------------------------
    // Beta-equilibrium proton fraction is small and rises with density.
    check(equilibrium_proton_fraction(kSatDensity_fm3) < 0.1, "few protons in beta equilibrium");
    check(equilibrium_proton_fraction(4.0 * kSatDensity_fm3) >
              equilibrium_proton_fraction(kSatDensity_fm3),
          "proton fraction rises with density");
    check(kNeutronDrip_g_cm3 > 1e11 && kNeutronDrip_g_cm3 < 1e12, "neutron drip ~ 4e11 g/cm^3");

    // --- Neutron-star end-points --------------------------------------------
    check(kTypicalRadius_km > 10.0 && kTypicalRadius_km < 14.0, "neutron star radius ~ 12 km");
    check(kMaxMass_Msun > 2.0, "TOV maximum mass above 2 Msun");
    check(kCentralDensity_n0 > 1.0, "central density several times saturation");
    // A 1.4 Msun star is a ~10^57-nucleon object.
    check(nucleon_count(1.4) > 1e57 && nucleon_count(1.4) < 1e58, "~10^57 nucleons in a NS");
    check(nucleon_count(2.0) > nucleon_count(1.4), "more massive star has more nucleons");

    // --- Determinism --------------------------------------------------------
    check(energy_per_nucleon(0.2, 0.3) == energy_per_nucleon(0.2, 0.3), "EOS deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nuclearmatter_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nuclearmatter_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
