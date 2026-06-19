// Verifies that the cosmos orbital / compact-object mechanics reproduce known
// real-world values to high precision. This is the "mathematical optimality"
// guarantee: the formulas the inspector shows are provably correct physics, not
// hand-tuned numbers.

#include "cosmos/Orbits.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void check_close(const std::string& what, double got, double expected, double rel_tol) {
    const double denom = std::max(1.0e-300, std::abs(expected));
    const double rel = std::abs(got - expected) / denom;
    if (!(rel <= rel_tol)) {
        std::cerr << "cosmos_orbits_verification FAILED: " << what
                  << " got " << got << " expected " << expected
                  << " (rel err " << rel << " > " << rel_tol << ")\n";
        ++g_failures;
    }
}

void require(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_orbits_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    using namespace cosmos;

    // Reference SI values.
    const double M_sun   = 1.98892e30;   // kg
    const double M_earth = 5.972e24;     // kg
    const double R_sun   = 6.96e8;       // m
    const double R_earth = 6.371e6;      // m
    const double AU      = 1.495978707e11; // m
    const double year    = 3.15576e7;    // s (Julian year)

    // --- Circular velocity: Earth around the Sun is ~29.78 km/s ---------------
    check_close("earth orbital velocity",
                orbits::circular_velocity(M_sun, AU), 29784.0, 0.02);

    // --- Escape velocity: Earth surface ~11.19 km/s, Sun surface ~617.5 km/s ---
    check_close("earth escape velocity",
                orbits::escape_velocity(M_earth, R_earth), 11186.0, 0.01);
    check_close("sun escape velocity",
                orbits::escape_velocity(M_sun, R_sun), 617700.0, 0.01);

    // --- Kepler's third law: Earth's year from a = 1 AU about the Sun ----------
    check_close("earth orbital period",
                orbits::orbital_period(M_sun, AU), year, 0.02);

    // --- Internal consistency: for a circular orbit, v = 2*pi*a / T ------------
    {
        const double a = 3.0 * AU;
        const double v = orbits::circular_velocity(M_sun, a);
        const double T = orbits::orbital_period(M_sun, a);
        check_close("circular orbit v == 2 pi a / T",
                    v, 2.0 * constants::pi * a / T, 1.0e-9);
    }

    // --- Schwarzschild radius: Sun ~2.95 km, and r_s(Earth) ~8.87 mm -----------
    check_close("sun schwarzschild radius",
                orbits::schwarzschild_radius(M_sun), 2953.0, 0.01);
    check_close("earth schwarzschild radius",
                orbits::schwarzschild_radius(M_earth), 8.87e-3, 0.02);

    // --- A black hole has compactness ~1; escape velocity capped at c ---------
    {
        // Object whose radius equals its own Schwarzschild radius.
        const double M = 2.0e31;
        const double rs = orbits::schwarzschild_radius(M);
        const DerivedQuantities d = derive_quantities(M, rs);
        require(std::abs(d.compactness - 1.0) < 1.0e-6, "compactness at r = r_s must be 1");
        check_close("escape velocity at horizon is c", d.escape_velocity_ms, constants::c, 1.0e-9);
    }

    // --- Dynamical time depends only on density; denser -> faster collapse -----
    {
        const double t_water = orbits::dynamical_time(1000.0);
        require(std::isfinite(t_water) && t_water > 0.0, "dynamical time must be positive/finite");
        const double t_dense = orbits::dynamical_time(1.0e6);
        require(t_dense < t_water, "denser bodies must have a shorter dynamical time");
        // t_dyn scales as rho^-1/2: 1000x density -> ~31.6x shorter.
        check_close("dynamical time scales as rho^-1/2",
                    t_water / t_dense, std::sqrt(1000.0), 1.0e-6);
    }

    // --- Degenerate inputs are handled (no NaNs / divide-by-zero) --------------
    require(orbits::circular_velocity(0.0, 1.0) == 0.0, "zero mass -> zero velocity");
    require(orbits::escape_velocity(M_sun, 0.0) == 0.0, "zero radius -> zero escape velocity");
    require(orbits::orbital_period(M_sun, 0.0) == 0.0, "zero axis -> zero period");

    if (g_failures == 0) {
        std::cout << "cosmos_orbits_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_orbits_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
