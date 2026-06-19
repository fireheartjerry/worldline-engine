// Verifies cosmos/NeutrinoOscillation.hpp: two-flavour oscillation probability
// and unitarity, the oscillation length / first maximum, PMNS row unitarity, and
// the MSW resonance sign.

#include "cosmos/NeutrinoOscillation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::nu;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_neutrino_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_neutrino_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Two-flavour oscillation --------------------------------------------
    // At the source (L=0) there is no oscillation yet.
    close(transition_prob(kTheta23, kDm31_eV2, 0.0, 1.0), 0.0, 1e-12, "P=0 at L=0");
    close(survival_prob(kTheta23, kDm31_eV2, 0.0, 1.0), 1.0, 1e-12, "survival=1 at L=0");
    // Unitarity at any baseline: P_transition + P_survival = 1.
    const double Pt = transition_prob(kTheta23, kDm31_eV2, 500.0, 1.0);
    const double Ps = survival_prob(kTheta23, kDm31_eV2, 500.0, 1.0);
    close(Pt + Ps, 1.0, 1e-12, "two-flavour unitarity");
    check(Pt >= 0.0 && Pt <= 1.0, "probability in [0,1]");

    // At the first maximum the phase is pi/2, so sin^2(phase)=1 and the
    // transition probability equals sin^2(2 theta).
    const double Lmax = first_maximum_km(kDm31_eV2, 1.0);
    const double s2 = std::sin(2.0 * kTheta23);
    close(transition_prob(kTheta23, kDm31_eV2, Lmax, 1.0), s2 * s2, 1e-9,
          "first maximum reaches sin^2(2theta)");

    // Oscillation length scales with E and inversely with dm^2.
    check(oscillation_length_km(kDm31_eV2, 2.0) > oscillation_length_km(kDm31_eV2, 1.0),
          "oscillation length grows with energy");
    check(oscillation_length_km(kDm31_eV2, 1.0) < oscillation_length_km(kDm21_eV2, 1.0),
          "larger dm^2 -> shorter oscillation length");
    // The atmospheric first max for ~1 GeV neutrinos is several hundred km.
    check(Lmax > 200.0 && Lmax < 800.0, "atmospheric first max ~ few hundred km at 1 GeV");

    // --- PMNS row unitarity -------------------------------------------------
    close(electron_row_sum(), 1.0, 1e-12, "|U_e1|^2+|U_e2|^2+|U_e3|^2 = 1");
    check(Ue1_sq() > Ue2_sq() && Ue2_sq() > Ue3_sq(), "electron content: nu1 > nu2 > nu3");
    check(Ue3_sq() > 0.01 && Ue3_sq() < 0.04, "|U_e3|^2 ~ sin^2(theta13) ~ 0.022");

    // --- MSW ----------------------------------------------------------------
    // For the solar angle (theta12 < 45 deg), cos(2 theta) > 0: a matter
    // resonance exists for neutrinos.
    check(msw_resonance_sign(kTheta12) > 0.0, "solar-sector MSW resonance for neutrinos");

    // --- Determinism --------------------------------------------------------
    check(transition_prob(kTheta23, kDm31_eV2, 500.0, 1.0) ==
              transition_prob(kTheta23, kDm31_eV2, 500.0, 1.0),
          "oscillation deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_neutrino_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_neutrino_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
