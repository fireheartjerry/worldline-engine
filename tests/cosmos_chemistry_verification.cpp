// Verifies the MOLECULAR-tier chemistry tables/models in cosmos/Chemistry.hpp:
//   - Cosmic molecular abundances: H2 most abundant; CO > CH4/NH3; lookups.
//   - Pauling-electronegativity bond classification.
//   - Simple T-P phase diagram for water (solid/liquid/gas/supercritical/plasma).
//   - Condensation ("snow line") ordering: rock/iron > water ice > CO2.
//   - Density-based matter regimes (ordered, distinct).
//   - Determinism / purity and finiteness of outputs.

#include "cosmos/Chemistry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::chem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_chemistry_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- Abundant molecules --------------------------------------------------
    const MoleculeInfo* h2 = molecule("H2");
    const MoleculeInfo* co = molecule("CO");
    const MoleculeInfo* ch4 = molecule("CH4");
    const MoleculeInfo* nh3 = molecule("NH3");
    check(h2 != nullptr, "molecule(\"H2\") found");
    check(co != nullptr, "molecule(\"CO\") found");
    check(ch4 != nullptr, "molecule(\"CH4\") found");
    check(nh3 != nullptr, "molecule(\"NH3\") found");
    check(molecule("XX") == nullptr, "molecule(\"XX\") null");
    check(molecule(nullptr) == nullptr, "molecule(nullptr) null");

    // H2 is the single most abundant molecule in the table.
    double maxab = 0.0;
    for (const auto& m : kMolecules) {
        if (m.rel_abundance > maxab) maxab = m.rel_abundance;
    }
    check(h2->rel_abundance == maxab, "H2 is the most abundant molecule");
    check(std::fabs(h2->rel_abundance - 1.0) < 1e-12, "H2 abundance == 1.0");

    // CO is more abundant than methane and ammonia.
    check(co->rel_abundance > ch4->rel_abundance, "CO > CH4");
    check(co->rel_abundance > nh3->rel_abundance, "CO > NH3");

    // --- Bond classification -------------------------------------------------
    // NaCl-like: EN(Na)=0.93, EN(Cl)=3.16, dEN ~ 2.23 -> Ionic.
    check(classify_bond(0.93, 3.16) == BondType::Ionic, "NaCl-like -> Ionic");
    // O-H in water: EN(O)=3.44, EN(H)=2.20, dEN ~ 1.24 -> (polar) Covalent.
    check(classify_bond(3.44, 2.20) == BondType::Covalent, "O-H -> Covalent");
    // H-H: dEN 0 -> (nonpolar) Covalent (H EN 2.20, above metallic cutoff).
    check(classify_bond(2.20, 2.20) == BondType::Covalent, "H-H -> Covalent");
    // Two low-EN metals (Na-Na, EN 0.93) -> Metallic.
    check(classify_bond(0.93, 0.93) == BondType::Metallic, "Na-Na -> Metallic");

    // --- Phase diagram of water ----------------------------------------------
    const double mk = kWaterMeltK, bk = kWaterBoilK;
    const double ct = kWaterCritTempK, cp = kWaterCritPressAtm;
    const double ion = 10000.0; // ~ ionization temperature scale
    check(phase_of(250.0, 1.0, mk, bk, ct, cp, ion) == Phase::Solid,
          "water 250 K, 1 atm -> Solid");
    check(phase_of(300.0, 1.0, mk, bk, ct, cp, ion) == Phase::Liquid,
          "water 300 K, 1 atm -> Liquid");
    check(phase_of(400.0, 1.0, mk, bk, ct, cp, ion) == Phase::Gas,
          "water 400 K, 1 atm -> Gas");
    check(phase_of(700.0, 300.0, mk, bk, ct, cp, ion) == Phase::Supercritical,
          "water 700 K, 300 atm -> Supercritical");
    check(phase_of(1.0e5, 1.0, mk, bk, ct, cp, ion) == Phase::Plasma,
          "water 1e5 K, 1 atm -> Plasma");

    // --- Condensation temperatures -------------------------------------------
    const double tIron = condensation_temperature_k("iron");
    const double tRock = condensation_temperature_k("rock");
    const double tIce = condensation_temperature_k("water");
    const double tCO2 = condensation_temperature_k("CO2");
    check(tIron > tRock, "iron condenses hotter than rock");
    check(tRock > tIce, "rock condenses hotter than water ice");
    check(tIce > tCO2, "water ice condenses hotter than CO2");
    check(condensation_temperature_k("unobtainium") == -1.0, "unknown -> -1");
    check(condensation_temperature_k(nullptr) == -1.0, "nullptr -> -1");

    // --- Matter regimes (ordered, distinct strings) --------------------------
    const std::string r0 = matter_regime(1.0e-6);  // diffuse gas
    const std::string r1 = matter_regime(1.0);      // molecular cloud
    const std::string r2 = matter_regime(1.0e4);    // condensed matter
    const std::string r3 = matter_regime(1.0e12);   // degenerate matter
    check(r0 == "diffuse gas", "1e-6 -> diffuse gas");
    check(r1 == "molecular cloud", "1.0 -> molecular cloud");
    check(r2 == "condensed matter", "1e4 -> condensed matter");
    check(r3 == "degenerate matter", "1e12 -> degenerate matter");
    check(r0 != r1 && r1 != r2 && r2 != r3 && r0 != r2 && r0 != r3 && r1 != r3,
          "matter regimes are distinct");

    // --- Determinism / purity ------------------------------------------------
    check(molecule("CO") == molecule("CO"), "molecule lookup deterministic");
    check(classify_bond(0.93, 3.16) == classify_bond(0.93, 3.16),
          "classify_bond deterministic");
    check(phase_of(300.0, 1.0, mk, bk, ct, cp, ion) ==
              phase_of(300.0, 1.0, mk, bk, ct, cp, ion),
          "phase_of deterministic");
    check(condensation_temperature_k("iron") ==
              condensation_temperature_k("iron"),
          "condensation deterministic");

    // --- Finiteness ----------------------------------------------------------
    check(std::isfinite(h2->rel_abundance), "H2 abundance finite");
    check(std::isfinite(tIron) && std::isfinite(tCO2), "condensation finite");
    check(std::isfinite(kWaterCritTempK) && std::isfinite(kWaterCritPressAtm),
          "water critical constants finite");

    if (g_failures != 0) {
        std::cerr << "cosmos_chemistry_verification: " << g_failures
                  << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_chemistry_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
