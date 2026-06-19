// Verifies cosmos/FineStructure.hpp: the Dirac fine-structure ordering and alpha^2
// scale, the spin-orbit Z^4 growth, the Lande interval rule, the 21 cm hyperfine
// line, the Lamb shift, and the gross >> fine >> hyperfine >> Lamb hierarchy.

#include "cosmos/AtomicStructure.hpp"
#include "cosmos/FineStructure.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::fine;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_finestructure_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    if (std::abs(got - want) / denom > rel) {
        std::cerr << "cosmos_finestructure_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Fine structure -----------------------------------------------------
    // The Dirac energy is close to the Bohr energy (correction ~ alpha^2).
    close(dirac_energy_ev(1, 0.5), cosmos::atom::energy_level_ev(1), 1e-3,
          "Dirac ~ Bohr to order alpha^2");
    // Higher j lies higher (less bound): 2p3/2 above 2p1/2.
    check(dirac_energy_ev(2, 1.5) > dirac_energy_ev(2, 0.5), "2p3/2 lies above 2p1/2");
    // The fine-structure correction is small compared to the gross energy.
    check(std::abs(fine_structure_correction_ev(2, 0.5)) <
              1e-3 * std::abs(cosmos::atom::energy_level_ev(2)),
          "fine structure << gross structure");
    // Spin-orbit scale grows as Z^4.
    close(spin_orbit_scale_ev(2, 2) / spin_orbit_scale_ev(2, 1), 16.0, 1e-9, "spin-orbit ~ Z^4");
    // Lande interval rule: spacing proportional to J.
    close(lande_interval(3.0) / lande_interval(2.0), 1.5, 1e-9, "Lande interval ~ J");

    // --- Hyperfine (21 cm) --------------------------------------------------
    close(k21cm_frequency_hz, 1.4204e9, 1e-3, "21 cm line at 1420 MHz");
    close(k21cm_wavelength_m, 0.211, 1e-2, "21 cm wavelength");
    // Frequency and wavelength are consistent with c.
    close(k21cm_frequency_hz * k21cm_wavelength_m, 2.998e8, 1e-3, "nu * lambda = c");
    // Hyperfine is suppressed by ~ m_e/m_p (~1/1836).
    check(hyperfine_suppression() < 1e-3, "hyperfine suppressed by m_e/m_p");

    // --- Lamb shift ---------------------------------------------------------
    close(kLambShift_hz, 1057.8e6, 1e-2, "Lamb shift ~1058 MHz");
    check(kLambShift_eV > 0.0, "Lamb shift energy positive");

    // --- Hierarchy of scales ------------------------------------------------
    const ScaleHierarchy h = hydrogen_scales();
    check(h.gross > h.fine, "gross >> fine");
    check(h.fine > h.lamb, "fine >> Lamb");
    check(h.fine > h.hyperfine, "fine >> hyperfine");
    // The Lamb shift and the hyperfine splitting are both micro-eV scale and
    // comparable to each other (hyperfine is slightly larger in hydrogen).
    check(h.lamb > 1e-7 && h.lamb < 1e-5, "Lamb shift is micro-eV scale");
    check(h.hyperfine > 1e-7 && h.hyperfine < 1e-5, "hyperfine is micro-eV scale");
    // Spanning many orders of magnitude from eV down to micro-eV.
    check(h.gross / h.hyperfine > 1e5, "gross exceeds hyperfine by >1e5");

    // --- Determinism --------------------------------------------------------
    check(dirac_energy_ev(2, 1.5) == dirac_energy_ev(2, 1.5), "Dirac deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_finestructure_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_finestructure_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
