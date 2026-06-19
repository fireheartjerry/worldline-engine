// FineStructure.hpp -- the small splittings that the gross Bohr/Rydberg picture
// misses: fine structure (spin-orbit + relativistic kinetic + Darwin terms, ~
// alpha^2), hyperfine structure (the electron-nuclear spin coupling that gives
// the 21 cm line, ~ alpha^2 m_e/m_p), and the Lamb shift (a pure QED effect).
// The hierarchy gross >> fine >> hyperfine >> Lamb is itself a deep result.
//
// Header-only, pure, deterministic. Energies in eV (or as noted).
//
// Sources:
//   - Dirac fine structure E_nj = E_n [1 + (alpha^2/n^2)(n/(j+1/2) - 3/4)].
//   - Hydrogen 21 cm hyperfine line: 1420.4 MHz, 5.87 micro-eV.
//   - Lamb shift (2S1/2 - 2P1/2): ~1057 MHz, ~4.4 micro-eV (QED).

#ifndef COSMOS_FINESTRUCTURE_HPP
#define COSMOS_FINESTRUCTURE_HPP

#include "cosmos/AtomicStructure.hpp"
#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace fine {

// --- Fine structure ---------------------------------------------------------

// Dirac fine-structure energy of a hydrogenic level (n, j): includes spin-orbit,
// relativistic kinetic, and Darwin corrections to order alpha^2. [eV]
inline double dirac_energy_ev(int n, double j, int Z = 1) {
    if (n <= 0)
        return 0.0;
    const double En = atom::energy_level_ev(n, Z); // negative
    const double a2 = constants::alpha * constants::alpha;
    const double corr = (a2 * Z * Z / (n * n)) * (static_cast<double>(n) / (j + 0.5) - 0.75);
    return En * (1.0 + corr);
}

// Fine-structure correction alone (Dirac energy minus the Bohr energy). [eV]
// Negative for low j (more bound), and states with larger j lie higher.
inline double fine_structure_correction_ev(int n, double j, int Z = 1) {
    return dirac_energy_ev(n, j, Z) - atom::energy_level_ev(n, Z);
}

// Spin-orbit coupling scale ~ Ry Z^4 alpha^2 / n^3 (grows steeply with Z). [eV]
inline double spin_orbit_scale_ev(int n, int Z = 1) {
    return atom::fine_structure_scale_ev(n, Z);
}

// Lande interval rule: the splitting between adjacent fine-structure levels J and
// J-1 is proportional to J. Returns the relative spacing factor.
inline double lande_interval(double J) {
    return J;
}

// --- Hyperfine structure ----------------------------------------------------

// The hydrogen ground-state hyperfine ("spin-flip") transition: 21 cm.
inline constexpr double k21cm_frequency_hz = 1.420405751768e9; // 1420.4 MHz
inline constexpr double k21cm_wavelength_m = 0.211061140542;   // 21.1 cm
inline constexpr double k21cm_energy_eV = 5.8743e-6;           // ~5.87 micro-eV

// Hyperfine splitting is suppressed below the fine structure by ~ m_e/m_p
// (the nuclear magneton is ~2000x smaller than the Bohr magneton).
inline double hyperfine_suppression() {
    return constants::electron_mass_kg / constants::proton_mass_kg;
}

// --- Lamb shift (QED) -------------------------------------------------------

// The Lamb shift lifts the Dirac degeneracy of 2S1/2 and 2P1/2 (which have the
// same n and j): the 2S1/2 state sits ~1057 MHz ABOVE 2P1/2 -- a pure quantum-
// electrodynamic effect (vacuum fluctuations + self-energy).
inline constexpr double kLambShift_hz = 1057.8e6;  // ~1057.8 MHz
inline constexpr double kLambShift_eV = 4.3747e-6; // ~4.37 micro-eV

// --- The hierarchy of scales ------------------------------------------------

// Convenience: the four energy scales of the hydrogen 2p level, which must obey
// gross >> fine >> hyperfine >> Lamb. Returns them in eV (all positive magnitudes).
struct ScaleHierarchy {
    double gross;     // ~ eV
    double fine;      // ~ alpha^2 * gross
    double lamb;      // ~ alpha^3 * gross (QED)
    double hyperfine; // ~ (m_e/m_p) alpha^2 * gross
};

inline ScaleHierarchy hydrogen_scales() {
    ScaleHierarchy h;
    h.gross = std::abs(atom::energy_level_ev(2, 1));
    h.fine = spin_orbit_scale_ev(2, 1);
    h.lamb = kLambShift_eV;
    h.hyperfine = k21cm_energy_eV;
    return h;
}

} // namespace fine
} // namespace cosmos

#endif // COSMOS_FINESTRUCTURE_HPP
