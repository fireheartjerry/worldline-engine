// Verifies cosmos/QEDScattering.hpp: the classical electron radius and Thomson
// cross section, Klein-Nishina Compton scattering and the wavelength shift,
// Rutherford/Mott angular dependence, Mandelstam s+t+u, and the Breit-Wigner
// resonance line shape.

#include "cosmos/Constants.hpp"
#include "cosmos/QEDScattering.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::qed;
using namespace cosmos::constants;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_qedscattering_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_qedscattering_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    const double pi = 3.14159265358979323846;

    // --- Classical electron radius & Thomson --------------------------------
    close(classical_electron_radius_m(), 2.8179e-15, 1e-3, "r_e ~ 2.818e-15 m");
    close(thomson_cross_section_m2(), 6.6524e-29, 1e-3, "Thomson sigma_T ~ 6.652e-29 m^2");

    // --- Compton scattering -------------------------------------------------
    // Wavelength shift: 0 forward, 2 lambda_C at back-scatter.
    close(compton_shift_m(0.0), 0.0, 1e-12, "no Compton shift at 0 deg");
    close(compton_shift_m(pi), 2.0 * 2.42631e-12, 1e-3, "back-scatter shift = 2 lambda_C");
    close(compton_shift_m(pi / 2.0), 2.42631e-12, 1e-3, "90 deg shift = lambda_C");
    // Scattered energy: unchanged forward, reduced at angle, and < incident.
    close(compton_scattered_energy_mev(1.0, 0.0), 1.0, 1e-9, "forward Compton: E' = E");
    check(compton_scattered_energy_mev(1.0, pi) < 1.0, "back-scatter loses energy");
    // Klein-Nishina reduces to Thomson at low energy and falls with energy.
    close(klein_nishina_total_m2(1e-4), thomson_cross_section_m2(), 1e-2,
          "Klein-Nishina -> Thomson at low E");
    check(klein_nishina_total_m2(1.0) < thomson_cross_section_m2(),
          "Klein-Nishina cross section falls with energy");
    check(klein_nishina_total_m2(10.0) < klein_nishina_total_m2(1.0),
          "Klein-Nishina monotonically decreasing");

    // --- Rutherford / Mott --------------------------------------------------
    const double E = 1.0e6 * e; // 1 MeV
    // 1/sin^4(theta/2): far more forward scattering than backward.
    check(rutherford_dcs(2, 79, E, 0.5) > rutherford_dcs(2, 79, E, 2.5),
          "Rutherford strongly forward-peaked");
    // The sin^4 law: ratio at 60 vs 120 deg = sin^4(60)/sin^4(30) = 9.
    close(rutherford_dcs(2, 79, E, pi / 3.0) / rutherford_dcs(2, 79, E, 2.0 * pi / 3.0),
          std::pow(std::sin(pi / 3.0), 4.0) / std::pow(std::sin(pi / 6.0), 4.0), 1e-9,
          "Rutherford 1/sin^4(theta/2) angular law");
    // Mott factor is 1 at theta=0 and 1-beta^2 at back-scatter.
    close(mott_factor(0.5, 0.0), 1.0, 1e-12, "Mott factor = 1 forward");
    close(mott_factor(0.5, pi), 1.0 - 0.25, 1e-12, "Mott factor = 1-beta^2 backward");

    // --- Mandelstam ---------------------------------------------------------
    // s + t + u = sum of squared masses (here: electron Compton, m3=m1, m4=m2=0).
    close(mandelstam_sum_mev2(0.511, 0.0, 0.511, 0.0), 2.0 * 0.511 * 0.511, 1e-9,
          "Mandelstam s+t+u = sum m^2");

    // --- Breit-Wigner -------------------------------------------------------
    // Peak = 1 at E = M, half-maximum at E = M +/- Gamma/2.
    close(breit_wigner(91.19, 91.19, 2.5), 1.0, 1e-12, "Breit-Wigner peak = 1 at M");
    close(breit_wigner(91.19 + 1.25, 91.19, 2.5), 0.5, 1e-9, "half-max at M + Gamma/2");
    check(breit_wigner(80.0, 91.19, 2.5) < 0.1, "Breit-Wigner falls off-resonance");
    // Relativistic form peaks at s = M^2.
    check(relativistic_breit_wigner(91.19 * 91.19, 91.19, 2.5) >
              relativistic_breit_wigner(85.0 * 85.0, 91.19, 2.5),
          "relativistic BW peaks at s = M^2");

    // --- Determinism --------------------------------------------------------
    check(thomson_cross_section_m2() == thomson_cross_section_m2(), "sigma_T deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_qedscattering_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_qedscattering_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
