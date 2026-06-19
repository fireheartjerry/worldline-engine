// Verifies the astrophysics + ecology lookups used by procedural generation:
// the habitable-zone formula, the stellar IMF weighting, the Whittaker biome
// model, NPP ordering, and the ecology scaling constants.

#include "cosmos/Astrobio.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::astro;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_astrobio_verification FAILED: " << what << "\n"; ++g_failures; }
}

void check_close(const std::string& what, double got, double expected, double tol) {
    if (std::abs(got - expected) > tol) {
        std::cerr << "cosmos_astrobio_verification FAILED: " << what << " got " << got
                  << " expected " << expected << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- Habitable zone: Sun (L=1) conservative edges 0.95 .. 1.37 AU ---------
    {
        const HabitableZone hz = habitable_zone(1.0);
        check_close("sun HZ inner", hz.inner_au, 0.95, 1.0e-9);
        check_close("sun HZ outer", hz.outer_au, 1.37, 1.0e-9);
        check(in_habitable_zone(1.0, 1.0), "Earth (1 AU) is in the Sun's HZ");
        check(!in_habitable_zone(0.3, 1.0), "0.3 AU is too hot");
        check(!in_habitable_zone(3.0, 1.0), "3 AU is too cold");
        // A 100x luminosity star's HZ is ~10x farther (sqrt scaling).
        const HabitableZone bright = habitable_zone(100.0);
        check_close("HZ scales as sqrt(L)", bright.inner_au, 9.5, 1.0e-6);
    }

    // --- Stellar relations -----------------------------------------------------
    {
        check_close("L of 1 Msun", star_luminosity(1.0), 1.0, 1.0e-9);
        check_close("lifetime of 1 Msun", star_lifetime_gyr(1.0), 10.0, 1.0e-9);
        check(star_luminosity(10.0) > 1000.0, "10 Msun is very luminous");
        check(star_lifetime_gyr(10.0) < 0.1, "10 Msun dies fast");
        check(star_lifetime_gyr(0.25) > 100.0, "M dwarf lives a long time");

        // IMF weights sum to ~1 and M dwarfs dominate.
        double sum = 0.0;
        for (const StarInfo& s : star_table()) sum += s.weight;
        check_close("IMF weights sum to 1", sum, 1.0, 0.01);
        check(star_info(StarClass::M).weight > 0.6, "M dwarfs dominate the population");
        check(star_info(StarClass::O).weight < 1.0e-5, "O stars are vanishingly rare");
        // Weighted picker is M-dominant over a sweep of u in [0,1).
        int m = 0, n = 100000;
        for (int i = 0; i < n; ++i) {
            if (pick_star_class((i + 0.5) / n) == StarClass::M) ++m;
        }
        check(m > 0.6 * n, "picker is M-dominant");
        // Habitability ranking: K best, O/B excluded.
        check(star_info(StarClass::K).habitability >= star_info(StarClass::G).habitability,
              "K is at least as habitable as G");
        check(star_info(StarClass::O).habitability == 0.0, "O stars cannot host complex life");
    }

    // --- Whittaker biomes ------------------------------------------------------
    {
        check(biome_for(-20.0, 100.0) == Biome::IceSheet, "very cold + dry -> ice sheet");
        check(biome_for(-10.0, 400.0) == Biome::Tundra, "cold + moist -> tundra");
        check(biome_for(0.0, 500.0) == Biome::Taiga, "subpolar + moist -> taiga");
        check(biome_for(12.0, 1200.0) == Biome::TemperateForest, "temperate + wet -> forest");
        check(biome_for(12.0, 400.0) == Biome::TemperateGrass, "temperate + dryish -> grassland");
        check(biome_for(26.0, 100.0) == Biome::Desert, "hot + dry -> desert");
        check(biome_for(26.0, 600.0) == Biome::Savanna, "hot + seasonal -> savanna");
        check(biome_for(26.0, 3000.0) == Biome::Rainforest, "hot + very wet -> rainforest");

        // NPP ordering: rainforest >> temperate forest > grassland > desert.
        check(npp_for(Biome::Rainforest) > npp_for(Biome::TemperateForest), "rainforest most productive");
        check(npp_for(Biome::TemperateForest) > npp_for(Biome::TemperateGrass), "forest > grassland");
        check(npp_for(Biome::TemperateGrass) > npp_for(Biome::Desert), "grassland > desert");
        check(npp_for(Biome::Desert) < 150.0 && npp_for(Biome::Tundra) < 200.0, "desert/tundra are low");

        // Richness rises with productivity and is bounded in [0,1].
        check(richness_factor(2200.0) > richness_factor(90.0), "richer biomes hold more species");
        check(richness_factor(90.0) >= 0.0 && richness_factor(5000.0) <= 1.0, "richness bounded");
    }

    // --- Ecology constants -----------------------------------------------------
    {
        check(std::abs(kTrophicEfficiency - 0.10) < 1.0e-9, "10% trophic efficiency");
        check(std::abs(kKleiberExp - 0.75) < 1.0e-9, "Kleiber exponent 0.75");
        check(std::abs(kDamuthExp + 0.75) < 1.0e-9, "Damuth exponent -0.75");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_astrobio_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_astrobio_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
