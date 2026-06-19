// Verifies cosmos/QuantumVacuum.hpp: Casimir pressure/energy scaling and sign,
// the Schwinger critical field, the Unruh temperature, and the Planck-cutoff
// vacuum energy density (the cosmological-constant problem).

#include "cosmos/QuantumVacuum.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::vac;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_quantumvacuum_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    const double err = std::abs(got - want) / denom;
    if (err > rel) {
        std::cerr << "cosmos_quantumvacuum_verification FAILED: " << what << " got=" << got
                  << " want=" << want << " rel=" << err << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Casimir effect -----------------------------------------------------
    // Attractive (negative) and ~1.3 mPa at 1 micron.
    check(casimir_pressure_Pa(1.0e-6) < 0.0, "Casimir pressure is attractive");
    close(std::abs(casimir_pressure_Pa(1.0e-6)), 1.30e-3, 2e-2, "Casimir P ~ 1.3 mPa at 1um");
    // Strong d^-4 scaling: halve the gap -> 16x the pressure.
    close(casimir_pressure_Pa(0.5e-6) / casimir_pressure_Pa(1.0e-6), 16.0, 1e-6,
          "Casimir pressure ~ d^-4");
    close(casimir_energy_per_area(0.5e-6) / casimir_energy_per_area(1.0e-6), 8.0, 1e-6,
          "Casimir energy/area ~ d^-3");

    // --- Schwinger limit ----------------------------------------------------
    close(schwinger_critical_field_Vm(), 1.323e18, 1e-2, "Schwinger E_c ~ 1.32e18 V/m");
    close(schwinger_critical_field_T(), 4.41e9, 1e-2, "Schwinger B_c ~ 4.4e9 T");
    // Suppression is exponentially tiny well below E_c and ~order-1 at E_c.
    check(schwinger_suppression(1.0e15) < 1e-100, "pair production negligible below E_c");
    check(schwinger_suppression(schwinger_critical_field_Vm()) > 0.04, "turns on near E_c");
    check(schwinger_suppression(2.0e18) > schwinger_suppression(1.0e18),
          "suppression rises with field");

    // --- Unruh effect -------------------------------------------------------
    check(unruh_temperature_K(1.0) > 0.0, "Unruh T positive for a>0");
    close(unruh_temperature_K(1.0), 4.06e-21, 2e-2, "Unruh T ~ 4.06e-21 K per (m/s^2)");
    check(unruh_temperature_K(2.0) > unruh_temperature_K(1.0), "Unruh T rises with acceleration");

    // --- Vacuum energy / cosmological-constant problem ----------------------
    // The Planck-cutoff vacuum density dwarfs the observed dark-energy density by
    // an astronomical factor (the famous ~120 orders of magnitude).
    const double ratio = planck_cutoff_vacuum_density() / kObservedDarkEnergy_Jm3;
    check(ratio > 1.0e100, "Planck-cutoff vacuum density >> observed (c.c. problem)");
    close(vacuum_energy_density(2.0) / vacuum_energy_density(1.0), 16.0, 1e-9,
          "vacuum energy density ~ k_max^4");

    // --- Determinism --------------------------------------------------------
    check(schwinger_critical_field_Vm() == schwinger_critical_field_Vm(), "E_c deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_quantumvacuum_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_quantumvacuum_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
