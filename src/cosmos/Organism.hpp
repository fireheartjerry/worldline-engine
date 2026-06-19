#pragma once
// Organism functional biology: a pure VIEW derived from a species' continuous
// traits + its climate, via the Metabolic Theory of Ecology (Brown et al. 2004)
// and the standard allometric scaling laws. Nothing here is stored on Species —
// it is computed on demand (e.g. for the creature inspector), so the ecosystem
// engine's byte layout and determinism are untouched.
//
// Scaling laws (M = body mass, kg):
//   metabolic rate   B  ∝ M^3/4 · e^(−E/kT)   (Kleiber + Arrhenius, E≈0.65 eV)
//   lifespan / ages     ∝ M^1/4               (life-history allometry)
//   von Bertalanffy  κ  ∝ M^−1/4              (growth-rate constant)
//   home range          ∝ M^1                 (Damuth home-range allometry)
//   max speed           ∝ M^0.17, saturating  (Hirt et al. 2017)
//   brain mass          ∝ M^3/4               (Jerison encephalization)
//   water flux          ∝ M^0.8
// All outputs are clamped to bounded physical ranges so a generator can never
// produce a nonsensical or unbounded organism.

#include "cosmos/Ecosystem.hpp" // eco::SpeciesTraits

#include <algorithm>
#include <cmath>

namespace cosmos {
namespace bio {

struct Organism {
    double bmr_w = 0.0;            // basal metabolic rate (W)
    double fmr_w = 0.0;            // field metabolic rate (~3× BMR)
    double lifespan_yr = 0.0;      // maximum lifespan
    double age_maturity_yr = 0.0;  // age at first reproduction
    double growth_k = 0.0;         // von Bertalanffy growth constant (1/yr)
    double mass_asymptotic_kg = 0.0;
    double fecundity = 0.0;        // offspring per reproductive event (relative)
    double endothermy = 0.0;       // ecto(0) <-> endo(1) thermoregulation continuum
    double home_range_m2 = 0.0;    // individual home-range area
    double speed_ms = 0.0;         // characteristic max speed
    double brain_g = 0.0;          // brain mass
    double water_flux_kg_day = 0.0;
};

namespace detail {
constexpr double kBoltzmann_eV = 8.617333e-5; // eV/K
constexpr double kActivation_eV = 0.65;        // metabolic activation energy
constexpr double kRefTempK = 293.15;           // 20 C reference

inline double clampd(double x, double lo, double hi) {
    return std::min(hi, std::max(lo, x));
}
// Arrhenius factor relative to the 20 C reference (ectotherm temperature scaling).
inline double arrhenius(double T_K) {
    return std::exp(-kActivation_eV / (kBoltzmann_eV * std::max(120.0, T_K)) +
                    kActivation_eV / (kBoltzmann_eV * kRefTempK));
}
} // namespace detail

// Derive the full organism from its traits and environment temperature (C).
inline Organism derive_organism(const eco::SpeciesTraits& t, double T_env_c) {
    using namespace detail;
    const double M = clampd(std::pow(10.0, t.m), 1.0e-9, 1.0e6); // body mass, kg
    const double T_K = std::max(120.0, t.T_opt + 273.15);

    Organism o;
    o.mass_asymptotic_kg = M;

    // Endothermy: large + metabolically "hot" (high kappa) animals regulate their
    // own temperature; tiny / low-kappa animals are ectotherms. Smooth in [0,1].
    const double endo_drive = 0.55 * (t.m + 2.0) + 1.3 * (t.kappa - 1.0);
    o.endothermy = clampd(0.5 + 0.5 * std::tanh(endo_drive), 0.0, 1.0);

    // Metabolism: Kleiber M^0.75 scaled by kappa, with an Arrhenius temperature
    // term that matters for ectotherms but is largely regulated away for endotherms.
    const double kleiber = 3.4 * std::pow(M, 0.75) * t.kappa; // W, mammalian-ish coeff
    const double temp_factor = (1.0 - o.endothermy) * arrhenius(T_K) + o.endothermy * 1.0;
    o.bmr_w = clampd(kleiber * temp_factor, 1.0e-6, 1.0e9);
    o.fmr_w = o.bmr_w * (2.0 + o.endothermy); // field rate above basal

    // Life history ∝ M^0.25, stretched/compressed by the r<->K axis (rho: 1=fast/r).
    const double m025 = std::pow(M, 0.25);
    const double k_factor = 1.6 - 1.2 * t.rho; // K-strategists (low rho) live longer
    o.lifespan_yr = clampd(11.0 * m025 * k_factor, 0.02, 5000.0);
    o.age_maturity_yr = clampd(0.30 * o.lifespan_yr, 0.005, 2000.0);
    o.growth_k = clampd(0.95 * std::pow(M, -0.25), 1.0e-4, 50.0); // 1/yr (von Bertalanffy)

    // Fecundity: r-strategists make many small offspring, K-strategists few large.
    o.fecundity = clampd(std::pow(10.0, 0.5 + 3.0 * t.rho - 0.45 * t.m), 1.0, 1.0e7);

    // Home range ∝ M^1 (Damuth), larger for higher trophic levels (more area to feed).
    o.home_range_m2 = clampd(1.0e3 * M * (1.0 + 0.6 * t.tau), 1.0e-3, 1.0e12);

    // Speed: saturating power law (Hirt 2017) — rises with mass then plateaus/declines.
    const double v = 25.0 * std::pow(M, 0.17) * std::exp(-M / 6.0e5);
    o.speed_ms = clampd(v, 1.0e-4, 130.0);

    o.brain_g = clampd(11.0 * std::pow(M, 0.75), 1.0e-6, 1.0e7);          // Jerison
    o.water_flux_kg_day = clampd(0.05 * std::pow(M, 0.8), 1.0e-9, 1.0e6); // ∝ M^0.8
    return o;
}

} // namespace bio
} // namespace cosmos
