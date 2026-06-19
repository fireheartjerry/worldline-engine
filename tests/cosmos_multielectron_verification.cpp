// Verifies cosmos/MultiElectronAtoms.hpp: Slater screening / effective nuclear
// charge, Hund's rules for ground-state terms, multiplicity, and the Aufbau
// exceptions.

#include "cosmos/MultiElectronAtoms.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::multielectron;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_multielectron_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_multielectron_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Slater screening / Z_eff -------------------------------------------
    close(z_effective(1), 1.0, 1e-9, "H Z_eff = 1");
    close(z_effective(2), 1.70, 1e-9, "He Z_eff = 1.70");
    close(z_effective(3), 1.30, 1e-9, "Li (2s) Z_eff = 1.30");
    close(z_effective(11), 2.20, 1e-9, "Na (3s) Z_eff = 2.20");
    // Z_eff rises across a period (more protons, weak same-shell screening).
    check(z_effective(9) > z_effective(3), "Z_eff rises across period 2 (Li -> F)");
    // Valence electrons feel far less than the full nuclear charge.
    check(z_effective(11) < 11.0 && z_effective(11) > 1.0, "Na valence well screened");

    // --- Hund's rules -------------------------------------------------------
    // Carbon 2p^2 -> ^3P_0 : S=1, L=1, J=0.
    {
        const Term t = hunds_ground_term(1, 2);
        close(t.S, 1.0, 1e-9, "C p^2: S=1");
        check(t.L == 1, "C p^2: L=1");
        close(t.J, 0.0, 1e-9, "C p^2: J=0 (less than half full)");
        check(term_multiplicity(t) == 3, "C p^2 is a triplet");
    }
    // Nitrogen 2p^3 -> ^4S_3/2 : half-filled, S=3/2, L=0.
    {
        const Term t = hunds_ground_term(1, 3);
        close(t.S, 1.5, 1e-9, "N p^3: S=3/2");
        check(t.L == 0, "N p^3: L=0");
        close(t.J, 1.5, 1e-9, "N p^3: J=3/2");
        check(term_multiplicity(t) == 4, "N p^3 is a quartet");
    }
    // Oxygen 2p^4 -> ^3P_2 : more than half full, J = L+S.
    {
        const Term t = hunds_ground_term(1, 4);
        close(t.S, 1.0, 1e-9, "O p^4: S=1");
        check(t.L == 1, "O p^4: L=1");
        close(t.J, 2.0, 1e-9, "O p^4: J=2 (more than half full)");
    }
    // Half-filled d^5 -> ^6S_5/2 (Mn-like): max spin, zero orbital.
    {
        const Term t = hunds_ground_term(2, 5);
        close(t.S, 2.5, 1e-9, "d^5: S=5/2");
        check(t.L == 0, "d^5: L=0");
        check(term_multiplicity(t) == 6, "d^5 is a sextet");
    }
    // Closed subshell -> singlet S (1S0).
    {
        const Term t = hunds_ground_term(1, 6);
        close(t.S, 0.0, 1e-9, "closed p^6: S=0");
        check(t.L == 0 && term_multiplicity(t) == 1, "closed shell is a singlet S");
    }

    // --- Aufbau exceptions --------------------------------------------------
    check(is_aufbau_exception(24), "chromium is an Aufbau exception");
    check(is_aufbau_exception(29), "copper is an Aufbau exception");
    check(is_aufbau_exception(47) && is_aufbau_exception(79), "Ag, Au are exceptions");
    check(!is_aufbau_exception(26), "iron follows the Aufbau rule");
    check(!is_aufbau_exception(20), "calcium follows the Aufbau rule");

    // --- Successive ionization ----------------------------------------------
    // A big jump comes after the valence electrons are stripped (sodium: 1).
    check(big_ionization_jump_after(1, 1), "Na: big jump after the 1st ionization");
    check(!big_ionization_jump_after(2, 1), "Mg: no jump after only the 1st");

    // --- Determinism --------------------------------------------------------
    check(z_effective(11) == z_effective(11), "Z_eff deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_multielectron_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_multielectron_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
