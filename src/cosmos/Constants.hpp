#pragma once
// Real physical constants (SI). Used for DISPLAY-side computed physics only —
// the dimensionless N-body sandbox dynamics are unaffected.

namespace cosmos {
namespace constants {

constexpr double G    = 6.67430e-11;     // gravitational constant, m^3 kg^-1 s^-2
constexpr double c    = 2.99792458e8;    // speed of light, m/s
constexpr double c2   = c * c;           // c^2, m^2/s^2
constexpr double kB   = 1.380649e-23;    // Boltzmann constant, J/K
constexpr double hbar = 1.054571817e-34; // reduced Planck constant, J s
constexpr double pi   = 3.14159265358979323846;

} // namespace constants
} // namespace cosmos
