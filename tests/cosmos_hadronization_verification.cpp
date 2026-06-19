// Verifies cosmos/Hadronization.hpp: building colour-singlet hadrons from quark
// content and reading off their additive quantum numbers (charge, baryon number,
// strangeness), the colour-singlet rule, the Gell-Mann-Nishijima cross-check, and
// the naive mass ordering p < Lambda < Omega.

#include "cosmos/Hadronization.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::qcd;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_hadronization_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_hadronization_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    const Flavour u = Flavour::Up, d = Flavour::Down, s = Flavour::Strange;

    // --- Baryons: charge, baryon number, strangeness ------------------------
    const HadronContent proton = baryon(u, u, d);
    const HadronContent neutron = baryon(u, d, d);
    const HadronContent lambda = baryon(u, d, s);
    const HadronContent omega = baryon(s, s, s);
    const HadronContent deltapp = baryon(u, u, u);

    close(hadron_charge(proton), +1.0, 1e-9, "proton (uud) charge +1");
    close(hadron_charge(neutron), 0.0, 1e-9, "neutron (udd) charge 0");
    close(hadron_charge(omega), -1.0, 1e-9, "Omega- (sss) charge -1");
    close(hadron_charge(deltapp), +2.0, 1e-9, "Delta++ (uuu) charge +2");
    close(baryon_number(proton), 1.0, 1e-9, "proton B = 1");
    close(strangeness(lambda), -1, 1e-9, "Lambda strangeness -1");
    close(strangeness(omega), -3, 1e-9, "Omega strangeness -3");
    close(strangeness(proton), 0, 1e-9, "proton strangeness 0");

    // --- Mesons -------------------------------------------------------------
    const HadronContent pip = meson(u, d); // u dbar = pi+
    const HadronContent kp = meson(u, s);  // u sbar = K+
    const HadronContent k0 = meson(d, s);  // d sbar = K0
    close(hadron_charge(pip), +1.0, 1e-9, "pi+ (u dbar) charge +1");
    close(hadron_charge(kp), +1.0, 1e-9, "K+ (u sbar) charge +1");
    close(hadron_charge(k0), 0.0, 1e-9, "K0 (d sbar) charge 0");
    close(baryon_number(pip), 0.0, 1e-9, "meson B = 0");
    close(strangeness(kp), +1, 1e-9, "K+ strangeness +1 (anti-s)");

    // --- Colour-singlet rule ------------------------------------------------
    check(is_meson(pip) && is_colour_singlet(pip), "pi+ is a colour-singlet meson");
    check(is_baryon(proton) && is_colour_singlet(proton), "proton is a colour-singlet baryon");
    check(!is_meson(proton), "proton is not a meson");
    check(!is_baryon(pip), "meson is not a baryon");
    // A quark-quark pair (qq, not q qbar) is NOT a singlet.
    HadronContent diquark;
    diquark.add(u, +1);
    diquark.add(d, +1);
    check(!is_colour_singlet(diquark), "diquark (uu) is not a free colour singlet");

    // --- Gell-Mann-Nishijima cross-check ------------------------------------
    // The implied I_3 from Q - (B+S)/2 must be a sensible half-integer/integer.
    close(implied_isospin_3(proton), +0.5, 1e-9, "proton I_3 = +1/2");
    close(implied_isospin_3(neutron), -0.5, 1e-9, "neutron I_3 = -1/2");
    close(implied_isospin_3(deltapp), +1.5, 1e-9, "Delta++ I_3 = +3/2");

    // --- Naive mass ordering ------------------------------------------------
    check(naive_mass_mev(proton) < naive_mass_mev(lambda), "m(p) < m(Lambda)");
    check(naive_mass_mev(lambda) < naive_mass_mev(omega), "m(Lambda) < m(Omega)");
    check(naive_mass_mev(pip) < naive_mass_mev(kp), "m(pi) < m(K)");

    // --- Determinism --------------------------------------------------------
    check(hadron_charge(proton) == hadron_charge(proton), "charge deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_hadronization_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_hadronization_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
