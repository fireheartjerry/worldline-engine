// Verifies cosmos/NuclearDecay.hpp: the decay law, Q-values for alpha/beta/EC,
// the alpha Gamow + Geiger-Nuttall systematics, Sargent's beta Q^5 rule, the
// Weisskopf gamma estimates, and decay-mode prediction.

#include "cosmos/NuclearDecay.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::decay;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nucleardecay_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_nucleardecay_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Decay law ----------------------------------------------------------
    close(decay_constant_from_halflife(1.0), kLn2, 1e-12, "lambda = ln2 / t_half");
    close(halflife_from_decay_constant(decay_constant_from_halflife(10.0)), 10.0, 1e-12,
          "t_half <-> lambda round trip");
    close(surviving_fraction(10.0, 10.0), 0.5, 1e-12, "half remains after one half-life");
    close(surviving_fraction(20.0, 10.0), 0.25, 1e-12, "quarter remains after two");
    close(mean_lifetime_s(kLn2), 1.0, 1e-12, "mean life = t_half / ln2");

    // --- Q-values -----------------------------------------------------------
    // Free neutron beta-minus Q ~ 0.782 MeV (n -> p + e + nu).
    close(q_beta_minus_mev(0, 1), 0.782, 2e-2, "free neutron beta Q ~ 0.782 MeV");
    // Heavy nuclei have positive alpha Q; light nuclei negative.
    check(q_alpha_mev(92, 238) > 0.0, "U-238 alpha Q > 0");
    check(q_alpha_mev(8, 16) < 0.0, "O-16 alpha Q < 0 (stable to alpha)");
    // Beta-plus and electron capture differ by 2 m_e: Q_EC = Q_b+ + 2 m_e.
    close(q_electron_capture_mev(26, 55) - q_beta_plus_mev(26, 55),
          2.0 * cosmos::nuclear::kElectronMass_mev, 1e-9, "Q_EC = Q_beta+ + 2 m_e");

    // --- Alpha systematics --------------------------------------------------
    // Higher Q -> easier tunnelling (smaller Gamow exponent) -> shorter half-life.
    check(alpha_gamow_exponent(90, 6.0) < alpha_gamow_exponent(90, 4.0),
          "higher Q -> smaller Gamow exponent");
    check(geiger_nuttall_log10_halflife(90, 6.0) < geiger_nuttall_log10_halflife(90, 4.0),
          "Geiger-Nuttall: higher Q -> shorter half-life");
    check(geiger_nuttall_log10_halflife(90, 4.0) > geiger_nuttall_log10_halflife(60, 4.0),
          "Geiger-Nuttall: higher Z -> longer half-life");

    // --- Beta systematics (Sargent) -----------------------------------------
    // Rate ~ Q^5: doubling Q multiplies the rate by 32.
    close(sargent_relative_rate(2.0) / sargent_relative_rate(1.0), 32.0, 1e-9,
          "Sargent rule: rate ~ Q^5");

    // --- Gamma (Weisskopf) --------------------------------------------------
    // Each higher multipole is far slower: E1 (L=1) >> E2 >> E3 at fixed E, A.
    check(weisskopf_rate_per_s(1, 1.0, 100) > weisskopf_rate_per_s(2, 1.0, 100),
          "E1 far faster than E2 at fixed energy");
    check(weisskopf_rate_per_s(2, 1.0, 100) > weisskopf_rate_per_s(3, 1.0, 100),
          "E2 faster than E3");
    check(weisskopf_rate_per_s(1, 2.0, 100) > weisskopf_rate_per_s(1, 1.0, 100),
          "gamma rate rises with energy");
    close(weisskopf_rate_per_s(1, 2.0, 100) / weisskopf_rate_per_s(1, 1.0, 100), 8.0, 1e-9,
          "E1 rate ~ E^3");

    // --- Decay-mode prediction ----------------------------------------------
    check(predict_mode(8, 16) == Mode::Stable, "O-16 predicted stable");
    check(predict_mode(92, 238) == Mode::Alpha, "U-238 predicted alpha");
    // A very neutron-rich nucleus beta-minus decays toward stability.
    check(predict_mode(8, 20) == Mode::BetaMinus, "neutron-rich O-20 -> beta-minus");
    // A proton-rich nucleus goes by beta-plus / electron capture.
    check(predict_mode(10, 18) == Mode::BetaPlusOrEC, "proton-rich Ne-18 -> beta+/EC");

    // --- Determinism --------------------------------------------------------
    check(q_alpha_mev(92, 238) == q_alpha_mev(92, 238), "Q_alpha deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nucleardecay_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nucleardecay_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
