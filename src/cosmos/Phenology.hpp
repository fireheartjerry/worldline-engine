#pragma once
// Deterministic climate / seasonality module — the abiotic forcing function the
// live ecosystem sim consumes. Given a region's latitude, the world's axial
// tilt, and continentality, it produces a closed-form annual temperature cycle
// plus the phenological responses (breeding pulses, metabolic dormancy,
// seasonal primary-production modulation) that drive populations. Everything is
// a pure, deterministic function of its arguments — no state, no RNG — so the
// same region under the same orbit always breathes the same way.

#include "cosmos/Spectrum.hpp"

#include <cmath>

namespace cosmos {
namespace pheno {

// A region's reduced annual climate: a sinusoidal temperature cycle described by
// its mean, its peak-to-mean seasonal amplitude, and a phase (season lag).
struct Climate {
    double mean_temp_c;
    double seasonal_amp_c;
    double phase;
};

inline double deg2rad(double d) { return d * (kTwoPi / 360.0); }

// Solar declination over the year: delta = tilt * sin(2*pi*day/year). Degrees.
// Its magnitude can never exceed the axial tilt.
inline double declination(double axial_tilt_deg, double day_of_year, double year_length) {
    const double yl = year_length > 1.0e-9 ? year_length : 1.0;
    return axial_tilt_deg * std::sin(kTwoPi * day_of_year / yl);
}

// Relative top-of-atmosphere daily insolation at a latitude on a given day.
// Uses the standard cos(latitude - declination) geometry; polar night (sun below
// the horizon) clamps to 0. Result is in roughly [0,1].
inline double insolation_factor(double latitude_deg, double axial_tilt_deg,
                                double day_of_year, double year_length) {
    const double delta = declination(axial_tilt_deg, day_of_year, year_length);
    const double s = std::cos(deg2rad(latitude_deg - delta));
    return clampd(s, 0.0, 1.0);
}

// Annual-mean insolation as a function of latitude, energy-balance-model form:
//   S(phi) = 1 - 0.482 * P2(sin phi),  P2(x) = (3x^2 - 1)/2.
// Normalized to ~[0,1]: equator (max) -> 1, poles (min) -> 0.
inline double annual_mean_insolation(double latitude_deg) {
    const double s = std::sin(deg2rad(latitude_deg));
    const double p2 = (3.0 * s * s - 1.0) * 0.5;
    const double raw = 1.0 - 0.482 * p2;          // max at equator (s=0): 1.241
    const double hi = 1.0 - 0.482 * (-0.5);        // equator (p2=-0.5):   1.241
    // Normalize by the equatorial maximum: equator -> 1, pole -> ~0.42 (the pole
    // still receives a substantial, strictly positive annual insolation).
    return clampd(raw / hi, 0.0, 1.0);
}

// Build a region's annual climate from its base (equatorial) temperature, its
// latitude, the world's axial tilt, and continentality (0 = maritime, 1 =
// deep continental interior).
inline Climate region_climate(double base_temp_c, double latitude_deg,
                              double axial_tilt_deg, double continentality) {
    const double abslat = std::fabs(latitude_deg);
    const double cont = clampd(continentality, 0.0, 1.0);

    // Mean temperature falls from base at the equator to base-40 at the poles,
    // following the annual-mean insolation gradient.
    const double ins = annual_mean_insolation(latitude_deg);
    // Remap the [pole..equator] insolation band to [0,1] for the thermal gradient
    // so the equator stays at base and the pole reaches base-40.
    const double ins_pole = (1.0 - 0.482) / (1.0 - 0.482 * (-0.5)); // ~0.417
    const double grad = clampd((ins - ins_pole) / (1.0 - ins_pole), 0.0, 1.0);
    const double mean = lerp(base_temp_c - 40.0, base_temp_c, grad);

    // Seasonal swing grows with latitude and continentality: high-latitude
    // continental interiors swing the most; tropical maritime regions the least.
    const double amp = (2.0 + 0.35 * abslat) * (0.4 + 0.6 * cont);

    // Axial tilt scales how pronounced seasons are at all (a near-zero tilt
    // world has almost no seasonality regardless of latitude).
    const double tilt_scale = clampd(axial_tilt_deg / 23.44, 0.0, 2.0);

    Climate c;
    c.mean_temp_c = mean;
    c.seasonal_amp_c = amp * tilt_scale;
    c.phase = 0.6; // fixed season lag (radians)
    return c;
}

// Temperature at fractional year t (in years) for a given climate.
inline double seasonal_temp(const Climate& c, double t_years) {
    return c.mean_temp_c + c.seasonal_amp_c * std::sin(kTwoPi * t_years - c.phase);
}

// Reproductive trigger: a Gaussian in temperature that peaks (==1) when the
// current temperature matches the species' thermal optimum and decays away.
// Returns a value in [0,1].
inline double breeding_pulse(double species_T_opt, double current_temp_c, double width_c) {
    const double w = width_c > 1.0e-9 ? width_c : 1.0e-9;
    const double d = (current_temp_c - species_T_opt) / w;
    return clampd(std::exp(-0.5 * d * d), 0.0, 1.0);
}

// Metabolic activity multiplier in (0,1]. Ectotherms (low endothermy) slow
// sharply when cold via an Arrhenius-like response; endotherms (high endothermy)
// hold activity near 1 regardless of ambient temperature.
inline double dormancy_factor(double endothermy, double current_temp_c, double T_opt) {
    const double endo = clampd(endothermy, 0.0, 1.0);

    // Arrhenius-like factor relative to the optimum, in kelvin, with a Q10-style
    // sensitivity. Cooler than optimum -> activity < 1; warmer -> approaches 1.
    const double Topt_k = T_opt + 273.15;
    const double Tcur_k = current_temp_c + 273.15;
    const double tk = Tcur_k > 1.0 ? Tcur_k : 1.0;
    const double Ea = 6000.0; // activation-temperature scale (K)
    double ecto = std::exp(Ea * (1.0 / Topt_k - 1.0 / tk));
    ecto = clampd(ecto, 0.0, 1.0);

    // Endotherms blend toward full activity; ectotherms follow the cold response.
    const double a = lerp(ecto, 1.0, endo);
    return clampd(a, 1.0e-6, 1.0);
}

// Producer carrying-capacity modulation from present-day insolation. A gentle,
// bounded multiplier so the live sim's equilibrium biomass breathes seasonally
// but can never blow up: maps insolation in [0,1] to roughly [0.5, 1.2].
inline double npp_seasonal_factor(double insolation_now) {
    const double i = clampd(insolation_now, 0.0, 1.0);
    const double s = smoothstep(0.0, 1.0, i); // smooth, bounded ramp
    return lerp(0.5, 1.2, s);
}

} // namespace pheno
} // namespace cosmos
