// AtomicSpectra.hpp -- how atoms emit and absorb light: the hydrogen spectral
// series (Lyman, Balmer, Paschen, ...), term symbols, the electric-dipole
// selection rules, the Zeeman effect, line broadening (natural / Doppler), and
// the blackbody / Wien law. The fingerprint by which we read the composition of
// stars and nebulae.
//
// Header-only, pure, deterministic. Wavelengths in nm, energies in eV.
//
// Sources:
//   - Hydrogen series: Lyman (n1=1, UV), Balmer (n1=2, visible), Paschen (n1=3, IR).
//   - Selection rules: Delta l = +/-1, Delta J = 0, +/-1 (not 0->0), Delta S = 0.
//   - Bohr magneton mu_B = 5.7884e-5 eV/T (Zeeman splitting).
//   - Wien displacement: lambda_max T = 2.8978e-3 m*K.

#ifndef COSMOS_ATOMICSPECTRA_HPP
#define COSMOS_ATOMICSPECTRA_HPP

#include "cosmos/AtomicStructure.hpp"
#include "cosmos/Constants.hpp"

#include <cmath>
#include <string>

namespace cosmos {
namespace spectra {

inline constexpr double kBohrMagneton_eV_per_T = 5.7883818e-5;
inline constexpr double kWien_m_K = 2.897771955e-3;

// --- Hydrogen spectral series -----------------------------------------------

enum class Series { Lyman = 1, Balmer = 2, Paschen = 3, Brackett = 4, Pfund = 5 };

// Lower level n1 that defines a series.
inline int series_lower_level(Series s) {
    return static_cast<int>(s);
}

// Wavelength of the n2 line of a series (n2 > n1), hydrogen. [nm]
inline double line_wavelength_nm(Series s, int n2) {
    return atom::transition_wavelength_nm(series_lower_level(s), n2, 1);
}

// Series limit (n2 -> infinity): the shortest wavelength of the series. [nm]
inline double series_limit_nm(Series s) {
    const int n1 = series_lower_level(s);
    const double inv = atom::kRydbergConst_per_m / static_cast<double>(n1 * n1);
    return 1.0e9 / inv;
}

// The first (alpha) line of a series sits at its longest wavelength.
inline double series_alpha_nm(Series s) {
    return line_wavelength_nm(s, series_lower_level(s) + 1);
}

// --- Term symbols -----------------------------------------------------------

inline int multiplicity(double S) {
    return static_cast<int>(std::round(2.0 * S + 1.0));
}

// Term symbol string ^{2S+1}L_J, e.g. (S=1/2, L=0, J=1/2) -> "2S1/2".
inline std::string term_symbol(double S, int L, double J) {
    static const char *kUpper = "SPDFGHIKLM";
    std::string s = std::to_string(multiplicity(S));
    s += (L >= 0 && L < 10) ? kUpper[L] : '?';
    const int twoJ = static_cast<int>(std::round(2.0 * J));
    if (twoJ % 2 == 0) {
        s += std::to_string(twoJ / 2);
    } else {
        s += std::to_string(twoJ) + "/2";
    }
    return s;
}

// --- Electric-dipole selection rules ----------------------------------------

// Allowed E1 transition: Delta l = +/-1, Delta J in {0,+/-1} but not 0->0,
// Delta S = 0 (for LS coupling).
inline bool dipole_allowed(int dl, int dJ, int Ji, int Jf, int dS) {
    if (std::abs(dl) != 1)
        return false;
    if (dS != 0)
        return false;
    if (std::abs(dJ) > 1)
        return false;
    if (Ji == 0 && Jf == 0)
        return false;
    return true;
}

// --- Zeeman effect ----------------------------------------------------------

// Normal Zeeman splitting of a level in a magnetic field B: Delta E = m_l mu_B B. [eV]
inline double zeeman_shift_ev(int m_l, double B_tesla) {
    return m_l * kBohrMagneton_eV_per_T * B_tesla;
}

// Lande g-factor for LS coupling: g_J = 1 + [J(J+1)+S(S+1)-L(L+1)] / [2J(J+1)].
inline double lande_g(double J, double L, double S) {
    if (J <= 0.0)
        return 0.0;
    return 1.0 + (J * (J + 1.0) + S * (S + 1.0) - L * (L + 1.0)) / (2.0 * J * (J + 1.0));
}

// Anomalous Zeeman shift: Delta E = g_J m_J mu_B B. [eV]
inline double anomalous_zeeman_ev(double g_J, double m_J, double B_tesla) {
    return g_J * m_J * kBohrMagneton_eV_per_T * B_tesla;
}

// --- Line broadening --------------------------------------------------------

// Natural (lifetime) linewidth Delta E = hbar / tau [eV], from the upper-state
// lifetime tau [s].
inline double natural_linewidth_ev(double tau_s) {
    if (tau_s <= 0.0)
        return INFINITY;
    return constants::hbar / tau_s / constants::e; // J -> eV
}

// Doppler (thermal) fractional width Delta_lambda/lambda = sqrt(2 k T / m c^2),
// for an emitter of mass m_kg at temperature T.
inline double doppler_fractional_width(double T_K, double m_kg) {
    if (m_kg <= 0.0 || T_K <= 0.0)
        return 0.0;
    return std::sqrt(2.0 * constants::kB * T_K / (m_kg * constants::c * constants::c));
}

// --- Blackbody / Wien -------------------------------------------------------

// Wien peak wavelength lambda_max = b / T [m].
inline double wien_peak_wavelength_m(double T_K) {
    if (T_K <= 0.0)
        return INFINITY;
    return kWien_m_K / T_K;
}

// Planck spectral radiance B(lambda, T) [W sr^-1 m^-3], for completeness.
inline double planck_radiance(double lambda_m, double T_K) {
    if (lambda_m <= 0.0 || T_K <= 0.0)
        return 0.0;
    const double h = constants::h, c = constants::c, kB = constants::kB;
    const double x = h * c / (lambda_m * kB * T_K);
    return (2.0 * h * c * c) / std::pow(lambda_m, 5.0) / (std::exp(x) - 1.0);
}

} // namespace spectra
} // namespace cosmos

#endif // COSMOS_ATOMICSPECTRA_HPP
