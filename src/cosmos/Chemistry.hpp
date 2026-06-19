// Chemistry.hpp -- molecular / condensed-matter data tables and models for the
// MOLECULAR scale tier of the Worldline engine.
//
// Header-only, pure, deterministic. Namespace: cosmos::chem.
//
// This module bridges atomic chemistry (bonding, abundant molecules) to the
// planetary / bulk condensed-matter tier (condensation "snow lines", matter
// regimes by density). Values are sourced from the deep-research appendix on
// cosmic molecular abundances, chemical bonding, and states of matter, cited
// inline:
//   - Cosmic molecular abundances: H2 is overwhelmingly the most abundant
//     interstellar molecule; CO is the second most abundant (CO/H2 ~ 1e-4 in
//     molecular clouds), followed by H2O, OH, N2, CO2, CH4, NH3 at lower
//     fractional abundances (Tielens, "The Physics and Chemistry of the
//     Interstellar Medium", 2005; van Dishoeck & Blake, ARA&A 36, 317, 1998).
//   - Bond classification by Pauling electronegativity difference: |dEN| > 1.7
//     ionic, ~0.4-1.7 polar covalent, < 0.4 (nonpolar) covalent; near-zero EN
//     between low-EN species -> metallic (Pauling, "The Nature of the Chemical
//     Bond", 1960; standard general-chemistry thresholds).
//   - Phase behaviour: triple/critical points and the simple T-P phase diagram;
//     water critical point 647 K / 218 atm (CRC Handbook).
//   - Condensation temperatures / equilibrium condensation sequence: refractory
//     rock/metal condense hot (~1300-1600 K) and volatile ices cold
//     (water ~150-170 K, CO2 ~80 K) (Lodders, ApJ 591, 1220, 2003; the
//     protoplanetary "snow line" picture).

#ifndef COSMOS_CHEMISTRY_HPP
#define COSMOS_CHEMISTRY_HPP

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "cosmos/Spectrum.hpp" // clampd, lerp (continuous helpers)

