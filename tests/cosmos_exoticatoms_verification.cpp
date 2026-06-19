// Verifies cosmos/ExoticAtoms.hpp: reduced-mass scaling, positronium, muonic
// hydrogen, and the Rydberg-atom n-power scaling laws.

#include "cosmos/ExoticAtoms.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::exotic;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_exoticatoms_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_exoticatoms_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Reduced mass -------------------------------------------------------
    // Hydrogen's reduced mass is just below 1 m_e (~0.99946).
    check(hydrogen_reduced_mass_me() < 1.0 && hydrogen_reduced_mass_me() > 0.999,
          "hydrogen reduced mass ~0.99946 m_e");
    // Ordinary hydrogen binding ~13.6 eV (with the small reduced-mass shift).
    close(scaled_binding_ev(hydrogen_reduced_mass_me(), 1), 13.6, 1e-2, "H binding ~13.6 eV");

    // --- Positronium --------------------------------------------------------
    // mu = m_e/2 -> ground state -6.80 eV, radius = 2 a0.
    close(positronium_binding_ev(), 6.803, 1e-2, "positronium binding 6.8 eV (half of H)");
    close(positronium_radius_pm(), 2.0 * 52.918, 1e-3, "positronium radius = 2 a0");

    // --- Muonic hydrogen ----------------------------------------------------
    // The muon orbits ~186x tighter: binding ~2.5 keV, radius ~285 fm with the
    // reduced mass (256 fm in the infinite-nuclear-mass approximation).
    check(muonic_hydrogen_binding_ev() > 2000.0, "muonic H binding ~2.5 keV");
    check(muonic_hydrogen_radius_pm() < 0.5, "muonic H Bohr radius is sub-pm");
    close(muonic_hydrogen_radius_pm(), 0.285, 5e-2, "muonic H radius ~0.285 pm (reduced mass)");
    // Muonic binding is ~186x the electronic binding.
    check(muonic_hydrogen_binding_ev() / scaled_binding_ev(hydrogen_reduced_mass_me(), 1) > 150.0,
          "muonic binding >150x electronic");

    // --- Rydberg atoms ------------------------------------------------------
    // Radius ~ n^2 : n=100 is ~0.5 micron across.
    close(rydberg_radius_pm(100) / rydberg_radius_pm(50), 4.0, 1e-9, "Rydberg radius ~ n^2");
    check(rydberg_radius_pm(100) > 5.0e5, "n=100 Rydberg atom ~ half a micron");
    // Binding ~ 1/n^2 : weakly bound (meV at n~50).
    close(rydberg_binding_ev(50) * 2500.0, 13.6, 1e-2, "Rydberg binding ~ Ry/n^2");
    check(rydberg_binding_ev(100) < rydberg_binding_ev(50), "higher n -> less bound");
    // Lifetime ~ n^3, polarizability ~ n^7 (extreme field sensitivity).
    close(rydberg_lifetime_scaling(20) / rydberg_lifetime_scaling(10), 8.0, 1e-9,
          "Rydberg lifetime ~ n^3");
    close(rydberg_polarizability_scaling(20) / rydberg_polarizability_scaling(10), 128.0, 1e-6,
          "Rydberg polarizability ~ n^7");
    // Level spacing ~ 1/n^3 collapses toward the ionization limit.
    check(rydberg_level_spacing_ev(100) < rydberg_level_spacing_ev(50),
          "Rydberg levels crowd together at high n");

    // --- Determinism --------------------------------------------------------
    check(positronium_binding_ev() == positronium_binding_ev(), "positronium deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_exoticatoms_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_exoticatoms_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
