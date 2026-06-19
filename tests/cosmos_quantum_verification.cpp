// Verifies the quantum & Planck-scale instruments of the SUBATOMIC tier:
//   - cosmos/QuantumScale.hpp: Planck units re-derived, Compton/de Broglie
//     wavelengths, the Compton-Schwarzschild Planck crossover, Heisenberg
//     bounds, the quantum harmonic oscillator, the Bohr/hydrogen ladder, and
//     decay width <-> lifetime.
//   - cosmos/Constants.hpp: the new elementary-mass and atomic anchors are
//     self-consistent with their defining formulas.
//   - cosmos/ParticleData.hpp: Standard Model classification predicates.
//
// Every numeric target is a textbook/CODATA/PDG value, cited inline.

#include "cosmos/Constants.hpp"
#include "cosmos/ParticleData.hpp"
#include "cosmos/QuantumScale.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::quantum;
using namespace cosmos::constants;
namespace pd = cosmos::particles;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_quantum_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}

void close(double got, double want, double rel, const std::string& what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    const double err = std::abs(got - want) / denom;
    if (err > rel) {
        std::cerr << "cosmos_quantum_verification FAILED: " << what
                  << " got=" << got << " want=" << want << " rel=" << err << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- Constants self-consistency -----------------------------------------
    // m_p / m_e must reproduce the stored dimensionless ratio.
    close(proton_mass_kg / electron_mass_kg, proton_electron_mass_ratio, 1e-5,
          "m_p/m_e ~ 1836.15");
    check(neutron_mass_kg > proton_mass_kg, "m_n > m_p (free neutron can decay)");
    check(electron_volt_J == e, "1 eV == elementary charge (numerically, by definition)");

    // Bohr radius and Rydberg energy re-derived from m_e, c, alpha must match the
    // stored CODATA anchors.
    close(bohr_radius_derived_m(), bohr_radius_m, 1e-4, "a_0 = hbar/(m_e c alpha)");
    close(rydberg_energy_derived_J(), rydberg_energy_J, 1e-4,
          "Ry = alpha^2 m_e c^2 / 2");

    // --- Planck units: derived == stored ------------------------------------
    const PlanckUnits pu = planck_units();
    close(pu.length_m, planck_length_m, 1e-4, "Planck length derived");
    close(pu.time_s, planck_time_s, 1e-4, "Planck time derived");
    close(pu.mass_kg, planck_mass_kg, 1e-4, "Planck mass derived");
    close(pu.energy_J, planck_energy_J, 1e-4, "Planck energy derived");
    close(pu.temperature_K, planck_temp_K, 1e-4, "Planck temperature derived");

    // The Planck length is the floor of scale.
    check(in_planck_lengths(planck_length_m) > 0.99 &&
              in_planck_lengths(planck_length_m) < 1.01,
          "l_P measures 1 Planck length");
    check(above_planck_floor(1.0e-30), "1e-30 m is above the Planck floor");
    check(!above_planck_floor(1.0e-40), "1e-40 m is below the Planck floor");

    // --- Mass <-> energy ----------------------------------------------------
    // Electron rest energy is 0.511 MeV.
    close(rest_energy_mev(electron_mass_kg), 0.5109989, 1e-3, "m_e c^2 = 0.511 MeV");
    close(rest_energy_mev(proton_mass_kg), 938.272, 1e-3, "m_p c^2 = 938.27 MeV");
    // Round-trip mass <-> energy.
    close(mass_from_energy_mev(rest_energy_mev(electron_mass_kg)), electron_mass_kg,
          1e-9, "mass<->energy round trip");
    // Relativistic energy reduces to rest energy at p=0 and to p c for m=0.
    close(relativistic_energy_J(electron_mass_kg, 0.0), rest_energy_J(electron_mass_kg),
          1e-12, "E(p=0) = m c^2");
    close(relativistic_energy_J(0.0, 5.0), 5.0 * c, 1e-12, "E(m=0) = p c");

    // --- Compton & de Broglie wavelengths -----------------------------------
    // Electron Compton wavelength = 2.426e-12 m; reduced = 3.862e-13 m.
    close(compton_wavelength_m(electron_mass_kg), 2.42631e-12, 1e-3,
          "electron Compton wavelength");
    close(reduced_compton_wavelength_m(electron_mass_kg), 3.8616e-13, 1e-3,
          "electron reduced Compton wavelength");
    // lambda_C = 2 pi * lambdabar_C.
    close(compton_wavelength_m(electron_mass_kg),
          2.0 * pi * reduced_compton_wavelength_m(electron_mass_kg), 1e-9,
          "lambda_C = 2 pi lambdabar_C");
    // Heavier particle -> shorter Compton wavelength.
    check(compton_wavelength_m(proton_mass_kg) < compton_wavelength_m(electron_mass_kg),
          "heavier proton has shorter Compton wavelength");
    // Massless -> infinite Compton wavelength.
    check(std::isinf(compton_wavelength_m(0.0)), "massless Compton wavelength is infinite");

    // The Bohr radius is the electron reduced Compton wavelength divided by alpha.
    close(reduced_compton_wavelength_m(electron_mass_kg) / alpha, bohr_radius_m, 1e-3,
          "a_0 = lambdabar_C(e) / alpha");

    // de Broglie: faster -> shorter wavelength; thermal wavelength grows as T falls.
    check(de_broglie_wavelength_nr_m(electron_mass_kg, 2.0e6) <
              de_broglie_wavelength_nr_m(electron_mass_kg, 1.0e6),
          "de Broglie shrinks with speed");
    check(thermal_de_broglie_m(electron_mass_kg, 1.0) >
              thermal_de_broglie_m(electron_mass_kg, 300.0),
          "thermal de Broglie grows as T falls");

    // --- Gravity meets the quantum ------------------------------------------
    // Schwarzschild radius scales linearly with mass and is tiny for the Sun-ish
    // test mass; a 1 Msun (~2e30 kg) hole is ~2.95 km.
    close(schwarzschild_radius_m(1.989e30), 2950.0, 2e-2, "r_s(1 Msun) ~ 2.95 km");
    check(schwarzschild_radius_m(2.0e30) > schwarzschild_radius_m(1.0e30),
          "r_s grows with mass");
    // The Compton-Schwarzschild crossover sits at the Planck mass / sqrt(2).
    close(compton_schwarzschild_crossover_mass_kg(), planck_mass_kg / std::sqrt(2.0),
          1e-3, "Compton=Schwarzschild crossover = m_P/sqrt(2)");
    // Hawking temperature: smaller holes are hotter; a Planck-mass hole ~ T_P/(8 pi).
    check(hawking_temperature_K(1.0e30) < hawking_temperature_K(1.0e29),
          "smaller black holes are hotter");
    close(hawking_temperature_K(planck_mass_kg), planck_temp_K / (8.0 * pi), 1e-3,
          "T_H(m_P) = T_P / (8 pi)");

    // --- Heisenberg uncertainty ---------------------------------------------
    // Confining an electron to a_0 forces a momentum spread; the implied
    // dx*dp saturates the bound hbar/2.
    const double dp = min_momentum_uncertainty(bohr_radius_m);
    close(bohr_radius_m * dp, 0.5 * hbar, 1e-9, "dx*dp_min saturates hbar/2");
    check(satisfies_uncertainty(bohr_radius_m, dp), "saturated pair satisfies bound");
    check(satisfies_uncertainty(bohr_radius_m, 2.0 * dp), "looser pair satisfies bound");
    check(!satisfies_uncertainty(bohr_radius_m, 0.4 * dp),
          "over-tight pair violates bound");
    // Energy-time form: a shorter window allows a larger energy spread.
    check(min_energy_uncertainty_J(1.0e-21) > min_energy_uncertainty_J(1.0e-18),
          "shorter dt -> larger dE");

    // --- Quantum harmonic oscillator ----------------------------------------
    const double omega = 1.0e15; // rad/s, optical-ish
    close(qho_level_energy_J(0, omega), qho_zero_point_energy_J(omega), 1e-12,
          "E_0 = zero-point energy");
    close(qho_zero_point_energy_J(omega), 0.5 * hbar * omega, 1e-12,
          "zero-point = hbar omega / 2");
    // Levels are evenly spaced by hbar omega.
    close(qho_level_energy_J(3, omega) - qho_level_energy_J(2, omega), hbar * omega,
          1e-12, "QHO levels spaced by hbar omega");
    check(qho_level_energy_J(0, omega) > 0.0, "vacuum still carries zero-point energy");

    // --- Bohr / hydrogen ----------------------------------------------------
    // Ground state -13.6 eV; energy rises toward 0 with n.
    close(hydrogen_level_energy_ev(1), -13.6057, 1e-3, "H ground state -13.6 eV");
    close(hydrogen_level_energy_ev(2), -3.4014, 2e-3, "H n=2 = -3.40 eV");
    check(hydrogen_level_energy_ev(2) > hydrogen_level_energy_ev(1),
          "H energy rises with n");
    // Lyman-alpha (2->1) ~ 10.2 eV; Balmer-alpha (3->2) ~ 1.89 eV.
    close(hydrogen_transition_ev(1, 2), 10.204, 2e-3, "Lyman-alpha ~ 10.2 eV");
    close(hydrogen_transition_ev(2, 3), 1.889, 3e-3, "Balmer-alpha ~ 1.89 eV");
    // Z^2 scaling: He+ (Z=2) ground state is 4x deeper than hydrogen.
    close(hydrogen_level_energy_ev(1, 2), 4.0 * hydrogen_level_energy_ev(1, 1), 1e-9,
          "hydrogenic energy scales as Z^2");
    // Photon energy <-> wavelength round trip; Lyman-alpha ~ 121.6 nm.
    close(photon_wavelength_m(ev_to_joules(10.204)), 121.5e-9, 5e-3,
          "Lyman-alpha wavelength ~ 121.6 nm");
    close(photon_energy_J(photon_wavelength_m(3.0e-19)), 3.0e-19, 1e-9,
          "photon energy<->wavelength round trip");

    // --- Decay: width <-> lifetime (tau = hbar / Gamma) ---------------------
    // Round trip through the relation.
    close(lifetime_s_from_width_mev(decay_width_mev_from_lifetime(kMuonLifetime_s)),
          kMuonLifetime_s, 1e-9, "muon width<->lifetime round trip");
    // The Z boson's 2.495 GeV width implies a ~2.6e-25 s lifetime (PDG).
    close(lifetime_s_from_width_mev(kZWidth_mev), 2.638e-25, 1e-2,
          "Z lifetime from its width ~ 2.64e-25 s");
    // Broader resonance <=> shorter life: the Z (2.495 GeV) is broader than the
    // W (2.085 GeV) and so is the shorter-lived of the two.
    check(lifetime_s_from_width_mev(kZWidth_mev) < lifetime_s_from_width_mev(kWWidth_mev),
          "broader Z is shorter-lived than W");
    check(decay_width_mev_from_lifetime(kTauLifetime_s) >
              decay_width_mev_from_lifetime(kMuonLifetime_s),
          "shorter-lived tau has a broader width than the muon");
    // Stable particle (zero width) -> infinite lifetime.
    check(std::isinf(lifetime_s_from_width_mev(0.0)), "zero width -> stable (infinite tau)");

    // --- Standard Model classification --------------------------------------
    check(pd::is_quark(pd::Particle::Up) && pd::is_quark(pd::Particle::Top),
          "up and top are quarks");
    check(!pd::is_quark(pd::Particle::Electron), "electron is not a quark");
    check(pd::is_charged_lepton(pd::Particle::Electron) &&
              pd::is_charged_lepton(pd::Particle::Tau),
          "electron and tau are charged leptons");
    check(pd::is_neutrino(pd::Particle::NeutrinoMu), "nu_mu is a neutrino");
    check(pd::is_lepton(pd::Particle::Electron) && pd::is_lepton(pd::Particle::NeutrinoE),
          "electron and nu_e are leptons");
    check(pd::is_gauge_boson(pd::Particle::Photon) && pd::is_gauge_boson(pd::Particle::Gluon),
          "photon and gluon are gauge bosons");
    check(pd::is_scalar_boson(pd::Particle::Higgs), "Higgs is the scalar boson");

    // Fermions have half-integer spin; bosons integer spin.
    check(pd::is_fermion(pd::Particle::Electron) && pd::is_fermion(pd::Particle::Up),
          "electron and up are fermions");
    check(pd::is_boson(pd::Particle::Photon) && pd::is_boson(pd::Particle::Higgs),
          "photon and Higgs are bosons");
    check(pd::is_fermion(pd::Particle::Electron) != pd::is_boson(pd::Particle::Electron),
          "fermion and boson partition the particle");

    // Colour charge is carried only by quarks and the gluon (confinement).
    check(pd::carries_colour(pd::Particle::Up) && pd::carries_colour(pd::Particle::Gluon),
          "quarks and gluon carry colour");
    check(!pd::carries_colour(pd::Particle::Electron) &&
              !pd::carries_colour(pd::Particle::Photon),
          "electron and photon are colourless");

    // Stability: lightest matter + massless bosons are stable; heavy ones decay.
    check(pd::is_stable(pd::Particle::Electron) && pd::is_stable(pd::Particle::Photon) &&
              pd::is_stable(pd::Particle::NeutrinoE),
          "electron, photon, neutrino are stable");
    check(!pd::is_stable(pd::Particle::Muon) && !pd::is_stable(pd::Particle::Top) &&
              !pd::is_stable(pd::Particle::Higgs),
          "muon, top, Higgs are unstable");

    // --- Determinism / purity -----------------------------------------------
    check(bohr_radius_derived_m() == bohr_radius_derived_m(), "a_0 deterministic");
    check(hydrogen_level_energy_ev(3) == hydrogen_level_energy_ev(3), "H level deterministic");
    check(compton_wavelength_m(electron_mass_kg) == compton_wavelength_m(electron_mass_kg),
          "Compton deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_quantum_verification: " << g_failures
                  << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_quantum_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
