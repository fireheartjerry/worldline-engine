// Verifies the SUBATOMIC..ATOMIC physics-data tables in cosmos/ParticleData.hpp:
//   - Standard Model particle mass ordering by generation, charges, masslessness.
//   - Bethe-Weizsaecker SEMF: iron-group peak of binding energy per nucleon.
//   - Big Bang nucleosynthesis primordial abundances and their ordering.
//   - Asplund (2009) solar photospheric abundance ordering.
//   - Electron shell / subshell capacities.
//   - Determinism / purity of the pure functions.

#include "cosmos/ParticleData.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::particles;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_particledata_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}

double mass(Particle p) { return particle_info(p).mass_mev; }
double charge(Particle p) { return particle_info(p).charge_e; }

} // namespace

int main() {
    // --- Particle masses ordered by generation within a family ---------------
    check(mass(Particle::Electron) < mass(Particle::Muon), "e < mu");
    check(mass(Particle::Muon) < mass(Particle::Tau), "mu < tau");
    check(mass(Particle::Up) < mass(Particle::Charm), "u < c");
    check(mass(Particle::Charm) < mass(Particle::Top), "c < t");
    check(mass(Particle::Down) < mass(Particle::Strange), "d < s");
    check(mass(Particle::Strange) < mass(Particle::Bottom), "s < b");

    // --- Charges -------------------------------------------------------------
    check(std::abs(charge(Particle::Up) - 2.0 / 3.0) < 1e-12, "up charge +2/3");
    check(std::abs(charge(Particle::Down) + 1.0 / 3.0) < 1e-12, "down charge -1/3");
    check(std::abs(charge(Particle::Electron) + 1.0) < 1e-12, "electron charge -1");
    check(std::abs(charge(Particle::NeutrinoE)) < 1e-12, "nu_e neutral");
    check(std::abs(charge(Particle::Photon)) < 1e-12, "photon neutral");
    check(std::abs(charge(Particle::ZBoson)) < 1e-12, "Z neutral");
    check(std::abs(charge(Particle::WBoson) - 1.0) < 1e-12, "W charge +1");

    // --- Massless gauge bosons ----------------------------------------------
    check(mass(Particle::Photon) == 0.0, "photon massless");
    check(mass(Particle::Gluon) == 0.0, "gluon massless");

    // Generation tags for fermions are 1/2/3; bosons 0.
    check(particle_info(Particle::Electron).generation == 1, "electron gen 1");
    check(particle_info(Particle::Top).generation == 3, "top gen 3");
    check(particle_info(Particle::Higgs).generation == 0, "higgs gen 0");
    check(particle_info(Particle::Higgs).spin == 0.0, "higgs scalar (spin 0)");
    check(particle_info(Particle::Photon).spin == 1.0, "photon spin 1");

    // --- SEMF: iron-group peak in binding energy per nucleon -----------------
    const double bpnFe = binding_per_nucleon(26, 56);
    const double bpnHe = binding_per_nucleon(2, 4);
    const double bpnC = binding_per_nucleon(6, 12);
    const double bpnU = binding_per_nucleon(92, 238);
    const double bpnD = binding_per_nucleon(1, 2);

    check(bpnFe > bpnHe, "BE/A Fe-56 > He-4");
    check(bpnFe > bpnC, "BE/A Fe-56 > C-12");
    check(bpnFe > bpnU, "BE/A Fe-56 > U-238");
    check(bpnFe > bpnD, "BE/A Fe-56 > deuterium");
    check(bpnFe > 8.5 && bpnFe < 9.0, "BE/A Fe-56 in [8.5, 9.0]");

    // most_bound_A should land in the iron group (A ~ 56-62).
    const int peakA = most_bound_A();
    check(peakA >= 56 && peakA <= 62, "most_bound_A in iron group [56,62]");

    // --- Big Bang nucleosynthesis -------------------------------------------
    check(std::abs(kPrimordialH_massfrac + kPrimordialHe_massfrac - 1.0) < 1e-9,
          "H + He mass fractions ~= 1");
    check(std::abs(kPrimordialH_massfrac - 0.75) < 1e-9, "H mass fraction ~= 0.75");
    check(kDH > kHe3H, "D/H > 3He/H");
    check(kHe3H > kLi7H, "3He/H > 7Li/H");

    // --- Asplund 2009 solar abundances --------------------------------------
    check(solar_log_abundance("H") == 12.00, "solar H = 12.00");
    check(solar_log_abundance("Xx") == -1.0, "unknown symbol returns -1");
    const double aH = solar_log_abundance("H");
    const double aHe = solar_log_abundance("He");
    const double aO = solar_log_abundance("O");
    const double aC = solar_log_abundance("C");
    const double aNe = solar_log_abundance("Ne");
    const double aFe = solar_log_abundance("Fe");
    check(aH > aHe, "H > He");
    check(aHe > aO, "He > O");
    check(aO > aC, "O > C");
    check(aC > aNe, "C > Ne");
    check(aO > aFe, "O > Fe");

    // --- Oddo-Harkins --------------------------------------------------------
    check(oddo_harkins_even_favored(8), "even Z favored");
    check(more_abundant_adjacent(8, 7) == 8, "even (8) favored over 7");
    check(more_abundant_adjacent(11, 12) == 12, "even (12) favored over 11");

    // --- Shell / subshell capacities ----------------------------------------
    check(shell_capacity(1) == 2, "shell n=1 -> 2");
    check(shell_capacity(2) == 8, "shell n=2 -> 8");
    check(shell_capacity(3) == 18, "shell n=3 -> 18");
    check(shell_capacity(4) == 32, "shell n=4 -> 32");
    check(subshell_capacity(0) == 2, "subshell s -> 2");
    check(subshell_capacity(1) == 6, "subshell p -> 6");
    check(subshell_capacity(2) == 10, "subshell d -> 10");
    check(subshell_capacity(3) == 14, "subshell f -> 14");

    // --- Determinism / purity ------------------------------------------------
    check(semf_binding_mev(26, 56) == semf_binding_mev(26, 56), "SEMF deterministic");
    check(binding_per_nucleon(26, 56) == binding_per_nucleon(26, 56),
          "BE/A deterministic");
    check(solar_log_abundance("Fe") == solar_log_abundance("Fe"),
          "solar lookup deterministic");
    check(shell_capacity(3) == shell_capacity(3), "shell_capacity deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_particledata_verification: " << g_failures
                  << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_particledata_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
