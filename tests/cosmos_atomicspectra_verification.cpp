// Verifies cosmos/AtomicSpectra.hpp: the hydrogen spectral series and their
// limits, term symbols, dipole selection rules, the Zeeman effect and Lande
// g-factor, line broadening, and the Wien / Planck blackbody law.

#include "cosmos/AtomicSpectra.hpp"
#include "cosmos/Constants.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::spectra;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_atomicspectra_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_atomicspectra_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Hydrogen series ----------------------------------------------------
    // Balmer-alpha (H-alpha) at 656 nm (red); Lyman-alpha at 121.6 nm (UV).
    close(series_alpha_nm(Series::Balmer), 656.3, 3e-3, "H-alpha 656 nm");
    close(series_alpha_nm(Series::Lyman), 121.6, 3e-3, "Lyman-alpha 121.6 nm");
    // Paschen-alpha is in the infrared.
    check(series_alpha_nm(Series::Paschen) > 1000.0, "Paschen-alpha in the IR");
    // Series limits get longer for higher series; Lyman limit ~91 nm.
    close(series_limit_nm(Series::Lyman), 91.18, 3e-3, "Lyman limit 91.2 nm");
    check(series_limit_nm(Series::Balmer) > series_limit_nm(Series::Lyman),
          "Balmer limit redder than Lyman");
    // Within a series, higher lines are bluer (shorter wavelength).
    check(line_wavelength_nm(Series::Balmer, 4) < line_wavelength_nm(Series::Balmer, 3),
          "H-beta bluer than H-alpha");

    // --- Term symbols -------------------------------------------------------
    check(multiplicity(0.5) == 2, "doublet (S=1/2)");
    check(multiplicity(1.0) == 3, "triplet (S=1)");
    check(term_symbol(0.5, 0, 0.5) == "2S1/2", "ground-state hydrogen term");
    check(term_symbol(1.0, 1, 2.0) == "3P2", "triplet P term");

    // --- Selection rules ----------------------------------------------------
    check(dipole_allowed(1, 0, 1, 1, 0), "dl=+1, dJ=0 allowed");
    check(dipole_allowed(-1, 1, 1, 2, 0), "dl=-1, dJ=1 allowed");
    check(!dipole_allowed(0, 0, 1, 1, 0), "dl=0 forbidden");
    check(!dipole_allowed(2, 0, 1, 1, 0), "dl=2 forbidden");
    check(!dipole_allowed(1, 0, 0, 0, 0), "0->0 forbidden");
    check(!dipole_allowed(1, 0, 1, 1, 1), "dS!=0 forbidden");

    // --- Zeeman effect ------------------------------------------------------
    // Normal splitting linear in B and m_l; symmetric about zero.
    close(zeeman_shift_ev(1, 1.0), kBohrMagneton_eV_per_T, 1e-9, "Zeeman shift = m_l mu_B B");
    close(zeeman_shift_ev(-1, 2.0), -2.0 * kBohrMagneton_eV_per_T, 1e-9, "Zeeman linear in B");
    // Lande g: a pure-spin state (L=0, S=1/2, J=1/2) has g=2.
    close(lande_g(0.5, 0.0, 0.5), 2.0, 1e-9, "Lande g = 2 for pure spin");
    // A pure-orbital state (S=0, J=L) has g=1.
    close(lande_g(1.0, 1.0, 0.0), 1.0, 1e-9, "Lande g = 1 for pure orbital");

    // --- Line broadening ----------------------------------------------------
    // Natural width falls with lifetime; Doppler width rises with temperature.
    check(natural_linewidth_ev(1e-9) > natural_linewidth_ev(1e-6),
          "shorter lifetime -> broader line");
    check(doppler_fractional_width(10000.0, 1.67e-27) > doppler_fractional_width(1000.0, 1.67e-27),
          "hotter gas -> broader Doppler width");
    // Heavier emitters have narrower Doppler widths at the same T.
    check(doppler_fractional_width(5000.0, 9.1e-26) < doppler_fractional_width(5000.0, 1.67e-27),
          "heavier atom -> narrower Doppler width");

    // --- Blackbody / Wien ---------------------------------------------------
    // Sun (~5772 K) peaks in the visible (~500 nm); hotter stars peak bluer.
    close(wien_peak_wavelength_m(5772.0) * 1e9, 502.0, 2e-2, "Sun peaks ~500 nm");
    check(wien_peak_wavelength_m(10000.0) < wien_peak_wavelength_m(5000.0), "hotter -> bluer peak");
    // Planck radiance positive and larger at the peak for a hotter body.
    check(planck_radiance(500e-9, 5772.0) > 0.0, "Planck radiance positive");
    check(planck_radiance(500e-9, 6000.0) > planck_radiance(500e-9, 5000.0),
          "hotter body radiates more at 500 nm");

    // --- Determinism --------------------------------------------------------
    check(series_alpha_nm(Series::Balmer) == series_alpha_nm(Series::Balmer),
          "spectra deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_atomicspectra_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_atomicspectra_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
