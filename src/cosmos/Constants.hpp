#pragma once
// Real physical constants (SI) and the quantities derived from an object's
// mass and radius. These are used for DISPLAY ONLY — the dimensionless N-body
// sandbox dynamics are unaffected. They let the inspector show genuine,
// computed physics (density, Schwarzschild radius, escape velocity) instead of
// hard-coded numbers, so the catalogue's SI anchors actually mean something.

#include <algorithm>
#include <cmath>

namespace cosmos {
namespace constants {

constexpr double G    = 6.67430e-11;     // gravitational constant, m^3 kg^-1 s^-2
constexpr double c    = 2.99792458e8;    // speed of light, m/s
constexpr double c2   = c * c;           // c^2, m^2/s^2
constexpr double kB   = 1.380649e-23;    // Boltzmann constant, J/K
constexpr double hbar = 1.054571817e-34; // reduced Planck constant, J s
constexpr double pi   = 3.14159265358979323846;

} // namespace constants

// Quantities computed from an object's SI mass and radius. All are exact for a
// uniform sphere; relativistic objects (compactness -> 1) are where the
// Schwarzschild radius approaches the physical radius.
struct DerivedQuantities {
    double density_kg_m3     = 0.0; // M / (4/3 pi r^3)
    double schwarzschild_m   = 0.0; // r_s = 2 G M / c^2
    double escape_velocity_ms = 0.0; // sqrt(2 G M / r)
    double surface_gravity_ms2 = 0.0; // G M / r^2
    double compactness       = 0.0; // r_s / r, in [0,1] (-> 1 for a black hole)
};

inline DerivedQuantities derive_quantities(double mass_kg, double radius_m) {
    using namespace constants;
    DerivedQuantities d;
    d.schwarzschild_m = 2.0 * G * mass_kg / c2;
    if (radius_m > 0.0) {
        const double volume = (4.0 / 3.0) * pi * radius_m * radius_m * radius_m;
        d.density_kg_m3      = mass_kg / volume;
        d.surface_gravity_ms2 = G * mass_kg / (radius_m * radius_m);
        // Escape speed, capped at c (an object with r < r_s is a black hole).
        d.escape_velocity_ms = std::min(c, std::sqrt(std::max(0.0, 2.0 * G * mass_kg / radius_m)));
        d.compactness        = std::clamp(d.schwarzschild_m / radius_m, 0.0, 1.0);
    }
    return d;
}

} // namespace cosmos
