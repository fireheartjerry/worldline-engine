#pragma once
// Rigorous orbital / compact-object mechanics, computed from real SI constants.
// These are the canonical, verified formulas (see tests/cosmos_orbits_verification.cpp)
// used to give the inspector genuine numbers. DISPLAY ONLY — the dimensionless
// sandbox dynamics are unaffected.

#include "cosmos/Constants.hpp"

#include <algorithm>
#include <cmath>

namespace cosmos {
namespace orbits {

// Circular orbital speed at radius r about a central mass M:  v = sqrt(G M / r).
inline double circular_velocity(double M_kg, double r_m) {
    return (M_kg > 0.0 && r_m > 0.0) ? std::sqrt(constants::G * M_kg / r_m) : 0.0;
}

// Escape speed from radius r about mass M:  v = sqrt(2 G M / r).
inline double escape_velocity(double M_kg, double r_m) {
    return (M_kg > 0.0 && r_m > 0.0) ? std::sqrt(2.0 * constants::G * M_kg / r_m) : 0.0;
}

// Kepler's third law: period of a circular/elliptical orbit of semi-major axis a
// about mass M:  T = 2*pi*sqrt(a^3 / (G M)).
inline double orbital_period(double M_kg, double a_m) {
    return (M_kg > 0.0 && a_m > 0.0)
        ? 2.0 * constants::pi * std::sqrt(a_m * a_m * a_m / (constants::G * M_kg))
        : 0.0;
}

// Gravitational free-fall / dynamical time of a body of mean density rho:
//   t_dyn = sqrt(3*pi / (32 G rho)).
// Depends only on density, so it is the natural collapse timescale of structure.
inline double dynamical_time(double density_kg_m3) {
    return (density_kg_m3 > 0.0)
        ? std::sqrt(3.0 * constants::pi / (32.0 * constants::G * density_kg_m3))
        : 0.0;
}

// Schwarzschild radius of a non-rotating mass:  r_s = 2 G M / c^2.
inline double schwarzschild_radius(double M_kg) {
    return 2.0 * constants::G * M_kg / constants::c2;
}

} // namespace orbits

// Quantities computed from an object's SI mass and radius, treating it as a
// uniform sphere. Relativistic objects (compactness -> 1) are where the
// Schwarzschild radius approaches the physical radius.
struct DerivedQuantities {
    double density_kg_m3       = 0.0; // M / (4/3 pi r^3)
    double schwarzschild_m     = 0.0; // r_s = 2 G M / c^2
    double escape_velocity_ms  = 0.0; // sqrt(2 G M / r), capped at c
    double surface_gravity_ms2 = 0.0; // G M / r^2
    double compactness         = 0.0; // r_s / r, in [0,1] (-> 1 for a black hole)
    double dynamical_time_s    = 0.0; // sqrt(3 pi / (32 G rho))
    double surface_orbit_s     = 0.0; // period of a test particle skimming the surface
};

inline DerivedQuantities derive_quantities(double mass_kg, double radius_m) {
    using namespace constants;
    DerivedQuantities d;
    d.schwarzschild_m = orbits::schwarzschild_radius(mass_kg);
    if (radius_m > 0.0) {
        const double volume = (4.0 / 3.0) * pi * radius_m * radius_m * radius_m;
        d.density_kg_m3       = mass_kg / volume;
        d.surface_gravity_ms2 = G * mass_kg / (radius_m * radius_m);
        // Escape speed, capped at c (an object with r < r_s is a black hole).
        d.escape_velocity_ms  = std::min(c, orbits::escape_velocity(mass_kg, radius_m));
        d.compactness         = std::clamp(d.schwarzschild_m / radius_m, 0.0, 1.0);
        d.dynamical_time_s    = orbits::dynamical_time(d.density_kg_m3);
        d.surface_orbit_s     = orbits::orbital_period(mass_kg, radius_m);
    }
    return d;
}

} // namespace cosmos