namespace cosmos::chem {

// ---------------------------------------------------------------------------
// Cosmically abundant molecules
// ---------------------------------------------------------------------------
//
// rel_abundance is the molecular number abundance relative to H2 (H2 == 1.0).
// Diatomic hydrogen H2 dominates molecular gas; CO is the canonical tracer at
// CO/H2 ~ 1e-4; the remaining values are representative dense-cloud fractional
// abundances (order-of-magnitude). H2O is given as 1e-4 here (it can reach a
// few x 1e-4 in warm dense gas).
struct MoleculeInfo {
    const char* name;
    const char* formula;
    double rel_abundance; // relative to H2 == 1.0
};

inline constexpr std::array<MoleculeInfo, 10> kMolecules = {{
    {"molecular hydrogen", "H2", 1.0},      // most abundant molecule in the universe
    {"carbon monoxide", "CO", 1.0e-4},      // standard CO/H2 ~ 1e-4
    {"water", "H2O", 1.0e-4},               // dense-gas value (can be a few e-4)
    {"hydroxyl", "OH", 3.0e-5},
    {"molecular nitrogen", "N2", 2.0e-5},
    {"carbon dioxide", "CO2", 1.0e-5},
    {"methane", "CH4", 1.0e-6},
    {"ammonia", "NH3", 1.0e-7},
    // Solid/ice and refractory notes (condensed phases on grains); abundances
    // are nominal grain-mantle / refractory fractions, far below gas-phase H2.
    {"silicate (rock)", "SiO4", 1.0e-8},    // amorphous/crystalline silicate grains
    {"water ice", "H2O(s)", 1.0e-6},        // dominant grain-mantle ice
}};

// Look up a molecule by chemical formula. Returns nullptr if unknown.
inline const MoleculeInfo* molecule(const char* formula) {
    if (formula == nullptr) return nullptr;
    for (const auto& m : kMolecules) {
        const char* a = m.formula;
        const char* b = formula;
        while (*a != '\0' && *b != '\0' && *a == *b) { ++a; ++b; }
        if (*a == '\0' && *b == '\0') return &m;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Chemical bonding classification
// ---------------------------------------------------------------------------

enum class BondType {
    Ionic,
    Covalent,
    Metallic,
    VanDerWaals,
    Hydrogen,
};

// Classify a two-atom bond from Pauling electronegativities. Thresholds follow
// the standard general-chemistry convention:
//   |dEN| > 1.7        -> Ionic
//   0.4 <= |dEN| <= 1.7-> (polar) Covalent
//   |dEN| < 0.4        -> (nonpolar) Covalent
// As a special case, a pair of very low-electronegativity atoms (both < ~1.4,
// i.e. metals) with negligible difference shares delocalised electrons:
//   -> Metallic
inline BondType classify_bond(double electronegativity_a,
                              double electronegativity_b) {
    const double den = std::fabs(electronegativity_a - electronegativity_b);
    if (den > 1.7) return BondType::Ionic;
    // Low-EN pair with small difference -> metallic bonding.
    if (den < 0.4 && electronegativity_a < 1.4 && electronegativity_b < 1.4) {
        return BondType::Metallic;
    }
    // Everything else from 0 up to 1.7 is a covalent bond (polar or nonpolar).
    return BondType::Covalent;
}

// ---------------------------------------------------------------------------
// States of matter / simple T-P phase diagram
// ---------------------------------------------------------------------------

enum class Phase {
    Solid,
    Liquid,
    Gas,
    Supercritical,
    Plasma,
};

// A deterministic, intentionally simple phase model.
//
//   - At or above the ionization temperature -> Plasma (overrides all else).
//   - Above the critical temperature AND critical pressure -> Supercritical.
//   - Below the melting point -> Solid.
//   - Between melt and the (pressure-adjusted) boiling point -> Liquid.
//   - Above the boiling point -> Gas.
//
// The boiling point is nudged with pressure (Clausius-Clapeyron-like, log in
// pressure) so that higher pressure raises the boil temperature; the shift is
// clamped to keep the ordering sane.
inline Phase phase_of(double temp_k, double pressure_atm, double melt_k,
                      double boil_k, double crit_temp_k, double crit_press_atm,
                      double ionization_k) {
    // Plasma dominates once matter is ionized.
    if (temp_k >= ionization_k) return Phase::Plasma;

    // Supercritical fluid: beyond both critical T and critical P.
    if (temp_k >= crit_temp_k && pressure_atm >= crit_press_atm) {
        return Phase::Supercritical;
    }

    // Pressure-adjusted boiling point. At 1 atm the shift is zero; the boil
    // temperature rises modestly with log10(pressure) and is clamped so it can
    // neither drop below the melting point nor exceed the critical temperature.
    const double p = pressure_atm > 1.0e-6 ? pressure_atm : 1.0e-6;
    const double span = (crit_temp_k > boil_k) ? (crit_temp_k - boil_k) : 0.0;
    double boil_adj = boil_k + std::log10(p) * (0.15 * boil_k);
    boil_adj = cosmos::clampd(boil_adj, melt_k, boil_k + span);

    if (temp_k < melt_k) return Phase::Solid;
    if (temp_k < boil_adj) return Phase::Liquid;
    return Phase::Gas;
}

// ---------------------------------------------------------------------------
// Condensation ("snow line") temperatures
// ---------------------------------------------------------------------------

// Rough equilibrium condensation temperature (K) below which a species
// condenses to a solid in a cooling protoplanetary nebula. Returns -1.0 for an
// unknown species. Refractories (iron, rock/silicates) condense hottest; ices
// condense coldest. Values from the equilibrium condensation sequence (Lodders
// 2003) and the standard snow-line picture.
inline double condensation_temperature_k(const char* species) {
    if (species == nullptr) return -1.0;
    const std::string s(species);
    if (s == "iron" || s == "Fe") return 1600.0;          // metal, hottest
    if (s == "silicate" || s == "rock" || s == "SiO4") return 1400.0; // ~1300-1500 K
    if (s == "water" || s == "H2O" || s == "ice") return 170.0;       // water snow line
    if (s == "CO2" || s == "carbon dioxide") return 80.0;             // CO2 frost line
    return -1.0;
}

// ---------------------------------------------------------------------------
// Matter regime by mass density (kg/m^3)
// ---------------------------------------------------------------------------

// Qualitative regime, marking where molecular chemistry gives way to bulk /
// condensed / degenerate matter. Thresholds (order-of-magnitude):
//   density < 1e-3                  -> "diffuse gas"   (rarefied ISM / nebula)
//   1e-3 <= density < 1e3           -> "molecular cloud" (dense gas, grain chemistry)
//   1e3  <= density < 1e9           -> "condensed matter" (bulk solid/liquid)
//   density >= 1e9                  -> "degenerate matter" (white-dwarf-like)
inline const char* matter_regime(double density_kg_m3) {
    if (density_kg_m3 < 1.0e-3) return "diffuse gas";
    if (density_kg_m3 < 1.0e3) return "molecular cloud";
    if (density_kg_m3 < 1.0e9) return "condensed matter";
    return "degenerate matter";
}

// Convenience water constants for callers / tests (CRC Handbook).
inline constexpr double kWaterMeltK = 273.15;
inline constexpr double kWaterBoilK = 373.15;
inline constexpr double kWaterCritTempK = 647.0;
inline constexpr double kWaterCritPressAtm = 218.0;

} // namespace cosmos::chem

#endif // COSMOS_CHEMISTRY_HPP
