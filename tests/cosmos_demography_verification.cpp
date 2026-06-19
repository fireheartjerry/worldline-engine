// Verifies the structured-demography layer: the 3-stage Lefkovitch stable stage
// distribution is a valid probability vector, r-strategists are juvenile-heavy
// (Type III) while K-strategists are adult-heavy (Type I), and every output is
// finite and bounded across the r<->K axis.

#include "cosmos/Demography.hpp"
#include "cosmos/Organism.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;

namespace {
int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_demography_verification FAILED: " << what << "\n"; ++g_failures; }
}
demo::StageModel build(double rho, double m) {
    eco::SpeciesTraits t; t.rho = rho; t.m = m; t.tau = 1.0;
    return demo::stage_model(t, bio::derive_organism(t, 20.0));
}
} // namespace

int main() {
    // --- Stable distribution is a valid probability vector --------------------
    for (double rho = 0.0; rho <= 1.0; rho += 0.1) {
        const demo::StageModel s = build(rho, 0.0);
        const double sum = s.stable[0] + s.stable[1] + s.stable[2];
        check(std::abs(sum - 1.0) < 1e-6, "stable stage distribution sums to 1");
        check(s.stable[0] >= 0 && s.stable[1] >= 0 && s.stable[2] >= 0, "stage fractions nonnegative");
        check(std::isfinite(s.lambda) && s.lambda > 0.0, "dominant eigenvalue finite positive");
    }

    // --- r vs K survivorship structure ----------------------------------------
    {
        const demo::StageModel k = build(0.05, 1.0); // K: slow, few young
        const demo::StageModel r = build(0.95, 1.0); // r: fast, many young
        check(r.stable[0] > k.stable[0], "r-strategists are more juvenile-heavy (Type III)");
        check(k.stable[2] > r.stable[2], "K-strategists are more adult-heavy (Type I)");
        check(r.survivorship > k.survivorship, "survivorship index higher for r-strategists");
        check(k.survivorship >= 1.0 && r.survivorship <= 3.0, "survivorship in [1,3]");
    }

    // --- Power iteration stays bounded across the whole trait space ------------
    {
        bool ok = true;
        for (double rho = 0.0; rho <= 1.0 && ok; rho += 0.2)
            for (double m = -5.0; m <= 4.0 && ok; m += 1.0) {
                const demo::StageModel s = build(rho, m);
                ok = std::isfinite(s.stable[0]) && std::isfinite(s.stable[1]) &&
                     std::isfinite(s.stable[2]) && std::isfinite(s.lambda);
            }
        check(ok, "stage model finite across the trait space");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_demography_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_demography_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
