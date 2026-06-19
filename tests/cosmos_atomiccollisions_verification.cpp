// Verifies cosmos/AtomicCollisions.hpp: geometric and Coulomb cross sections,
// mean free path and collision frequency, thermal speed, Bethe stopping power,
// electron-impact thresholds, and radiative recombination scaling.

#include "cosmos/AtomicCollisions.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::collisions;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_atomiccollisions_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_atomiccollisions_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Cross sections -----------------------------------------------------
    // Geometric cross section ~ (r1+r2)^2.
    close(geometric_cross_section(1e-10, 1e-10) / geometric_cross_section(0.5e-10, 0.5e-10), 4.0,
          1e-9, "geometric sigma ~ (r1+r2)^2");
    // Coulomb cross section falls as 1/E^2.
    close(coulomb_cross_section_scale(1, 1, 1.0) / coulomb_cross_section_scale(1, 1, 2.0), 4.0,
          1e-9, "Coulomb sigma ~ 1/E^2");
    check(coulomb_cross_section_scale(2, 2, 1.0) > coulomb_cross_section_scale(1, 1, 1.0),
          "higher charge -> larger Coulomb cross section");

    // --- Transport ----------------------------------------------------------
    // Mean free path ~ 1/(n sigma): denser or larger cross section -> shorter path.
    close(mean_free_path(1e25, 1e-19), 1.0 / (1e25 * 1e-19), 1e-9, "mean free path = 1/(n sigma)");
    check(mean_free_path(2e25, 1e-19) < mean_free_path(1e25, 1e-19), "denser gas -> shorter path");
    // Collision frequency rises with density, cross section, and speed.
    check(collision_frequency(2e25, 1e-19, 500.0) > collision_frequency(1e25, 1e-19, 500.0),
          "collision frequency ~ density");
    // Thermal speed rises with temperature and falls with mass.
    check(mean_thermal_speed(1000.0, 1.67e-27) > mean_thermal_speed(300.0, 1.67e-27),
          "thermal speed rises with T");
    check(mean_thermal_speed(300.0, 9.1e-31) > mean_thermal_speed(300.0, 1.67e-27),
          "lighter particle is faster");

    // --- Bethe stopping power -----------------------------------------------
    // Scales as z^2 and falls as 1/v^2 (slow, highly-charged particles deposit
    // most). Well above the logarithmic threshold, the 1/v^2 term dominates.
    const double v = 2.0e7, ne = 1e29, I = 80.0;
    check(bethe_stopping_scale(2, v, ne, I) > bethe_stopping_scale(1, v, ne, I),
          "stopping power ~ z^2");
    check(bethe_stopping_scale(1, v, ne, I) > bethe_stopping_scale(1, 2.0 * v, ne, I),
          "slower projectile loses more energy (1/v^2)");

    // --- Electron-impact ----------------------------------------------------
    check(impact_above_threshold(15.0, 13.6) && !impact_above_threshold(10.0, 13.6),
          "impact ionization only above threshold");

    // --- Recombination ------------------------------------------------------
    // Radiative recombination rate ~ T^(-1/2): cooler plasma recombines faster.
    check(radiative_recombination_scale(5000.0) > radiative_recombination_scale(20000.0),
          "cooler plasma recombines faster");
    close(radiative_recombination_scale(4000.0) / radiative_recombination_scale(16000.0), 2.0, 1e-9,
          "recombination ~ T^(-1/2)");

    // --- Determinism --------------------------------------------------------
    check(mean_free_path(1e25, 1e-19) == mean_free_path(1e25, 1e-19), "mfp deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_atomiccollisions_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_atomiccollisions_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
