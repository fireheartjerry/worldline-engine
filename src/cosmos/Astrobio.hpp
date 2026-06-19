#pragma once
// Astrophysics + ecology lookups for grounded procedural generation.
// All values come from the deep-research appendix (stellar IMF/HR relations,
// Kopparapu habitable zone, Whittaker biomes + NPP, Lindeman/Damuth/Kleiber
// ecology). Pure and header-only so the generator and tests share one source.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cosmos {
namespace astro {

// ── Stars (main-sequence, Morgan-Keenan) ────────────────────────────────────
enum class StarClass { O, B, A, F, G, K, M, COUNT };

struct StarInfo {
    const char* name;
    double temp_k;        // representative effective temperature
    double mass_sun;      // representative mass
    double lifetime_gyr;  // main-sequence lifetime
    double weight;        // fraction of stars (IMF + lifetimes); M dominates
    double hue;           // display hue (blackbody-ish), low saturation
    double habitability;  // suitability for complex life [0,1]
};

inline const std::array<StarInfo, 7>& star_table() {
    // weight column sums to ~1.0 (canonical solar-neighborhood fractions).
    static const std::array<StarInfo, 7> t = {{
        {"O", 40000.0, 30.0,  0.005, 0.0000003, 0.62, 0.00},
        {"B", 20000.0,  8.0,  0.10,  0.0013,    0.60, 0.00},
        {"A",  8500.0,  1.8,  1.50,  0.006,     0.58, 0.05},
        {"F",  6700.0,  1.2,  4.00,  0.030,     0.16, 0.50},
        {"G",  5700.0,  0.95, 10.0,  0.076,     0.13, 0.90},
        {"K",  4400.0,  0.65, 30.0,  0.121,     0.07, 1.00},
        {"M",  3200.0,  0.25, 100.0, 0.76457,   0.02, 0.20},
    }};
    return t;
}

inline const StarInfo& star_info(StarClass c) { return star_table()[static_cast<std::size_t>(c)]; }

// Weighted pick by the IMF/frequency column (u01 in [0,1)).
inline StarClass pick_star_class(double u01) {
    double acc = 0.0;
    const auto& t = star_table();
    for (std::size_t i = 0; i < t.size(); ++i) {
        acc += t[i].weight;
        if (u01 < acc) return static_cast<StarClass>(i);
    }
    return StarClass::M;
}

inline double star_luminosity(double mass_sun) { return std::pow(mass_sun, 3.5); }   // L ∝ M^3.5
inline double star_lifetime_gyr(double mass_sun) { return 10.0 * std::pow(mass_sun, -2.5); }

// ── Habitable zone (conservative Kopparapu: 0.95–1.37·sqrt(L) AU) ────────────
struct HabitableZone { double inner_au; double outer_au; };
inline HabitableZone habitable_zone(double lum_sun) {
    const double s = std::sqrt(std::max(1.0e-6, lum_sun));
    return {0.95 * s, 1.37 * s};
}
inline bool in_habitable_zone(double orbit_au, double lum_sun) {
    const HabitableZone hz = habitable_zone(lum_sun);
    return orbit_au >= hz.inner_au && orbit_au <= hz.outer_au;
}

// ── Biomes (Whittaker temperature × precipitation) ──────────────────────────
enum class Biome {
    Ocean, IceSheet, Tundra, Taiga, TemperateGrass, TemperateForest,
    Desert, Savanna, TropicalForest, Rainforest, Barren, COUNT
};

inline const char* biome_name(Biome b) {
    switch (b) {
    case Biome::Ocean:           return "Ocean";
    case Biome::IceSheet:        return "Ice sheet";
    case Biome::Tundra:          return "Tundra";
    case Biome::Taiga:           return "Boreal forest";
    case Biome::TemperateGrass:  return "Temperate grassland";
    case Biome::TemperateForest: return "Temperate forest";
    case Biome::Desert:          return "Desert";
    case Biome::Savanna:         return "Savanna";
    case Biome::TropicalForest:  return "Tropical forest";
    case Biome::Rainforest:      return "Tropical rainforest";
    default:                     return "Barren";
    }
}

// Whittaker climate model: temperature gates the cold↔hot band, precipitation
// gates the desert↔forest gradient within each band.
inline Biome biome_for(double temp_c, double precip_mm) {
    if (temp_c < -5.0)  return (precip_mm < 150.0) ? Biome::IceSheet : Biome::Tundra;
    if (temp_c < 5.0)   return (precip_mm < 300.0) ? Biome::Tundra : Biome::Taiga;
    if (temp_c < 20.0) {
        if (precip_mm < 250.0)  return Biome::Desert;
        if (precip_mm < 750.0)  return Biome::TemperateGrass;
        return Biome::TemperateForest;
    }
    if (precip_mm < 250.0)   return Biome::Desert;
    if (precip_mm < 1000.0)  return Biome::Savanna;
    if (precip_mm < 2000.0)  return Biome::TropicalForest;
    return Biome::Rainforest;
}

// Net primary productivity (g dry matter / m²/yr), Whittaker & Likens 1973.
inline double npp_for(Biome b) {
    switch (b) {
    case Biome::Rainforest:      return 2200.0;
    case Biome::TropicalForest:  return 1600.0;
    case Biome::TemperateForest: return 1200.0;
    case Biome::Savanna:         return 900.0;
    case Biome::Taiga:           return 800.0;
    case Biome::TemperateGrass:  return 600.0;
    case Biome::Ocean:           return 125.0;
    case Biome::Tundra:          return 140.0;
    case Biome::Desert:          return 90.0;
    case Biome::IceSheet:        return 2.0;
    default:                     return 0.0;
    }
}

inline double biome_hue(Biome b) {
    switch (b) {
    case Biome::Ocean:           return 0.56;
    case Biome::IceSheet:        return 0.55;
    case Biome::Tundra:          return 0.45;
    case Biome::Taiga:           return 0.38;
    case Biome::TemperateGrass:  return 0.22;
    case Biome::TemperateForest: return 0.33;
    case Biome::Desert:          return 0.11;
    case Biome::Savanna:         return 0.16;
    case Biome::TropicalForest:  return 0.30;
    case Biome::Rainforest:      return 0.34;
    default:                     return 0.0;
    }
}

// Species richness scales with productivity/energy (species-energy hypothesis),
// normalized to ~[0,1] against rainforest NPP.
inline double richness_factor(double npp) {
    return std::clamp(npp / 2200.0, 0.02, 1.0);
}

// ── Ecology constants (Lindeman / Damuth / Kleiber) ─────────────────────────
constexpr double kTrophicEfficiency = 0.10;     // ~10% energy per level (5–20%)
constexpr double kKleiberExp        = 0.75;      // metabolism ∝ mass^0.75
constexpr double kDamuthExp         = -0.75;     // abundance ∝ mass^-0.75
constexpr double kHutchinsonMass    = 2.0;       // ≥2× mass between adjacent competitors

} // namespace astro
} // namespace cosmos
