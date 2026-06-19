// Verifies cosmos/DecayChains.hpp: the two-step Bateman solution, secular /
// transient equilibrium classification, the four natural decay series, and the
// alpha/beta step counts from parent to stable end-point.

#include "cosmos/DecayChains.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::chains;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_decaychains_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_decaychains_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Bateman two-step ---------------------------------------------------
    // Daughter starts at zero, rises, then falls; parent decays monotonically.
    const double lA = 0.1, lB = 1.0, N0 = 1000.0;
    close(bateman_daughter(N0, lA, lB, 0.0), 0.0, 1e-9, "daughter starts at 0");
    check(bateman_daughter(N0, lA, lB, 1.0) > 0.0, "daughter grows then decays");
    check(parent_population(N0, lA, 10.0) < parent_population(N0, lA, 1.0),
          "parent decays monotonically");
    // Degenerate equal-rate case stays finite (limit form).
    check(std::isfinite(bateman_daughter(N0, 0.5, 0.5, 2.0)), "equal-rate Bateman finite");

    // --- Equilibrium --------------------------------------------------------
    check(classify_equilibrium(1e-4, 1.0) == Equilibrium::Secular,
          "long parent / short daughter -> secular");
    check(classify_equilibrium(0.3, 1.0) == Equilibrium::Transient, "comparable -> transient");
    check(classify_equilibrium(2.0, 1.0) == Equilibrium::None, "short parent -> no equilibrium");
    // In deep secular equilibrium the activity ratio approaches 1.
    close(secular_activity_ratio(1e-5, 1.0), 1.0, 1e-3, "secular activity ratio -> 1");

    // --- The four decay series ----------------------------------------------
    // U-238 (A=238) is the 4n+2 uranium series.
    check(series_for_A(238) == Series::Uranium4n2, "A=238 -> uranium 4n+2 series");
    check(series_for_A(232) == Series::Thorium4n, "A=232 -> thorium 4n series");
    check(series_for_A(235) == Series::Actinium4n3, "A=235 -> actinium 4n+3 series");
    check(series_for_A(237) == Series::Neptunium4n1, "A=237 -> neptunium 4n+1 series");
    check(std::string(series_name(Series::Uranium4n2)).find("Uranium") != std::string::npos,
          "series name string");

    // --- Step counts: U-238 -> Pb-206 ---------------------------------------
    // (A=238,Z=92) decays to stable Pb-206 (Z=82) via 8 alpha and 6 beta-minus.
    close(alpha_count(238, 206), 8, 1e-9, "U-238 -> Pb-206: 8 alpha decays");
    close(beta_minus_count(92, 238, 82, 206), 6, 1e-9, "U-238 -> Pb-206: 6 beta-minus");
    // Th-232 -> Pb-208: 6 alpha, 4 beta-minus.
    close(alpha_count(232, 208), 6, 1e-9, "Th-232 -> Pb-208: 6 alpha decays");
    close(beta_minus_count(90, 232, 82, 208), 4, 1e-9, "Th-232 -> Pb-208: 4 beta-minus");

    // --- Determinism --------------------------------------------------------
    check(series_for_A(238) == series_for_A(238), "series deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_decaychains_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_decaychains_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
