// Verifies the CosmoStats astrophysical distribution module: the Kroupa IMF
// sampler is bottom-heavy and bounded, the mass-luminosity / lifetime relations
// are monotone and solar-anchored, the Chen-Kipping mass-radius law is
// continuous and monotone-ish, the Schechter GSMF and NFW profile have the
// expected shapes, the Dutton-Maccio concentration relation decreases with
// mass, the Planck 2018 constants are self-consistent, and the cosmology
// functions (P(k), xi, Sheth-Tormen, Schwarzschild, M-sigma) behave correctly.

#include "cosmos/CosmoStats.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;
using namespace cosmos::cstat;

namespace {
int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_cosmostats_verification FAILED: " << what << "\n"; ++g_failures; }
}
bool approx(double a, double b, double rel) {
    const double scale = std::max(1.0, std::max(std::abs(a), std::abs(b)));
    return std::abs(a - b) <= rel * scale;
}
} // namespace

int main() {
    // --- IMF sampling: bottom-heavy, bounded -----------------------------------
    {
        const double m_lo = 0.08, m_hi = 120.0;
        const int N = 200000;
        long below = 0;
        double sum = 0.0;
        bool in_range = true;
        // Deterministic uniform sweep of u01 in [0,1).
        for (int i = 0; i < N; ++i) {
            const double u = (i + 0.5) / N;
            const double m = sample_imf_mass(u, m_lo, m_hi);
            if (m < m_lo - 1e-9 || m > m_hi + 1e-9) in_range = false;
            if (m < 0.5) ++below;
            sum += m;
        }
        const double frac_below = static_cast<double>(below) / N;
        const double mean = sum / N;
        check(in_range, "all IMF samples within [m_lo, m_hi]");
        check(frac_below > 0.7, "majority of IMF mass below 0.5 Msun (bottom-heavy)");
        check(mean < 1.0, "mean IMF sampled mass is low (<1 Msun)");
    }

    // --- Mass-luminosity & lifetime --------------------------------------------
    {
        check(approx(stellar_luminosity(1.0), 1.0, 1e-9), "L(1 Msun) ~ 1 Lsun");
        check(stellar_luminosity(2.0) > stellar_luminosity(1.0), "L increasing in M (1->2)");
        check(stellar_luminosity(20.0) > stellar_luminosity(5.0), "L increasing in M (5->20)");
        check(stellar_luminosity(0.2) < stellar_luminosity(1.0), "L increasing in M (0.2->1)");

        check(stellar_lifetime_gyr(2.0) < stellar_lifetime_gyr(1.0), "lifetime decreasing in M");
        check(stellar_lifetime_gyr(10.0) < stellar_lifetime_gyr(2.0), "lifetime decreasing in M (hi)");
        check(approx(stellar_lifetime_gyr(1.0), 10.0, 0.30), "lifetime(1 Msun) ~ 10 Gyr (within 30%)");
        check(stellar_lifetime_gyr(20.0) < 1.0, "20 Msun lifetime < 1 Gyr");
        check(stellar_lifetime_gyr(0.5) > 10.0, "0.5 Msun lifetime > 10 Gyr");
    }

    // --- Planet mass-radius -----------------------------------------------------
    {
        check(approx(planet_radius_earth(1.0), 1.0, 1e-9), "R(1 Mearth) ~ 1 Rearth");
        // Monotone-ish increasing up to the Jovian break.
        bool inc = true;
        double prev = planet_radius_earth(0.5);
        for (double m = 1.0; m <= 132.0; m *= 1.5) {
            const double r = planet_radius_earth(m);
            if (r < prev - 1e-9) inc = false;
            check(std::isfinite(r), "planet radius finite");
            prev = r;
        }
        check(inc, "planet radius increasing up to ~132 Mearth");
        // Beyond ~132 Mearth, radius is ~flat (slightly shrinking) per Jovian slope.
        const double r132 = planet_radius_earth(132.0);
        const double r5000 = planet_radius_earth(5000.0);
        check(std::abs(r5000 - r132) < 0.5 * r132, "planet radius ~flat above ~132 Mearth");
        check(std::isfinite(r5000), "planet radius finite at large M");
    }

    // --- Schechter GSMF ---------------------------------------------------------
    {
        const double Mstar = 1e10, alpha = -1.45, phi_star = 1e-3;
        check(schechter_phi(0.5 * Mstar, Mstar, alpha, phi_star) > 0.0, "schechter positive below Mstar");
        check(schechter_phi(Mstar, Mstar, alpha, phi_star) > 0.0, "schechter positive at Mstar");
        check(schechter_phi(2.0 * Mstar, Mstar, alpha, phi_star) <
                  schechter_phi(Mstar, Mstar, alpha, phi_star),
              "schechter falls past Mstar (exp cutoff)");
    }

    // --- NFW profile slopes -----------------------------------------------------
    {
        const double rho_s = 1.0, r_s = 10.0;
        // Inner (r << r_s): rho ~ r^-1. Ratio of densities at r and 2r ~ 0.5.
        const double r1 = 0.01 * r_s, r2 = 2.0 * r1;
        const double inner_ratio = nfw_density(r2, rho_s, r_s) / nfw_density(r1, rho_s, r_s);
        check(approx(inner_ratio, 0.5, 0.05), "NFW inner slope ~ r^-1");
        // Outer (r >> r_s): rho ~ r^-3. Ratio at r and 2r ~ 0.125.
        const double R1 = 100.0 * r_s, R2 = 2.0 * R1;
        const double outer_ratio = nfw_density(R2, rho_s, r_s) / nfw_density(R1, rho_s, r_s);
        check(approx(outer_ratio, 0.125, 0.05), "NFW outer slope ~ r^-3");
        check(nfw_density(r_s, rho_s, r_s) > 0.0, "NFW density positive");
    }

    // --- Concentration-mass -----------------------------------------------------
    {
        check(approx(nfw_concentration(1.0), 8.0, 0.05), "concentration ~8 at M=1e12 h^-1 Msun");
        check(nfw_concentration(1000.0) < nfw_concentration(1.0), "concentration decreasing with mass");
    }

    // --- LCDM consistency -------------------------------------------------------
    {
        check(approx(kOmegaM + kOmegaLambda, 1.0, 0.005), "OmegaM + OmegaLambda ~ 1 (flat)");
        check(std::abs((kOmegaB + kOmegaC) - kOmegaM) < 0.01, "OmegaB + OmegaC ~ OmegaM");
    }

    // --- Power spectrum & correlation ------------------------------------------
    {
        const double k_turn = 0.018;
        const double left = power_spectrum_pk(k_turn * (1.0 - 1e-7), k_turn);
        const double right = power_spectrum_pk(k_turn * (1.0 + 1e-7), k_turn);
        check(approx(left, right, 1e-4), "P(k) continuous at k_turn");
        check(power_spectrum_pk(0.001, k_turn) > 0.0, "P(k) positive at small k");

        check(approx(correlation_xi(5.0), 1.0, 1e-9), "xi(5 h^-1 Mpc) ~ 1");
        check(correlation_xi(10.0) < correlation_xi(5.0), "xi decreasing with r");
    }

    // --- Sheth-Tormen multiplicity ---------------------------------------------
    {
        bool pos = true, fin = true;
        double peak_nu = 0.0, peak_val = -1.0;
        for (double nu = 0.05; nu <= 6.0; nu += 0.05) {
            const double f = sheth_tormen_f(nu);
            if (!(f > 0.0)) pos = false;
            if (!std::isfinite(f)) fin = false;
            if (f > peak_val) { peak_val = f; peak_nu = nu; }
        }
        check(pos, "Sheth-Tormen f(nu) positive");
        check(fin, "Sheth-Tormen f(nu) finite");
        check(peak_nu > 0.2 && peak_nu < 3.0, "Sheth-Tormen peaks at intermediate nu");
        check(sheth_tormen_f(6.0) < sheth_tormen_f(peak_nu), "Sheth-Tormen -> 0 at large nu");
        check(sheth_tormen_f(10.0) < 1e-3, "Sheth-Tormen strongly suppressed at very large nu");
    }

    // --- Black holes ------------------------------------------------------------
    {
        check(approx(schwarzschild_radius_km(1.0), 2.95, 1e-9), "Schwarzschild radius(1 Msun) ~ 2.95 km");
        check(approx(m_sigma_bh_mass(200.0), 3.1e8, 0.02), "M-sigma at 200 km/s ~ 3.1e8 Msun");
        check(m_sigma_bh_mass(300.0) > m_sigma_bh_mass(200.0), "M-sigma increasing with sigma");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_cosmostats_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_cosmostats_verification: " << g_failures << " check(s) failed\n";
    return EXIT_FAILURE;
}
