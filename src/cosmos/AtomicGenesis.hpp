// AtomicGenesis.hpp -- the GENERATION STEP for the atomic tier. Given a universe's
// law genome, synthesize its chemistry: the extent of the periodic table (how
// heavy an atom can exist before its inner electrons turn relativistic and
// collapse), the atomic energy and size scales (which set bond strengths and the
// temperature window of chemistry), whether the CHNOPS elements of life can
// exist, and an anthropic verdict on whether rich chemistry is possible at all.
//
// Anchored so an all-1.0 genome reproduces our universe: alpha ~ 1/137, a periodic
// table reaching ~Z 118-137, Rydberg 13.6 eV, Bohr radius 52.9 pm, and the full
// set of biologically essential elements.
//
// Header-only, pure, deterministic. Depends only on header-only atomic modules.
//
// Sources:
//   - Relativistic limit of the periodic table Z ~ 1/alpha ~ 137 (the "feynmanium"
//       bound where the 1s electron velocity reaches c).
//   - Rydberg ~ alpha^2 m_e c^2, Bohr radius ~ hbar/(alpha m_e c): both scale with
//       the EM coupling and the electron mass.
//   - CHNOPS: H, C, N, O, P, S as the backbone elements of terrestrial life.

#ifndef COSMOS_ATOMICGENESIS_HPP
#define COSMOS_ATOMICGENESIS_HPP

#include "cosmos/AtomicStructure.hpp"
#include "cosmos/LawGenome.hpp"
#include "cosmos/PeriodicTable.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace cosmos {
namespace atomgen {

// The CHNOPS backbone elements of life, by atomic number.
inline constexpr std::array<int, 6> kLifeElements = {1, 6, 7, 8, 15, 16}; // H C N O P S

struct AtomicUniverse {
    double alpha_eff;         // effective fine-structure constant
    int max_stable_Z;         // heaviest atom before relativistic 1s collapse
    double rydberg_ev;        // atomic energy scale
    double bohr_radius_pm;    // atomic size scale
    double bond_energy_scale; // relative to our universe (sets chemistry temperature)

    bool hydrogen_stable;
    bool carbon_available;
    bool life_elements_available;   // all of CHNOPS exist
    bool rich_periodic_table;       // reaches the transition metals (Z >= 50)
    bool full_periodic_table;       // reaches uranium (Z >= 92)
    bool distinct_metals_nonmetals; // halogens exist (Z >= 17) -> ionic chemistry

    int available_element_count;
    double complexity_score; // [0,1]
    std::string verdict;
};

// The headline generator: genome -> atomic / chemical profile.
inline AtomicUniverse synthesize(const LawGenome &g) {
    AtomicUniverse u;

    // Effective EM coupling and the relativistic periodic-table bound Z ~ 1/alpha.
    u.alpha_eff = constants::alpha * g.coupling_em;
    u.max_stable_Z = static_cast<int>(std::floor(1.0 / u.alpha_eff));

    // Rydberg ~ alpha^2 m_e ; Bohr radius ~ 1/(alpha m_e). Both move with the EM
    // coupling and the electron-mass knob (mass_scale).
    u.rydberg_ev = atom::kRydberg_eV * (g.coupling_em * g.coupling_em) * g.mass_scale;
    u.bohr_radius_pm = atom::kBohrRadius_pm / std::max(1e-6, g.coupling_em * g.mass_scale);
    // Bond energies track the Rydberg scale (chemistry's natural energy).
    u.bond_energy_scale = u.rydberg_ev / atom::kRydberg_eV;

    // Element availability gates.
    u.hydrogen_stable = u.max_stable_Z >= 1;
    u.carbon_available = u.max_stable_Z >= 6;
    u.life_elements_available = true;
    for (int z : kLifeElements)
        if (z > u.max_stable_Z)
            u.life_elements_available = false;
    u.rich_periodic_table = u.max_stable_Z >= 50;
    u.full_periodic_table = u.max_stable_Z >= 92;
    u.distinct_metals_nonmetals = u.max_stable_Z >= 17; // need halogens for ionic bonds
    u.available_element_count = std::min(u.max_stable_Z, 118);

    const bool gates[] = {u.hydrogen_stable,         u.carbon_available,
                          u.life_elements_available, u.rich_periodic_table,
                          u.full_periodic_table,     u.distinct_metals_nonmetals};
    int passed = 0;
    for (bool b : gates)
        passed += b ? 1 : 0;
    const double base = static_cast<double>(passed) / 6.0;
    u.complexity_score = std::clamp(base * (0.6 + 0.4 * g.stability_bias), 0.0, 1.0);

    if (!u.carbon_available) {
        u.verdict = "EM far too strong: only the lightest atoms exist -- no chemistry.";
    } else if (!u.life_elements_available) {
        u.verdict = "Periodic table truncated below sulfur: the CHNOPS set is incomplete.";
    } else if (!u.distinct_metals_nonmetals) {
        u.verdict = "No halogens: ionic chemistry is impossible, only weak covalent bonds.";
    } else if (!u.rich_periodic_table) {
        u.verdict = "Light-element chemistry only: no transition metals or catalysis.";
    } else if (!u.full_periodic_table) {
        u.verdict = "Rich organic chemistry, but the periodic table stops short of uranium.";
    } else {
        u.verdict = "Full chemistry: the complete periodic table and all elements of life.";
    }
    return u;
}

// Convenience: is a given atomic number a CHNOPS life element?
inline bool is_life_element(int Z) {
    for (int z : kLifeElements)
        if (z == Z)
            return true;
    return false;
}

} // namespace atomgen
} // namespace cosmos

#endif // COSMOS_ATOMICGENESIS_HPP
