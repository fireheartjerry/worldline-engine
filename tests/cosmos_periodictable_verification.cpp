// Verifies cosmos/PeriodicTable.hpp: Aufbau/Madelung electron configurations,
// valence counting, period/block assignment, noble gases, and the measured
// periodic trends (ionization energy, atomic radius, electronegativity).

#include "cosmos/PeriodicTable.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::periodic;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_periodictable_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Electron configurations (Madelung) ---------------------------------
    check(configuration_string(1) == "1s1", "H = 1s1");
    check(configuration_string(2) == "1s2", "He = 1s2");
    check(configuration_string(6) == "1s2 2s2 2p2", "C = 1s2 2s2 2p2");
    check(configuration_string(10) == "1s2 2s2 2p6", "Ne = 1s2 2s2 2p6");
    check(configuration_string(11) == "1s2 2s2 2p6 3s1", "Na = ...3s1");
    // The Madelung rule fills 4s before 3d: K = [Ar] 4s1.
    check(configuration_string(19) == "1s2 2s2 2p6 3s2 3p6 4s1", "K = ...4s1 (4s before 3d)");
    // Total electrons conserved.
    {
        const auto cfg = electron_configuration(26);
        int total = 0;
        for (const auto &s : cfg)
            total += s.occupancy;
        check(total == 26, "Fe configuration holds 26 electrons");
    }

    // --- Valence, period, block ---------------------------------------------
    check(valence_electrons(1) == 1, "H has 1 valence electron");
    check(valence_electrons(6) == 4, "C has 4 valence electrons");
    check(valence_electrons(8) == 6, "O has 6 valence electrons");
    check(valence_electrons(10) == 8, "Ne has 8 (full octet)");
    check(valence_electrons(11) == 1, "Na has 1 valence electron");
    check(period(1) == 1 && period(2) == 1, "H, He in period 1");
    check(period(3) == 2 && period(11) == 3 && period(19) == 4, "period assignment");
    check(block(1) == 's' && block(6) == 'p' && block(26) == 'd', "s/p/d blocks");

    // --- Noble gases --------------------------------------------------------
    check(is_noble_gas(2) && is_noble_gas(10) && is_noble_gas(18) && is_noble_gas(36),
          "noble gases recognised");
    check(!is_noble_gas(11) && !is_noble_gas(6), "non-noble elements rejected");

    // --- Periodic trends ----------------------------------------------------
    const Element *Li = element(3);
    const Element *F = element(9);
    const Element *Na = element(11);
    const Element *Cl = element(17);
    const Element *K = element(19);
    const Element *C = element(6);
    check(Li && F && Na && Cl && K && C, "elements present in table");
    // Ionization energy: rises across a period, falls down a group.
    check(F->ionization_ev > Li->ionization_ev, "IE rises across period 2 (Li -> F)");
    check(Li->ionization_ev > Na->ionization_ev && Na->ionization_ev > K->ionization_ev,
          "IE falls down group 1 (Li > Na > K)");
    // Atomic radius: shrinks across a period, grows down a group.
    check(F->radius_pm < Li->radius_pm, "radius shrinks across a period");
    check(K->radius_pm > Na->radius_pm && Na->radius_pm > Li->radius_pm,
          "radius grows down a group");
    // Electronegativity: rises across, falls down.
    check(F->electronegativity > C->electronegativity &&
              C->electronegativity > Li->electronegativity,
          "electronegativity rises across a period");
    check(F->electronegativity > Cl->electronegativity, "EN falls down the halogens");
    // Fluorine is the most electronegative element.
    check(F->electronegativity > 3.9, "fluorine most electronegative (~3.98)");

    // --- Lookup -------------------------------------------------------------
    check(element(1000) == nullptr, "unknown Z returns null");
    check(std::string(element(26)->symbol) == "Fe", "Z=26 is iron");

    // --- Determinism --------------------------------------------------------
    check(configuration_string(11) == configuration_string(11), "configuration deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_periodictable_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_periodictable_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
