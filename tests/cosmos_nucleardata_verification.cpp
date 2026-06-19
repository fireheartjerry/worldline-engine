// Verifies cosmos/NuclearData.hpp: nuclear radius/density, the extended SEMF
// binding (iron-group peak), separation energies, mass excess, the valley of
// stability, and the drip lines.

#include "cosmos/NuclearData.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::nuclear;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nucleardata_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_nucleardata_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Radius & density ---------------------------------------------------
    close(nuclear_radius_fm(216), 7.2, 1e-9, "R(A=216) = 1.2 * 6 = 7.2 fm");
    check(nuclear_radius_fm(238) > nuclear_radius_fm(56), "heavier nucleus is larger");
    // Saturation density is ~A-independent (~0.14 nucleons/fm^3).
    close(nucleon_density_per_fm3(56), nucleon_density_per_fm3(208), 1e-9,
          "nuclear density A-independent");
    close(nucleon_density_per_fm3(120), 0.138, 5e-2, "saturation density ~ 0.14/fm^3");

    // --- Binding energy -----------------------------------------------------
    // Iron-group peak of BE/A, around 8.7-8.8 MeV.
    const double bpnFe = binding_per_nucleon_mev(26, 56);
    check(bpnFe > 8.4 && bpnFe < 9.0, "BE/A(Fe-56) in [8.4,9.0] MeV");
    check(bpnFe > binding_per_nucleon_mev(2, 4), "BE/A: Fe-56 > He-4");
    check(bpnFe > binding_per_nucleon_mev(92, 238), "BE/A: Fe-56 > U-238");
    check(binding_energy_mev(26, 56) > 480.0, "total B(Fe-56) ~ 490 MeV");

    // --- Separation energies ------------------------------------------------
    // Separation energies are positive for bound nuclei near stability.
    check(neutron_separation_mev(26, 56) > 0.0, "S_n(Fe-56) > 0");
    check(proton_separation_mev(26, 56) > 0.0, "S_p(Fe-56) > 0");
    // Alpha separation energy: positive => bound against alpha (light, stable);
    // negative => alpha-unstable (heavy emitters).
    check(alpha_separation_mev(8, 16) > 0.0, "S_alpha(O-16) > 0 (alpha-stable)");
    check(alpha_separation_mev(92, 238) < 0.0, "S_alpha(U-238) < 0 (alpha-unstable)");

    // --- Mass & mass excess -------------------------------------------------
    // Nuclear mass is close to A atomic mass units; mass excess is a small offset.
    check(std::abs(mass_excess_mev(26, 56)) < 100.0, "mass excess Fe-56 is small");
    check(nuclear_mass_mev(26, 56) > 0.0, "nuclear mass positive");

    // --- Valley of stability ------------------------------------------------
    // The analytic and scanned valley agree, and land on the right elements.
    check(most_stable_Z(56) >= 24 && most_stable_Z(56) <= 28, "valley Z(56) ~ iron (26)");
    check(most_stable_Z(208) >= 80 && most_stable_Z(208) <= 84, "valley Z(208) ~ lead (82)");
    check(most_stable_Z(40) >= 18 && most_stable_Z(40) <= 20, "valley Z(40) ~ calcium (20)");
    close(valley_Z_real(208), 82.0, 5e-2, "analytic valley Z(208) ~ 82");
    // Light nuclei sit near N=Z (Z/A ~ 0.45-0.5); heavy nuclei are neutron-rich
    // (Z/A < 0.42). The proton fraction falls monotonically with mass.
    check(static_cast<double>(most_stable_Z(40)) / 40.0 > 0.44, "light: Z/A near 1/2");
    check(static_cast<double>(most_stable_Z(238)) / 238.0 < 0.42, "heavy: neutron-rich");
    check(static_cast<double>(most_stable_Z(40)) / 40.0 >
              static_cast<double>(most_stable_Z(238)) / 238.0,
          "proton fraction falls with mass");

    // --- Drip lines ---------------------------------------------------------
    // A wildly neutron-rich nucleus is beyond the neutron drip line.
    check(beyond_neutron_drip(8, 30), "very neutron-rich O is beyond the drip line");
    check(!beyond_neutron_drip(26, 56), "Fe-56 is bound against neutron emission");

    // --- Determinism --------------------------------------------------------
    check(binding_energy_mev(26, 56) == binding_energy_mev(26, 56), "binding deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nucleardata_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nucleardata_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
