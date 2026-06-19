// Verifies the organism functional-biology layer (Metabolic Theory of Ecology):
// the derived physiology obeys the canonical allometric scaling laws (BMR ∝
// M^0.75, lifespan ∝ M^0.25, von Bertalanffy growth ∝ M^-0.25, brain ∝ M^0.75),
// the ecto<->endotherm continuum is monotone in mass, and every output is finite
// and bounded across the whole trait/mass range.

#include "cosmos/Organism.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_organism_verification FAILED: " << what << "\n"; ++g_failures; }
}

// Log-log regression slope of y vs M across a mass sweep (fixed other traits).
double loglog_slope(double (*pick)(const bio::Organism&)) {
    eco::SpeciesTraits t;          // neutral traits
    t.kappa = 1.0; t.rho = 0.5; t.tau = 1.0; t.T_opt = 20.0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (double logm = -4.0; logm <= 3.0; logm += 0.25) {
        t.m = logm;
        const bio::Organism o = bio::derive_organism(t, 20.0);
        const double x = logm * std::log(10.0);          // ln M
        const double y = std::log(std::max(1e-300, pick(o)));
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++n;
    }
    return (n * sxy - sx * sy) / (n * sxx - sx * sx);
}

} // namespace

int main() {
    // --- Allometric scaling exponents -----------------------------------------
    const double s_bmr   = loglog_slope([](const bio::Organism& o) { return o.bmr_w; });
    const double s_life  = loglog_slope([](const bio::Organism& o) { return o.lifespan_yr; });
    const double s_growth= loglog_slope([](const bio::Organism& o) { return o.growth_k; });
    const double s_brain = loglog_slope([](const bio::Organism& o) { return o.brain_g; });
    const double s_home  = loglog_slope([](const bio::Organism& o) { return o.home_range_m2; });

    check(std::abs(s_bmr - 0.75) < 0.06, "BMR scales as M^0.75 (Kleiber)");
    check(std::abs(s_life - 0.25) < 0.06, "lifespan scales as M^0.25");
    check(std::abs(s_growth + 0.25) < 0.06, "von Bertalanffy growth scales as M^-0.25");
    check(std::abs(s_brain - 0.75) < 0.06, "brain mass scales as M^0.75 (Jerison)");
    check(std::abs(s_home - 1.0) < 0.08, "home range scales as M^1 (Damuth)");

    // --- Endothermy increases with body mass ----------------------------------
    {
        eco::SpeciesTraits small; small.m = -3.0; small.kappa = 1.0;
        eco::SpeciesTraits big;   big.m = 2.5;    big.kappa = 1.0;
        const bio::Organism os = bio::derive_organism(small, 20.0);
        const bio::Organism ob = bio::derive_organism(big, 20.0);
        check(ob.endothermy > os.endothermy, "endothermy rises with body mass");
        check(os.endothermy >= 0.0 && ob.endothermy <= 1.0, "endothermy in [0,1]");
    }

    // --- K-strategists outlive r-strategists of equal mass --------------------
    {
        eco::SpeciesTraits k; k.m = 1.0; k.rho = 0.1; // slow / K
        eco::SpeciesTraits r; r.m = 1.0; r.rho = 0.9; // fast / r
        check(bio::derive_organism(k, 20.0).lifespan_yr >
                  bio::derive_organism(r, 20.0).lifespan_yr,
              "K-strategists outlive r-strategists at equal mass");
    }

    // --- Everything finite and bounded across the full trait space ------------
    {
        bool ok = true;
        for (double m = -6.0; m <= 5.0 && ok; m += 0.5)
            for (double rho = 0.0; rho <= 1.0 && ok; rho += 0.25)
                for (double kap = 0.4; kap <= 2.0 && ok; kap += 0.4)
                    for (double T = -20.0; T <= 45.0 && ok; T += 15.0) {
                        eco::SpeciesTraits t; t.m = m; t.rho = rho; t.kappa = kap; t.T_opt = T;
                        const bio::Organism o = bio::derive_organism(t, T);
                        ok = std::isfinite(o.bmr_w) && o.bmr_w > 0.0 &&
                             std::isfinite(o.lifespan_yr) && o.lifespan_yr > 0.0 &&
                             std::isfinite(o.speed_ms) && o.speed_ms > 0.0 &&
                             std::isfinite(o.home_range_m2) && o.home_range_m2 > 0.0 &&
                             o.speed_ms <= 130.0;
                    }
        check(ok, "all organism outputs finite, positive, and bounded");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_organism_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_organism_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
