// QuantumGenesis.hpp -- the GENERATION STEP for the first layer of existence.
//
// Given a universe's law genome, synthesize the entire particle-physics content
// of that universe deterministically: the effective fundamental couplings, the
// neutron-proton mass split, whether protons / deuterons / di-protons are stable,
// whether hydrogen and heavy atoms can exist at all, the primordial light-element
// abundances, the colour-singlet hadron spectrum, and the early-universe epoch
// timeline from the Planck era down to recombination -- plus an anthropic verdict
// on whether this universe can build complex matter.
//
// The physics knobs are toy but physically motivated: each is anchored so that a
// genome of all-1.0 reproduces our universe (n-p split 1.29 MeV, deuteron bound
// at 2.22 MeV, di-proton unbound, Y_He ~ 0.25), and small genome drifts push the
// universe through the real anthropic boundaries (the deuteron bottleneck, the
// di-proton catastrophe, relativistic-collapse of heavy atoms, proton decay).
//
// Header-only, pure, deterministic. Depends only on header-only physics modules.
//
// Sources for the anthropic windows:
//   - n-p split as a competition of (m_d - m_u) vs EM self-energy (~+2.05 / -0.76 MeV).
//   - Deuteron bottleneck & di-proton catastrophe (Dyson 1971; Barnes 2012,
//       "The Fine-Tuning of the Universe for Intelligent Life").
//   - Relativistic instability of inner electrons at Z*alpha ~ 1.
//   - Big Bang nucleosynthesis helium fraction Y = 2(n/p)/(1 + n/p).

#ifndef COSMOS_QUANTUMGENESIS_HPP
#define COSMOS_QUANTUMGENESIS_HPP

#include "cosmos/Constants.hpp"
#include "cosmos/Hadronization.hpp"
#include "cosmos/LawGenome.hpp"
#include "cosmos/StandardModel.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cosmos {
namespace genesis {

// --- Anchored model coefficients (genome = 1.0 reproduces our universe) -------
inline constexpr double kQuarkSplit_mev = 2.05;  // (m_d - m_u) raises the neutron
inline constexpr double kEMSplit_mev = 0.76;     // EM self-energy raises the proton
inline constexpr double kDeuteronBE_mev = 2.224; // our deuteron binding energy
inline constexpr double kDiprotonGap_mev = 0.70; // how far the di-proton is from binding
inline constexpr double kNuclearSens = 30.0;     // MeV per unit strong-coupling drift
inline constexpr double kElectronMass_mev = 0.511;
inline constexpr double kFreezeTemp_mev = 0.658; // effective n/p freeze-out (-> Y~0.247)

// One stage of the early universe.
struct CosmicEpoch {
    const char *name;
    double energy_GeV;    // characteristic thermal energy
    double temperature_K; // kT
    double time_s;        // age of the universe at this stage
    const char *note;
};

// The synthesized particle-physics profile of a universe.
struct QuantumUniverse {
    // Effective fundamental parameters.
    double alpha_em_eff;
    double alpha_inv_eff;
    double lambda_qcd_mev;
    double higgs_vev_gev;
    double np_mass_diff_mev; // m_n - m_p
    double deuteron_binding_mev;
    double diproton_binding_mev; // negative => unbound (as in our universe)
    int max_stable_Z;            // heaviest atom before Z*alpha >= 1

    // Viability flags.
    bool electron_stable;
    bool proton_stable;
    bool free_neutron_decays;
    bool deuteron_bound;
    bool diproton_bound; // true => stellar runaway (bad)
    bool hydrogen_forms;
    bool carbon_stable;
    bool heavy_atoms_stable; // up to uranium (Z=92)
    bool bbn_possible;
    bool chemistry_possible;

    // Primordial light-element mass fractions.
    double primordial_H;
    double primordial_He;

    // Hadron spectrum (colour-singlet states built from u/d/s).
    int meson_count;
    int baryon_count;

