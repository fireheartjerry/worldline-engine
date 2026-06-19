// Verifies cosmos/NuclearMoments.hpp: the nuclear magneton, free nucleon
// moments, the Schmidt single-particle magnetic moments, even-even zero moments,
// quadrupole sign, and Larmor precession.

#include "cosmos/NuclearMoments.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::moments;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nuclearmoments_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_nuclearmoments_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Nuclear magneton & free nucleon moments ----------------------------
    close(kNuclearMagneton_eV_per_T, 3.15245e-8, 1e-4, "nuclear magneton ~ 3.152e-8 eV/T");
    close(kProtonMoment, 2.792847, 1e-5, "proton moment +2.793 mu_N");
    close(kNeutronMoment, -1.913043, 1e-5, "neutron moment -1.913 mu_N");
    check(kProtonGs > 0.0 && kNeutronGs < 0.0, "g_s signs: proton +, neutron -");

    // --- Schmidt single-particle moments ------------------------------------
    // A single s1/2 proton (l=0, j=l+1/2) reproduces the free proton moment.
    close(schmidt_moment(true, 0, true), kProtonMoment, 1e-9, "Schmidt p s1/2 = +2.793");
    // A single s1/2 neutron reproduces the free neutron moment.
    close(schmidt_moment(true, 0, false), kNeutronMoment, 1e-9, "Schmidt n s1/2 = -1.913");
    // d3/2 proton (l=2, j=l-1/2) gives the lower Schmidt line.
    {
        const double mu = schmidt_moment(false, 2, true);
        // j/(j+1)[(j+3/2) g_l - g_s/2] with j=1.5, g_l=1, g_s=5.5857.
        const double j = 1.5;
        const double want = j / (j + 1.0) * ((j + 1.5) * 1.0 - 0.5 * kProtonGs);
        close(mu, want, 1e-9, "Schmidt p d3/2 lower line");
    }
    // The two Schmidt lines for a given l differ (j=l+1/2 vs j=l-1/2).
    check(schmidt_moment(true, 2, true) != schmidt_moment(false, 2, true),
          "Schmidt upper and lower lines differ");

    // --- Even-even & g-factor -----------------------------------------------
    check(even_even_moment() == 0.0, "even-even nucleus has zero moment");
    close(g_factor(kProtonMoment, 0.5), kProtonMoment / 0.5, 1e-9, "g = mu / j");

    // --- Quadrupole moment --------------------------------------------------
    // Single-particle Q is negative (oblate single-particle distribution); the
    // magnitude grows with nuclear size.
    check(single_particle_quadrupole(2.5, 100) < 0.0, "single-particle Q < 0");
    check(std::abs(single_particle_quadrupole(2.5, 208)) >
              std::abs(single_particle_quadrupole(2.5, 16)),
          "Q magnitude grows with A");
    check(is_prolate(0.5) && is_oblate(-0.5), "quadrupole sign convention");

    // --- Larmor precession --------------------------------------------------
    // Frequency scales with field and g-factor.
    check(larmor_frequency(5.586, 1.0) > 0.0, "Larmor frequency positive");
    close(larmor_frequency(5.586, 2.0) / larmor_frequency(5.586, 1.0), 2.0, 1e-9,
          "Larmor frequency ~ B");

    // --- Determinism --------------------------------------------------------
    check(schmidt_moment(true, 0, true) == schmidt_moment(true, 0, true), "Schmidt deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nuclearmoments_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nuclearmoments_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
