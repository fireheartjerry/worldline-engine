// Verifies cosmos/BetaDecayTheory.hpp: the Q^5 phase-space factor, ft / log ft
// classification, Fermi vs Gamow-Teller selection rules, the Fermi Coulomb
// correction, the Kurie linearisation, and double beta decay phase space.

#include "cosmos/BetaDecayTheory.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::beta;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_betadecay_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_betadecay_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Phase space (Sargent) ----------------------------------------------
    close(phase_space_factor(2.0) / phase_space_factor(1.0), 32.0, 1e-9, "phase space ~ Q^5");
    check(phase_space_factor(0.0) == 0.0, "no phase space at zero Q");

    // --- ft / log ft classification -----------------------------------------
    close(ft_value(1000.0, 3.0), 3000.0, 1e-9, "ft = f * t");
    close(log_ft(1000.0, 3.0), std::log10(3000.0), 1e-9, "log ft");
    check(classify_log_ft(3.2) == Transition::Superallowed, "log ft 3.2 -> superallowed");
    check(classify_log_ft(5.0) == Transition::Allowed, "log ft 5.0 -> allowed");
    check(classify_log_ft(7.5) == Transition::FirstForbidden, "log ft 7.5 -> first forbidden");
    check(classify_log_ft(12.0) == Transition::HigherForbidden, "log ft 12 -> higher forbidden");
    // The superallowed Ft is a ~3000 s constant.
    check(kSuperallowedFt_s > 3000.0 && kSuperallowedFt_s < 3150.0, "superallowed Ft ~ 3072 s");

    // --- Selection rules ----------------------------------------------------
    check(is_allowed_fermi(0, false), "Fermi: dJ=0, no parity change allowed");
    check(!is_allowed_fermi(1, false), "Fermi: dJ=1 forbidden");
    check(!is_allowed_fermi(0, true), "Fermi: parity change forbidden");
    check(is_allowed_gamow_teller(1, 1, false), "GT: 1->1 allowed");
    check(is_allowed_gamow_teller(1, 2, false), "GT: dJ=1 allowed");
    check(!is_allowed_gamow_teller(0, 0, false), "GT: 0->0 forbidden");
    check(!is_allowed_gamow_teller(1, 1, true), "GT: parity change forbidden");
    check(!is_allowed_gamow_teller(1, 3, false), "GT: dJ=2 forbidden");

    // --- Fermi Coulomb correction -------------------------------------------
    // Electrons (beta-) are attracted -> F > 1; positrons (beta+) repelled -> F < 1.
    check(fermi_function(26, 0.5, true) > 1.0, "beta- Fermi function enhances (F>1)");
    check(fermi_function(26, 0.5, false) < 1.0, "beta+ Fermi function suppresses (F<1)");
    // The effect grows with daughter charge.
    check(fermi_function(82, 0.5, true) > fermi_function(26, 0.5, true),
          "higher Z -> larger Coulomb correction");

    // --- Kurie plot ---------------------------------------------------------
    // Linear in energy, hitting zero at the endpoint Q.
    close(kurie_linear(1.0, 1.0), 0.0, 1e-12, "Kurie ordinate zero at the endpoint");
    check(kurie_linear(2.0, 0.5) > kurie_linear(2.0, 1.5), "Kurie falls toward the endpoint");

    // --- Double beta decay --------------------------------------------------
    // 2nu mode (~Q^11) is far more Q-sensitive than 0nu (~Q^5).
    close(double_beta_2nu_phase_space(2.0) / double_beta_2nu_phase_space(1.0), 2048.0, 1e-6,
          "2nu double beta ~ Q^11");
    check(double_beta_2nu_phase_space(2.0) / double_beta_2nu_phase_space(1.0) >
              double_beta_0nu_phase_space(2.0) / double_beta_0nu_phase_space(1.0),
          "2nu more Q-sensitive than 0nu");

    // --- Determinism --------------------------------------------------------
    check(phase_space_factor(3.0) == phase_space_factor(3.0), "phase space deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_betadecay_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_betadecay_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
