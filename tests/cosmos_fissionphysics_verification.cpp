// Verifies cosmos/FissionPhysics.hpp: the asymmetric fragment mass split, prompt
// neutron multiplicity and delayed fractions, the ~200 MeV energy partition, the
// barrier estimate, and reactor criticality (four/six-factor, reactivity).

#include "cosmos/FissionPhysics.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::fission;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_fissionphysics_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_fissionphysics_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Fragment mass distribution -----------------------------------------
    // U-236* (U-235 + n) splits asymmetrically: heavy ~139, light ~95.
    const FragmentPeaks p = fragment_peaks(236, kNubar_U235);
    check(p.heavy == 139, "heavy fragment peak at A~139");
    check(p.light >= 92 && p.light <= 97, "light fragment peak at A~95");
    check(is_asymmetric(p), "low-energy actinide fission is asymmetric");
    // Fragments + prompt neutrons account for the compound mass.
    check(p.light + p.heavy + static_cast<int>(kNubar_U235 + 0.5) == 236, "mass balance");

    // --- Neutron emission ---------------------------------------------------
    close(kNubar_U235, 2.42, 1e-9, "U-235 nu-bar ~ 2.42");
    check(kNubar_Pu239 > kNubar_U235, "Pu-239 emits more neutrons than U-235");
    // Delayed fraction is small (sub-percent) and largest for U-238.
    check(kBeta_U235 < 0.01, "U-235 delayed fraction < 1%");
    check(kBeta_U238 > kBeta_Pu239, "U-238 delayed fraction > Pu-239");

    // --- Energy partition ---------------------------------------------------
    const EnergyPartition e = u235_energy_partition();
    check(e.total() > 195.0 && e.total() < 210.0, "total fission energy ~ 200 MeV");
    check(e.fragments_ke > 160.0, "fragment KE dominates (~169 MeV)");
    // The escaping antineutrinos are NOT recoverable as heat.
    check(e.recoverable() < e.total(), "neutrinos reduce recoverable heat");
    close(e.total() - e.recoverable(), e.antineutrinos, 1e-9, "lost energy = neutrino energy");

    // --- Fission barrier ----------------------------------------------------
    // A barrier exists below the fissility limit and vanishes as x -> 1.
    check(barrier_height_estimate(0.7, 600.0) > 0.0, "barrier positive below x=1");
    check(barrier_height_estimate(0.95, 600.0) < barrier_height_estimate(0.7, 600.0),
          "barrier shrinks as fissility rises");
    check(barrier_height_estimate(1.0, 600.0) == 0.0, "no barrier at x=1");

    // --- Reactor criticality ------------------------------------------------
    // Four-factor product; a balanced reactor sits near k=1.
    close(four_factor(2.0, 1.03, 0.75, 0.71), 2.0 * 1.03 * 0.75 * 0.71, 1e-9,
          "four-factor product");
    const double kinf = four_factor(1.65, 1.03, 0.87, 0.71);
    const double keff = six_factor(kinf, 0.97, 0.99);
    check(keff < kinf, "leakage reduces k_eff below k_inf");
    check(classify_criticality(1.0) == Criticality::Critical, "k=1 is critical");
    check(classify_criticality(0.95) == Criticality::Subcritical, "k<1 subcritical");
    check(classify_criticality(1.05) == Criticality::Supercritical, "k>1 supercritical");
    // Reactivity is zero at critical, positive above, negative below.
    close(reactivity(1.0), 0.0, 1e-12, "zero reactivity at critical");
    check(reactivity(1.05) > 0.0 && reactivity(0.95) < 0.0, "reactivity sign tracks k");

    // Reproduction factor: more capture (vs fission) lowers eta below nu-bar.
    check(reproduction_factor(2.42, 500.0, 100.0) < 2.42, "capture lowers eta below nu-bar");
    check(reproduction_factor(2.42, 500.0, 0.0) == 2.42, "no capture -> eta = nu-bar");

    // --- Determinism --------------------------------------------------------
    check(four_factor(2, 1, 1, 1) == four_factor(2, 1, 1, 1), "four-factor deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_fissionphysics_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_fissionphysics_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
