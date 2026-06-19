// Verifies the deterministic climate / seasonality module: the latitude-driven
// insolation gradient, seasonal-amplitude scaling with latitude and
// continentality, the bounded annual temperature cycle, the phenological
// responses (breeding pulse, metabolic dormancy, NPP modulation), the
// declination bound, and purity/determinism.

#include "cosmos/Phenology.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::pheno;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_phenology_verification FAILED: " << what << "\n"; ++g_failures; }
}

bool finite_pos(double x) { return std::isfinite(x) && x > 0.0; }

} // namespace

int main() {
    // --- annual_mean_insolation: equator > mid > pole, all finite & positive ---
    const double ins_eq = annual_mean_insolation(0.0);
    const double ins_mid = annual_mean_insolation(45.0);
    const double ins_pole = annual_mean_insolation(90.0);
    check(finite_pos(ins_eq) && finite_pos(ins_mid) && finite_pos(ins_pole),
          "annual_mean_insolation finite and positive");
    check(ins_eq > ins_mid && ins_mid > ins_pole,
          "annual_mean_insolation decreases equator -> mid -> pole");

    // --- seasonal_amp_c: higher at high lat & high continentality -------------
    const Climate trop_maritime = region_climate(25.0, 5.0, 23.44, 0.05);
    const Climate polar_cont = region_climate(25.0, 75.0, 23.44, 0.95);
    check(polar_cont.seasonal_amp_c > trop_maritime.seasonal_amp_c,
          "seasonal amplitude greater for high-latitude continental region");

    // Isolate each axis.
    const Climate lo_lat = region_climate(25.0, 5.0, 23.44, 0.5);
    const Climate hi_lat = region_climate(25.0, 75.0, 23.44, 0.5);
    check(hi_lat.seasonal_amp_c > lo_lat.seasonal_amp_c,
          "seasonal amplitude grows with latitude");
    const Climate lo_cont = region_climate(25.0, 45.0, 23.44, 0.1);
    const Climate hi_cont = region_climate(25.0, 45.0, 23.44, 0.9);
    check(hi_cont.seasonal_amp_c > lo_cont.seasonal_amp_c,
          "seasonal amplitude grows with continentality");

    // --- seasonal_temp oscillates within [mean-amp, mean+amp] -----------------
    {
        const Climate c = region_climate(15.0, 50.0, 23.44, 0.7);
        const double lo = c.mean_temp_c - c.seasonal_amp_c;
        const double hi = c.mean_temp_c + c.seasonal_amp_c;
        bool ok = true;
        for (int i = 0; i < 100; ++i) {
            const double t = static_cast<double>(i) / 100.0;
            const double T = seasonal_temp(c, t);
            if (!(T >= lo - 1.0e-9 && T <= hi + 1.0e-9)) ok = false;
        }
        check(ok, "seasonal_temp stays within [mean-amp, mean+amp] over a year");
    }

    // --- breeding_pulse: peaks at T_opt, decays, in [0,1] ---------------------
    {
        const double T_opt = 20.0, width = 4.0;
        const double at_opt = breeding_pulse(T_opt, T_opt, width);
        const double off_near = breeding_pulse(T_opt, T_opt + 5.0, width);
        const double off_far = breeding_pulse(T_opt, T_opt + 20.0, width);
        check(std::fabs(at_opt - 1.0) < 1.0e-12, "breeding_pulse peaks at ~1 when temp == T_opt");
        check(at_opt > off_near && off_near > off_far, "breeding_pulse decays away from optimum");
        bool bounded = true;
        for (int i = -40; i <= 60; ++i) {
            const double v = breeding_pulse(T_opt, static_cast<double>(i), width);
            if (!(v >= 0.0 && v <= 1.0)) bounded = false;
        }
        check(bounded, "breeding_pulse stays in [0,1]");
    }

    // --- dormancy_factor: ectotherm cold << warm; endotherm ~1; all in (0,1] --
    {
        const double T_opt = 25.0;
        const double ecto_cold = dormancy_factor(0.0, 0.0, T_opt);
        const double ecto_warm = dormancy_factor(0.0, 25.0, T_opt);
        check(ecto_cold < ecto_warm, "ectotherm much slower when cold than warm");
        check(ecto_cold < 0.5 * ecto_warm, "ectotherm cold activity well below warm activity");

        const double endo_cold = dormancy_factor(1.0, -20.0, T_opt);
        const double endo_warm = dormancy_factor(1.0, 25.0, T_opt);
        check(endo_cold > 0.9 && endo_warm > 0.9, "endotherm stays near 1 regardless of temperature");

        bool bounded = true;
        for (double endo = 0.0; endo <= 1.0; endo += 0.25) {
            for (int t = -40; t <= 50; t += 5) {
                const double v = dormancy_factor(endo, static_cast<double>(t), T_opt);
                if (!(v > 0.0 && v <= 1.0)) bounded = false;
            }
        }
        check(bounded, "dormancy_factor stays in (0,1]");
    }

    // --- npp_seasonal_factor: bounded for insolation in [0,1] -----------------
    {
        bool bounded = true;
        for (int i = 0; i <= 100; ++i) {
            const double v = npp_seasonal_factor(static_cast<double>(i) / 100.0);
            if (!(v >= 0.4 && v <= 1.3)) bounded = false;
        }
        // Also probe out-of-range inputs (must clamp, not explode).
        const double under = npp_seasonal_factor(-5.0);
        const double over = npp_seasonal_factor(5.0);
        check(bounded, "npp_seasonal_factor stays within [0.4,1.3] for insolation in [0,1]");
        check(under >= 0.4 && under <= 1.3 && over >= 0.4 && over <= 1.3,
              "npp_seasonal_factor clamps out-of-range insolation");
    }

    // --- declination magnitude <= axial tilt ----------------------------------
    {
        const double tilt = 23.44, year = 365.25;
        bool ok = true;
        for (int d = 0; d < 366; ++d) {
            const double delta = declination(tilt, static_cast<double>(d), year);
            if (std::fabs(delta) > tilt + 1.0e-9) ok = false;
        }
        check(ok, "declination magnitude never exceeds axial tilt");
    }

    // --- insolation_factor bounded in [0,1] -----------------------------------
    {
        bool ok = true;
        for (int lat = -90; lat <= 90; lat += 10) {
            for (int d = 0; d < 365; d += 30) {
                const double v = insolation_factor(static_cast<double>(lat), 23.44,
                                                   static_cast<double>(d), 365.25);
                if (!(v >= 0.0 && v <= 1.0) || !std::isfinite(v)) ok = false;
            }
        }
        check(ok, "insolation_factor stays in [0,1] and finite");
    }

    // --- Determinism / purity: same inputs -> same outputs --------------------
    {
        const Climate a = region_climate(18.0, 40.0, 23.44, 0.6);
        const Climate b = region_climate(18.0, 40.0, 23.44, 0.6);
        check(a.mean_temp_c == b.mean_temp_c && a.seasonal_amp_c == b.seasonal_amp_c &&
              a.phase == b.phase, "region_climate is deterministic");
        check(seasonal_temp(a, 0.37) == seasonal_temp(b, 0.37), "seasonal_temp is deterministic");
        check(annual_mean_insolation(33.0) == annual_mean_insolation(33.0),
              "annual_mean_insolation is deterministic");
        check(breeding_pulse(20.0, 17.0, 4.0) == breeding_pulse(20.0, 17.0, 4.0),
              "breeding_pulse is deterministic");
        check(dormancy_factor(0.3, 5.0, 25.0) == dormancy_factor(0.3, 5.0, 25.0),
              "dormancy_factor is deterministic");
        check(npp_seasonal_factor(0.42) == npp_seasonal_factor(0.42),
              "npp_seasonal_factor is deterministic");
        check(declination(23.44, 80.0, 365.25) == declination(23.44, 80.0, 365.25),
              "declination is deterministic");
        check(insolation_factor(45.0, 23.44, 120.0, 365.25) ==
              insolation_factor(45.0, 23.44, 120.0, 365.25),
              "insolation_factor is deterministic");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_phenology_verification: all checks passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "cosmos_phenology_verification: " << g_failures << " check(s) failed\n";
    return EXIT_FAILURE;
}
