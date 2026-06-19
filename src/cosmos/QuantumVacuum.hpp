// QuantumVacuum.hpp -- the physics of "empty" space: zero-point fields are real
// and measurable. Casimir attraction between plates, the Schwinger critical
// field where the vacuum sparks into electron-positron pairs, the Unruh thermal
// bath an accelerated observer sees, and the vacuum-energy / cosmological-
// constant problem. These are the QFT phenomena of the smallest tier.
//
// Header-only, pure, deterministic. SI units unless a name says otherwise.
//
// Sources:
//   - Casimir (1948): P = -pi^2 hbar c / (240 d^4) between ideal plates.
//   - Sauter-Schwinger critical field E_c = m_e^2 c^3 / (e hbar) ~ 1.32e18 V/m.
//   - Unruh (1976): T = hbar a / (2 pi c k_B).
//   - Zero-point energy density with a momentum cutoff; the Planck-cutoff value
//       exceeds the observed dark-energy density by ~120 orders (the c.c. problem).

#ifndef COSMOS_QUANTUMVACUUM_HPP
#define COSMOS_QUANTUMVACUUM_HPP

#include "cosmos/Constants.hpp"

#include <cmath>

namespace cosmos {
namespace vac {

using namespace cosmos::constants;

// ---------------------------------------------------------------------------
// Casimir effect -- attraction from excluded vacuum modes between two plates.
// ---------------------------------------------------------------------------

// Casimir pressure between two perfectly conducting plates a distance d apart:
//   P = -pi^2 hbar c / (240 d^4)   (negative = attractive). [Pa]
inline double casimir_pressure_Pa(double plate_gap_m) {
    if (plate_gap_m <= 0.0)
        return -INFINITY;
    const double d4 = plate_gap_m * plate_gap_m * plate_gap_m * plate_gap_m;
    return -(pi * pi * hbar * c) / (240.0 * d4);
}

// Casimir energy per unit area: u = -pi^2 hbar c / (720 d^3). [J/m^2]
inline double casimir_energy_per_area(double plate_gap_m) {
    if (plate_gap_m <= 0.0)
        return -INFINITY;
    const double d3 = plate_gap_m * plate_gap_m * plate_gap_m;
    return -(pi * pi * hbar * c) / (720.0 * d3);
}

// ---------------------------------------------------------------------------
// Schwinger limit -- where a static E-field rips pairs out of the vacuum.
// ---------------------------------------------------------------------------

// Critical electric field E_c = m_e^2 c^3 / (e hbar) ~ 1.323e18 V/m.
inline double schwinger_critical_field_Vm() {
    const double me = electron_mass_kg;
    return me * me * c * c * c / (e * hbar);
}

// Critical magnetic field B_c = E_c / c ~ 4.41e9 T (the QED "critical field").
inline double schwinger_critical_field_T() {
    return schwinger_critical_field_Vm() / c;
}

// Leading nonperturbative pair-production rate factor exp(-pi E_c / E): vanishes
// far below E_c, turns on sharply as E -> E_c. Dimensionless suppression in [0,1].
inline double schwinger_suppression(double field_Vm) {
    if (field_Vm <= 0.0)
        return 0.0;
    return std::exp(-pi * schwinger_critical_field_Vm() / field_Vm);
}

// ---------------------------------------------------------------------------
// Unruh effect -- acceleration looks like temperature.
// ---------------------------------------------------------------------------

// Unruh temperature seen by an observer with proper acceleration a:
//   T = hbar a / (2 pi c k_B). [K]
inline double unruh_temperature_K(double acceleration_ms2) {
    if (acceleration_ms2 <= 0.0)
        return 0.0;
    return hbar * acceleration_ms2 / (2.0 * pi * c * kB);
}

// ---------------------------------------------------------------------------
// Vacuum energy density -- and why it is the worst prediction in physics.
// ---------------------------------------------------------------------------

// Zero-point energy density of a field summed up to a momentum cutoff k_max
// (in 1/m): rho ~ (hbar c / (16 pi^2)) k_max^4. [J/m^3]
inline double vacuum_energy_density(double k_max_inv_m) {
    if (k_max_inv_m <= 0.0)
        return 0.0;
    const double k4 = k_max_inv_m * k_max_inv_m * k_max_inv_m * k_max_inv_m;
    return (hbar * c / (16.0 * pi * pi)) * k4;
}

// The vacuum energy density at the Planck cutoff (k_max = 1/l_P). Astronomically
// larger than the observed dark-energy density (~6e-10 J/m^3): the ~120-orders
// "cosmological constant problem".
inline double planck_cutoff_vacuum_density() {
    const double lP = std::sqrt(hbar * G / (c * c * c));
    return vacuum_energy_density(1.0 / lP);
}

// Observed dark-energy density (Planck 2018), for the dramatic comparison. [J/m^3]
inline constexpr double kObservedDarkEnergy_Jm3 = 6.0e-10;

} // namespace vac
} // namespace cosmos

#endif // COSMOS_QUANTUMVACUUM_HPP
