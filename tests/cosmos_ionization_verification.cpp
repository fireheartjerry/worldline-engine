// Verifies cosmos/Ionization.hpp: photoionization thresholds, the Saha ionization
// equilibrium and its temperature/density dependence, and plasma collective
// quantities (Debye length, plasma frequency, Debye number).

#include "cosmos/Ionization.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::ionization;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_ionization_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_ionization_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Photoionization ----------------------------------------------------
    // Hydrogen (13.6 eV) ionizes at the Lyman limit ~91 nm.
    close(photoionization_threshold_nm(13.6057), 91.18, 1e-2, "H photoionization limit ~91 nm");
    check(can_ionize(20.0, 13.6), "20 eV photon ionizes hydrogen");
    check(!can_ionize(10.0, 13.6), "10 eV photon cannot ionize hydrogen");
    // Higher binding energy -> shorter threshold wavelength.
    check(photoionization_threshold_nm(54.4) < photoionization_threshold_nm(13.6),
          "deeper binding -> shorter threshold");

    // --- Saha equation ------------------------------------------------------
    // Ionization rises steeply with temperature.
    const double chi = 13.6;   // hydrogen
    const double n_e = 1.0e20; // electrons/m^3 (stellar-atmosphere-ish)
    check(ionized_fraction(12000.0, chi, n_e) > ionized_fraction(6000.0, chi, n_e),
          "Saha: hotter -> more ionized");
    // At fixed temperature, higher electron density suppresses ionization.
    check(ionized_fraction(10000.0, chi, 1.0e18) > ionized_fraction(10000.0, chi, 1.0e22),
          "Saha: denser -> less ionized (recombination)");
    // The ionized fraction is bounded in [0,1] and near 0 when cold.
    check(ionized_fraction(3000.0, chi, n_e) < 0.05, "cold gas mostly neutral");
    check(ionized_fraction(50000.0, chi, n_e) > 0.9, "very hot gas mostly ionized");
    // Lower ionization energy ionizes more easily at the same T (e.g. sodium).
    check(ionized_fraction(6000.0, 5.14, n_e) > ionized_fraction(6000.0, 13.6, n_e),
          "low-chi species (Na) ionizes before hydrogen");

    // --- Plasma collective behaviour ----------------------------------------
    // Debye length grows with T and shrinks with density.
    check(debye_length_m(20000.0, 1e18) > debye_length_m(10000.0, 1e18),
          "Debye length grows with temperature");
    check(debye_length_m(10000.0, 1e20) < debye_length_m(10000.0, 1e18),
          "Debye length shrinks with density");
    // Plasma frequency rises as sqrt(density): 100x density -> 10x frequency.
    close(plasma_frequency_rad_s(1e20) / plasma_frequency_rad_s(1e18), 10.0, 1e-6,
          "plasma frequency ~ sqrt(n_e)");
    check(plasma_frequency_rad_s(1e18) > 0.0, "plasma frequency positive");
    // A valid plasma has many particles in a Debye sphere.
    check(debye_number(1e6, 1e18) > 1.0, "many electrons in a Debye sphere (collective)");

    // --- Determinism --------------------------------------------------------
    check(debye_length_m(1e4, 1e18) == debye_length_m(1e4, 1e18), "Debye deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_ionization_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_ionization_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
