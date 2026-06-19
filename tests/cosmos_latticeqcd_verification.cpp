// Verifies cosmos/LatticeQCD.hpp: the Cornell static-quark potential (Coulomb at
// short range, linear confinement at long range), the string tension and its
// Wilson-loop area law, string breaking, Regge slope, and the cold-lattice
// plaquette / Wilson action.

#include "cosmos/LatticeQCD.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::lattice;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_latticeqcd_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_latticeqcd_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- String tension -----------------------------------------------------
    // sigma ~ 0.18 GeV^2 ~ 0.91 GeV/fm.
    close(string_tension_gev_per_fm(), 0.912, 2e-2, "string tension ~ 0.91 GeV/fm");

    // --- Cornell potential --------------------------------------------------
    // Short range: Coulomb attraction dominates -> V < 0.
    check(cornell_potential_gev(0.1) < 0.0, "Cornell potential attractive at short range");
    // Long range: linear confinement -> V grows without bound and is increasing.
    check(cornell_potential_gev(2.0) > cornell_potential_gev(1.0), "confining: V rises with r");
    check(cornell_potential_gev(3.0) > 1.0, "V exceeds 1 GeV by a few fm (confinement)");
    // The incremental cost per fm at large r approaches the string tension.
    const double dV = cornell_potential_gev(5.0) - cornell_potential_gev(4.0);
    close(dV, confining_force_gev_per_fm(), 5e-2, "large-r slope -> string tension");

    // --- String breaking ----------------------------------------------------
    // Heavier produced quarks require a longer string to break.
    check(string_breaking_distance_fm(0.5) > string_breaking_distance_fm(0.3),
          "heavier pair -> longer breaking distance");
    check(string_breaking_distance_fm(0.33) > 0.5 && string_breaking_distance_fm(0.33) < 2.0,
          "light-quark string breaks around ~1 fm");

    // --- Wilson loop area law -----------------------------------------------
    // <W> decays exponentially with area (confinement order parameter).
    check(wilson_loop_area_law(2.0) < wilson_loop_area_law(1.0), "area law: <W> falls with area");
    close(wilson_loop_area_law(0.0), 1.0, 1e-12, "<W> = 1 for zero area");
    // The potential extracted from the area law is exactly linear.
    close(potential_from_area_law(2.0) / potential_from_area_law(1.0), 2.0, 1e-9,
          "area-law potential is linear in R");

    // --- Regge trajectory ---------------------------------------------------
    close(regge_slope_gev2(), 0.884, 2e-2, "Regge slope alpha' ~ 0.88 GeV^-2");
    check(regge_spin(2.0) > regge_spin(1.0), "spin rises with mass on a Regge trajectory");

    // --- Lattice plaquette / action -----------------------------------------
    close(cold_average_plaquette(4), 1.0, 1e-12, "cold lattice average plaquette = 1");
    close(wilson_action(2.0, cold_average_plaquette(4), 16), 0.0, 1e-12,
          "cold lattice Wilson action = 0 (classical vacuum)");
    check(wilson_action(2.0, 0.5, 16) > 0.0, "disordered plaquettes raise the action");

    // --- Determinism --------------------------------------------------------
    check(cornell_potential_gev(1.0) == cornell_potential_gev(1.0), "Cornell deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_latticeqcd_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_latticeqcd_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
