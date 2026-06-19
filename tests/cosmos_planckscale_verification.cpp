// Verifies cosmos/PlanckScale.hpp: the full Planck unit system derived from
// c/G/hbar/k_B/e, black-hole thermodynamics (Hawking T, Bekenstein-Hawking
// entropy, Page evaporation), the holographic / Bekenstein bounds, and the GUP
// minimal length. Targets are CODATA-derived Planck quantities.

#include "cosmos/Constants.hpp"
#include "cosmos/PlanckScale.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::planck;
using namespace cosmos::constants;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_planckscale_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    const double err = std::abs(got - want) / denom;
    if (err > rel) {
        std::cerr << "cosmos_planckscale_verification FAILED: " << what << " got=" << got
                  << " want=" << want << " rel=" << err << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    const PlanckSystem p = planck_system();

    // --- Base units match the stored CODATA Planck table --------------------
    close(p.length_m, planck_length_m, 1e-4, "Planck length");
    close(p.mass_kg, planck_mass_kg, 1e-4, "Planck mass");
    close(p.time_s, planck_time_s, 1e-4, "Planck time");
    close(p.temperature_K, planck_temp_K, 1e-4, "Planck temperature");
    close(p.energy_J, planck_energy_J, 1e-4, "Planck energy");

    // --- Derived units against textbook values ------------------------------
    close(p.charge_C, 1.875546e-18, 1e-4, "Planck charge = e/sqrt(alpha)");
    close(p.charge_C, e / std::sqrt(alpha), 1e-9, "Planck charge identity");
    close(p.momentum_kgms, 6.5249, 1e-3, "Planck momentum ~ 6.52 kg m/s");
    close(p.force_N, 1.2103e44, 1e-3, "Planck force c^4/G");
    close(p.power_W, 3.6283e52, 1e-3, "Planck power c^5/G");
    close(p.density_kgm3, 5.155e96, 2e-3, "Planck density");
    close(p.area_m2, p.length_m * p.length_m, 1e-12, "Planck area = l_P^2");
    close(p.volume_m3, p.length_m * p.length_m * p.length_m, 1e-12, "Planck volume = l_P^3");
    // Force is c^4/G and contains no hbar (classical gravity scale).
    close(p.force_N, c * c * c * c / G, 1e-12, "Planck force is hbar-free");

    // --- Black-hole thermodynamics ------------------------------------------
    // Hawking T scales as 1/M; a solar-mass hole is ~6e-8 K.
    close(hawking_temperature_K(1.989e30), 6.17e-8, 2e-2, "Hawking T(1 Msun) ~ 62 nK");
    check(hawking_temperature_K(1.0e29) > hawking_temperature_K(1.0e30),
          "smaller holes are hotter");
    // Entropy scales as M^2.
    close(bekenstein_hawking_entropy_over_kb(2.0e30) / bekenstein_hawking_entropy_over_kb(1.0e30),
          4.0, 1e-9, "BH entropy ~ M^2");
    check(bekenstein_hawking_entropy_over_kb(1.989e30) > 1.0e76,
          "solar-mass BH entropy is enormous (>1e76 k_B)");
    // Evaporation time scales as M^3.
    close(evaporation_time_s(2.0e11) / evaporation_time_s(1.0e11), 8.0, 1e-9,
          "evaporation time ~ M^3");

    // --- Holographic / Bekenstein bounds ------------------------------------
    check(holographic_bits(1.0) > 0.0 && std::isfinite(holographic_bits(1.0)),
          "holographic bit count finite & positive");
    close(holographic_bits(2.0) / holographic_bits(1.0), 2.0, 1e-9, "holographic bound ~ area");
    check(bekenstein_bound_over_kb(1.0, 1.0) > 0.0, "Bekenstein bound positive");

    // --- GUP minimal length -------------------------------------------------
    const double lmin = gup_minimal_length(1.0);
    close(lmin, p.length_m * std::sqrt(2.0), 1e-9, "GUP min length = l_P sqrt(2)");
    // Scan dp: the position uncertainty never drops below lmin, and saturates it.
    double observed_min = 1.0e300;
    for (double dp = p.momentum_kgms * 1.0e-3; dp <= p.momentum_kgms * 1.0e3; dp *= 1.2)
        observed_min = std::min(observed_min, gup_position_uncertainty(dp, 1.0));
    check(observed_min >= lmin * (1.0 - 1e-6), "GUP uncertainty never below the minimum");
    close(observed_min, lmin, 5e-3, "GUP scan saturates the minimal length");

    // --- Compton-Schwarzschild crossover ~ Planck mass ----------------------
    close(compton_schwarzschild_mass_kg(), planck_mass_kg / std::sqrt(2.0), 1e-3,
          "Compton=Schwarzschild at m_P/sqrt(2)");

    // --- Determinism --------------------------------------------------------
    check(hawking_temperature_K(1.0e30) == hawking_temperature_K(1.0e30), "Hawking deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_planckscale_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_planckscale_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
