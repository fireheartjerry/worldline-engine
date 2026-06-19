// Verifies the physics-grounding constants used across the scale ladder: that
// the stored Planck units are self-consistent with their defining formulas (so
// the constexpr table can't silently drift from G/c/hbar/kB), and that the
// fundamental constants + dimensionless ratios match CODATA/PDG within tolerance.
// These are the floor of the SUBATOMIC tier; the generator must never sample
// below them.

#include "cosmos/Constants.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::constants;

namespace {

int g_failures = 0;

// Relative-tolerance check (the stored Planck values carry ~6 sig figs).
void close(double got, double want, double rel, const std::string& what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_physics_verification FAILED: " << what
                  << " got=" << got << " want=" << want
                  << " rel=" << std::abs(got - want) / denom << "\n";
        ++g_failures;
    }
}

void check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_physics_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- Planck units re-derived from their defining formulas ------------------
    const double lP = std::sqrt(hbar * G / (c * c * c));
    const double tP = lP / c;
    const double mP = std::sqrt(hbar * c / G);
    const double EP = mP * c * c;
    const double TP = EP / kB;

    close(lP, planck_length_m, 1e-4, "Planck length = sqrt(hbar*G/c^3)");
    close(tP, planck_time_s,   1e-4, "Planck time = l_P/c");
    close(mP, planck_mass_kg,  1e-4, "Planck mass = sqrt(hbar*c/G)");
    close(EP, planck_energy_J, 1e-4, "Planck energy = m_P*c^2");
    close(TP, planck_temp_K,   1e-4, "Planck temperature = E_P/kB");

    // --- Exact-by-definition SI constants -------------------------------------
    close(c, 2.99792458e8, 1e-12, "speed of light exact");
    close(h, 6.62607015e-34, 1e-12, "Planck constant exact");
    close(e, 1.602176634e-19, 1e-12, "elementary charge exact");
    close(kB, 1.380649e-23, 1e-12, "Boltzmann constant exact");
    close(hbar, h / (2.0 * pi), 1e-6, "hbar = h/2pi");

    // --- Dimensionless ratios --------------------------------------------------
    close(1.0 / alpha, alpha_inv, 1e-5, "alpha_inv = 1/alpha ~ 137.036");
    close(proton_electron_mass_ratio, 1836.152673, 1e-6, "m_p/m_e ~ 1836");
    // alpha_G = G*m_e^2/(hbar*c) with m_e = 9.1093837e-31 kg.
    const double m_e = 9.1093837015e-31;
    close(G * m_e * m_e / (hbar * c), grav_coupling_electron, 1e-3,
          "gravitational coupling alpha_G(electron)");

    // --- Force hierarchy ordering (strong >> EM >> weak >> gravity) ------------
    check(strength_strong > strength_em && strength_em > strength_weak &&
              strength_weak > strength_gravity,
          "force strengths ordered strong>EM>weak>gravity");
    check(planck_length_m > 0.0 && planck_temp_K > 0.0, "Planck floor positive");

    if (g_failures == 0) {
        std::cout << "cosmos_physics_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_physics_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
