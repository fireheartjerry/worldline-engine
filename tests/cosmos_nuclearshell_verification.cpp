// Verifies cosmos/NuclearShell.hpp: the magic numbers, doubly-magic nuclei, the
// shell-model level ordering and its cumulative occupancies, ground-state
// spin-parity from the last unpaired nucleon, and the pairing gap.

#include "cosmos/NuclearShell.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::shell;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_nuclearshell_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Magic numbers ------------------------------------------------------
    for (int m : {2, 8, 20, 28, 50, 82, 126})
        check(is_magic(m), "magic number recognised");
    check(!is_magic(58) && !is_magic(40), "non-magic numbers rejected");

    // Doubly-magic nuclei: He-4, O-16, Ca-40, Ca-48, Pb-208.
    check(is_doubly_magic(2, 2), "He-4 doubly magic");
    check(is_doubly_magic(8, 8), "O-16 doubly magic");
    check(is_doubly_magic(20, 20), "Ca-40 doubly magic");
    check(is_doubly_magic(20, 28), "Ca-48 doubly magic");
    check(is_doubly_magic(82, 126), "Pb-208 doubly magic");
    check(!is_doubly_magic(26, 30), "Fe-56 is not doubly magic");

    // --- Shell-model level ordering -----------------------------------------
    // The cumulative occupancy must hit every magic number exactly at a shell gap.
    const Orbital *lv = shell_levels();
    bool hit2 = false, hit8 = false, hit20 = false, hit28 = false, hit50 = false, hit82 = false,
         hit126 = false;
    for (std::size_t i = 0; i < shell_level_count(); ++i) {
        switch (lv[i].cumulative) {
        case 2:
            hit2 = true;
            break;
        case 8:
            hit8 = true;
            break;
        case 20:
            hit20 = true;
            break;
        case 28:
            hit28 = true;
            break;
        case 50:
            hit50 = true;
            break;
        case 82:
            hit82 = true;
            break;
        case 126:
            hit126 = true;
            break;
        default:
            break;
        }
        // Each orbital holds 2j+1 nucleons.
        check(lv[i].capacity == lv[i].two_j + 1, "orbital capacity = 2j+1");
    }
    check(hit2 && hit8 && hit20 && hit28 && hit50 && hit82 && hit126,
          "cumulative occupancy reproduces all magic numbers");

    // --- Ground-state spin-parity -------------------------------------------
    // Even-even nuclei are 0+.
    {
        const SpinParity sp = ground_state_spin_parity(8, 8); // O-16
        check(sp.two_j == 0 && sp.parity == +1, "O-16 ground state 0+");
    }
    // O-17 (Z=8, N=9): the 9th neutron sits in 1d5/2 -> 5/2+.
    {
        const SpinParity sp = ground_state_spin_parity(8, 9);
        check(sp.two_j == 5 && sp.parity == +1, "O-17 ground state 5/2+");
    }
    // The 9th nucleon is in an l=2 (d) orbital -> positive parity.
    check(orbital_for_nucleon(9).l == 2, "9th nucleon in a d orbital");
    // The 29th nucleon is just past the 28 shell, in 2p3/2 -> j=3/2.
    check(valence_two_j(29) == 3, "29th nucleon is 2p3/2 (j=3/2)");

    // --- Pairing ------------------------------------------------------------
    // Pairing gap ~ 12/sqrt(A): smaller for heavier nuclei.
    check(pairing_gap_mev(16) > pairing_gap_mev(208), "pairing gap shrinks with A");
    check(std::abs(pairing_gap_mev(144) - 1.0) < 1e-9, "pairing gap(A=144) = 1 MeV");
    check(shell_closure_count(82, 126) == 2, "Pb-208 has two closed shells");
    check(shell_closure_count(26, 30) == 0, "Fe-56 has no closed shell");

    // --- Determinism --------------------------------------------------------
    check(pairing_gap_mev(56) == pairing_gap_mev(56), "pairing deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_nuclearshell_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_nuclearshell_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
