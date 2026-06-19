// Verifies cosmos/LightMatter.hpp: the Einstein A/B relations, the photoelectric
// effect, Rabi flopping, Beer-Lambert absorption, and the laser population-
// inversion / gain condition.

#include "cosmos/LightMatter.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::lightmatter;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_lightmatter_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_lightmatter_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Einstein coefficients ----------------------------------------------
    // A/B ~ nu^3: spontaneous emission dominates at high frequency.
    close(a_over_b_ratio(2e15) / a_over_b_ratio(1e15), 8.0, 1e-9, "A/B ~ nu^3");
    check(a_over_b_ratio(1e15) > a_over_b_ratio(1e9),
          "spontaneous emission dominates in the optical vs radio");
    // Detailed balance g1 B12 = g2 B21.
    close(b12_from_b21(5.0, 1, 3), 15.0, 1e-9, "g1 B12 = g2 B21");
    close(b12_from_b21(5.0, 2, 2), 5.0, 1e-9, "equal g -> B12 = B21");
    close(spontaneous_rate(1e-8), 1e8, 1e-9, "A21 = 1/tau");

    // --- Photoelectric effect -----------------------------------------------
    // KE = h nu - W, zero below threshold, linear above.
    close(photoelectron_ke_ev(5.0, 2.0), 3.0, 1e-9, "KE = photon - work function");
    check(photoelectron_ke_ev(1.0, 2.0) == 0.0, "no emission below threshold");
    check(emits_photoelectron(3.0, 2.0) && !emits_photoelectron(1.0, 2.0),
          "emission only above threshold");
    check(photoelectric_threshold_hz(4.0) > photoelectric_threshold_hz(2.0),
          "higher work function -> higher threshold frequency");

    // --- Rabi flopping ------------------------------------------------------
    // Rabi frequency linear in the field; generalized Rabi exceeds it on detuning.
    close(rabi_frequency(2e-29, 2e6) / rabi_frequency(2e-29, 1e6), 2.0, 1e-9,
          "Rabi frequency ~ field");
    check(generalized_rabi(1e9, 1e9) > 1e9, "detuning raises the generalized Rabi frequency");
    // P_e oscillates: 0 at t=0, 1 at a pi-pulse (Omega t = pi).
    close(rabi_excited_probability(1e9, 0.0), 0.0, 1e-12, "no excitation at t=0");
    close(rabi_excited_probability(1e9, 3.14159265358979 / 1e9), 1.0, 1e-6,
          "pi-pulse fully inverts");

    // --- Beer-Lambert -------------------------------------------------------
    // Transmission falls exponentially with column; tau=1 attenuates by ~1/e.
    check(transmission(1e-20, 1e20, 2.0) < transmission(1e-20, 1e20, 1.0),
          "thicker column -> less transmitted");
    close(transmission(1e-20, 1e20, 1.0), std::exp(-1.0), 1e-9, "tau=1 -> 1/e transmission");
    close(optical_depth(1e-20, 1e20, 1.0), 1.0, 1e-9, "optical depth = sigma n L");

    // --- Laser gain ---------------------------------------------------------
    // Inversion when n2/g2 > n1/g1; gain is positive only when inverted.
    check(is_inverted(0.6, 1, 0.4, 1), "more in upper level -> inverted");
    check(!is_inverted(0.4, 1, 0.6, 1), "thermal population -> not inverted");
    check(gain_coefficient(1e-20, 0.6, 1, 0.4, 1) > 0.0, "inverted medium amplifies");
    check(gain_coefficient(1e-20, 0.4, 1, 0.6, 1) < 0.0, "non-inverted medium absorbs");

    // --- Determinism --------------------------------------------------------
    check(a_over_b_ratio(1e15) == a_over_b_ratio(1e15), "A/B deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_lightmatter_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_lightmatter_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