    double complexity_score; // [0,1]
    std::string verdict;
};

// Number of light colour-singlet hadrons built from {u,d,s}: the qqbar meson
// nonet (9) and the qqq baryon states (10 with repetition). Deterministic.
inline void light_hadron_counts(int &mesons, int &baryons) {
    using namespace qcd;
    const Flavour fl[3] = {Flavour::Up, Flavour::Down, Flavour::Strange};
    mesons = 0;
    baryons = 0;
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            if (is_colour_singlet(meson(fl[a], fl[b])))
                ++mesons;
    for (int a = 0; a < 3; ++a)
        for (int b = a; b < 3; ++b)
            for (int cc = b; cc < 3; ++cc)
                if (is_colour_singlet(baryon(fl[a], fl[b], fl[cc])))
                    ++baryons;
}

// The headline generator: genome -> full quantum-tier profile.
inline QuantumUniverse synthesize(const LawGenome &g) {
    QuantumUniverse u;

    // Effective couplings.
    u.alpha_em_eff = constants::alpha * g.coupling_em;
    u.alpha_inv_eff = 1.0 / u.alpha_em_eff;
    u.lambda_qcd_mev = 1000.0 * sm::kLambdaQCD_GeV * g.coupling_strong;
    u.higgs_vev_gev = sm::kHiggsVEV_GeV * g.mass_scale;

    // Neutron-proton mass split: quark-mass term (scales with the mass knob)
    // competes with EM self-energy (scales with the EM coupling).
    u.np_mass_diff_mev = kQuarkSplit_mev * g.mass_scale - kEMSplit_mev * g.coupling_em;

    // Nuclear binding shifts linearly with the strong-coupling drift; the
    // deuteron and di-proton sit on opposite sides of the binding threshold.
    const double drift = (g.coupling_strong - 1.0) * kNuclearSens;
    u.deuteron_binding_mev = kDeuteronBE_mev + drift;
    u.diproton_binding_mev = -kDiprotonGap_mev + drift;

    // Heaviest atom whose innermost electron stays non-relativistic (Z alpha < 1).
    u.max_stable_Z = static_cast<int>(std::floor(1.0 / u.alpha_em_eff));

    // Stability logic.
    u.electron_stable = true; // lightest charged lepton; charge conservation
    // Proton beta-plus decays only if m_p > m_n + m_e  <=>  np_diff < -m_e.
    u.proton_stable = u.np_mass_diff_mev > -kElectronMass_mev;
    // Free neutron beta decays if m_n > m_p + m_e  <=>  np_diff > +m_e.
    u.free_neutron_decays = u.np_mass_diff_mev > kElectronMass_mev;
    u.deuteron_bound = u.deuteron_binding_mev > 0.0;
    u.diproton_bound = u.diproton_binding_mev > 0.0;
    u.hydrogen_forms = u.proton_stable && u.electron_stable;
    u.carbon_stable = u.max_stable_Z >= 6;
    u.heavy_atoms_stable = u.max_stable_Z >= 92;
    // BBN needs a bound deuteron (the bottleneck) and stable protons, and it
    // must NOT short-circuit through a bound di-proton.
    u.bbn_possible = u.deuteron_bound && u.proton_stable && !u.diproton_bound;
    u.chemistry_possible = u.hydrogen_forms && u.carbon_stable;

    // Primordial abundances from the neutron fraction at freeze-out.
    const double r = std::exp(-u.np_mass_diff_mev / kFreezeTemp_mev);
    double Y = 2.0 * r / (1.0 + r);
    Y = std::clamp(Y, 0.0, 1.0);
    u.primordial_He = Y;
    u.primordial_H = 1.0 - Y;

    light_hadron_counts(u.meson_count, u.baryon_count);

    // Complexity score: fraction of the key viability gates that pass, nudged by
    // the genome's structural stability bias.
    const bool gates[] = {u.electron_stable,    u.proton_stable,  u.deuteron_bound,
                          !u.diproton_bound,    u.hydrogen_forms, u.carbon_stable,
                          u.heavy_atoms_stable, u.bbn_possible,   u.chemistry_possible};
    int passed = 0;
    for (bool b : gates)
        passed += b ? 1 : 0;
    const double base =
        static_cast<double>(passed) / static_cast<double>(sizeof(gates) / sizeof(gates[0]));
    u.complexity_score = std::clamp(base * (0.6 + 0.4 * g.stability_bias), 0.0, 1.0);

    // A human verdict, keyed on the first/most-severe failure.
    if (!u.proton_stable) {
        u.verdict = "Proton-unstable: no hydrogen, no atoms -- a barren universe.";
    } else if (u.diproton_bound) {
        u.verdict = "Di-proton bound: stars flash and die -- no slow stellar burning.";
    } else if (!u.deuteron_bound) {
        u.verdict = "Deuteron unbound: the BBN bottleneck never opens -- only hydrogen.";
    } else if (!u.carbon_stable) {
        u.verdict = "EM too strong: heavy atoms collapse -- chemistry truncated.";
    } else if (!u.heavy_atoms_stable) {
        u.verdict = "Periodic table truncated below uranium, but life-chemistry survives.";
    } else {
        u.verdict = "Complex-matter friendly: stable nuclei, full chemistry, slow stars.";
    }
    return u;
}

// The thermal history of the early universe, with characteristic stages scaled by
// the genome (the electroweak scale follows the Higgs VEV, the quark-hadron
// transition follows Lambda_QCD). Energies in GeV; temperatures via kT.
inline std::vector<CosmicEpoch> cosmic_timeline(const LawGenome &g) {
    const QuantumUniverse u = synthesize(g);
    // kT [K] from energy [GeV]: E / k_B with E in joules.
    auto K_of_GeV = [](double E_gev) {
        return E_gev * 1.0e9 * constants::electron_volt_J / constants::kB;
    };
    const double ew_gev = 0.1 * u.higgs_vev_gev;      // ~ electroweak scale
    const double qcd_gev = u.lambda_qcd_mev * 1.7e-3; // quark-hadron ~ 1.7*Lambda
    const double bbn_gev = 0.1e-3;                    // ~0.1 MeV
    const double rec_gev = 0.26e-9;                   // ~0.26 eV

    std::vector<CosmicEpoch> t;
    t.push_back({"Planck", 1.22e19, K_of_GeV(1.22e19), constants::planck_time_s,
                 "Quantum gravity; spacetime itself is uncertain."});
    t.push_back({"Grand unification", 1.0e16, K_of_GeV(1.0e16), 1.0e-36,
                 "Strong and electroweak forces merge; inflation ends."});
    t.push_back({"Electroweak", ew_gev, K_of_GeV(ew_gev), 1.0e-11,
                 "Higgs switches on; W/Z gain mass, fermions gain mass."});
    t.push_back({"Quark-hadron", qcd_gev, K_of_GeV(qcd_gev), 1.0e-5,
                 "Quarks confine into protons and neutrons."});
    t.push_back({"Nucleosynthesis", bbn_gev, K_of_GeV(bbn_gev), 180.0,
                 "Light nuclei form; the deuteron bottleneck gates the rest."});
    t.push_back({"Recombination", rec_gev, K_of_GeV(rec_gev), 1.2e13,
                 "Electrons bind to nuclei; the universe turns transparent."});
    return t;
}

} // namespace genesis
} // namespace cosmos

#endif // COSMOS_QUANTUMGENESIS_HPP
