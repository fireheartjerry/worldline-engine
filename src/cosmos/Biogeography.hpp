#pragma once
// Planetary biogeography — a deterministic, analytic model of a planet surface as
// a small set of bounded biome REGIONS rather than a stored raster. Region centers
// are placed on a Fibonacci lattice on the sphere; each region's abiotic state
// (elevation, temperature, precipitation, continentality) is a closed-form +
// hashed-noise function of (seed, region index, latitude, axial tilt). Biomes come
// from the shared Whittaker model (cosmos::astro), and the regions are coupled by
// island-biogeography theory: a species-area law for the local pool and a
// great-circle dispersal kernel between regions. Everything is a pure,
// header-only, deterministic function of its inputs — no RNG state, no globals.

#include "cosmos/Astrobio.hpp"
#include "cosmos/Phenology.hpp"
#include "cosmos/Spectrum.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cosmos {
namespace biogeo {

// Hard cap on the number of regions a planet is tessellated into.
constexpr int kMaxRegions = 24;

// Golden ratio and the golden angle used by the spherical Fibonacci lattice.
constexpr double kPhi = 1.61803398874989484820;            // (1 + sqrt(5)) / 2
constexpr double kGoldenAngle = kTwoPi * (1.0 - 1.0 / kPhi); // ~2.39996 rad

// One bounded biome region: a center on the sphere plus its analytic abiotic state.
struct Region {
    double lat_deg;         // center latitude  [-90, 90]
    double lon_deg;         // center longitude (normalized to [-180, 180])
    double elevation;       // signed terrain height proxy, ~[-1, 1] (negative = ocean)
    double temp_c;          // annual-mean surface temperature (deg C)
    double precip_mm;       // annual precipitation (mm)
    double continentality;  // [0,1]: 0 = maritime, 1 = deep continental interior
    double area_frac;       // fraction of the planet surface this region covers
    int biome;              // astro::Biome cast to int
    double pool_richness;   // species-area pool weight, pow(area_frac, z)
};

// A planet surface as up to kMaxRegions analytic regions. The leading n entries
// are valid; trailing entries are unused.
struct RegionField {
    int n;
    std::array<Region, 24> region;
};

// ── Internal helpers ─────────────────────────────────────────────────────────
namespace detail {

inline double deg2rad(double d) { return d * (kTwoPi / 360.0); }
inline double rad2deg(double r) { return r * (360.0 / kTwoPi); }

// Normalize a longitude in degrees to [-180, 180].
inline double wrap_lon_deg(double lon_deg) {
    double x = std::fmod(lon_deg + 180.0, 360.0);
    if (x < 0.0) x += 360.0;
    return x - 180.0;
}

// Smooth, deterministic hashed gradient in [-1,1] for a 1-D integer coordinate
// at a given frequency cell, interpolated with smoothstep (value noise).
inline double hashed_unit(std::uint64_t seed, std::uint64_t channel, std::int64_t cell) {
    const std::uint64_t h = mix2(seed ^ (channel * 0x9E3779B97F4A7C15ull),
                                 static_cast<std::uint64_t>(cell) * 0x85EBCA6Bull + 1ull);
    return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0); // [0,1)
}

// 1-D value noise over a continuous coordinate `t` (interpreted in cells of
// `frequency` per unit) summed over `octaves`, returned in [-1,1].
inline double value_noise(std::uint64_t seed, std::uint64_t channel, double t,
                          double frequency, int octaves) {
    double sum = 0.0;
    double amp = 1.0;
    double norm = 0.0;
    double freq = frequency;
    for (int o = 0; o < octaves; ++o) {
        const double x = t * freq;
        const double xf = std::floor(x);
        const std::int64_t i0 = static_cast<std::int64_t>(xf);
        const double frac = x - xf;
        const std::uint64_t ch = channel * 131ull + static_cast<std::uint64_t>(o);
        const double a = hashed_unit(seed, ch, i0);
        const double b = hashed_unit(seed, ch, i0 + 1);
        const double s = smoothstep(0.0, 1.0, frac);
        const double v = lerp(a, b, s);     // [0,1)
        sum += amp * (v * 2.0 - 1.0);       // [-1,1)
        norm += amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    return (norm > 0.0) ? sum / norm : 0.0;
}

} // namespace detail

// ── Island-biogeography helpers ──────────────────────────────────────────────

// Species-area relationship S = pool_S * area_frac^z (Arrhenius form; z~0.25).
inline double species_area_richness(double pool_S, double area_frac, double z = 0.25) {
    const double a = (area_frac > 0.0) ? area_frac : 0.0;
    return pool_S * std::pow(a, z);
}

// Great-circle (central) angular separation between two region centers, in
// degrees, via the haversine formula. Self -> 0; antipode -> 180; symmetric.
inline double great_circle_deg(const Region& a, const Region& b) {
    const double la1 = detail::deg2rad(a.lat_deg);
    const double la2 = detail::deg2rad(b.lat_deg);
    const double dla = la2 - la1;
    const double dlo = detail::deg2rad(b.lon_deg - a.lon_deg);
    const double sdla = std::sin(dla * 0.5);
    const double sdlo = std::sin(dlo * 0.5);
    double h = sdla * sdla + std::cos(la1) * std::cos(la2) * sdlo * sdlo;
    h = clampd(h, 0.0, 1.0);
    const double c = 2.0 * std::asin(std::sqrt(h)); // central angle in radians
    return detail::rad2deg(c);
}

// Dispersal kernel: exp(-dist/lambda), in (0,1], decreasing with distance.
// Callers may further attenuate ocean crossings by scaling the result.
inline double dispersal_weight(double dist_deg, double lambda_deg) {
    const double lam = (lambda_deg > 1.0e-9) ? lambda_deg : 1.0e-9;
    const double d = (dist_deg > 0.0) ? dist_deg : 0.0;
    return std::exp(-d / lam);
}

// ── Region-field construction ────────────────────────────────────────────────

// Build the analytic region field for a planet. Deterministic in all arguments.
inline RegionField build_region_field(std::uint64_t seed, double planet_temp_c,
                                       double planet_precip_mm, double axial_tilt_deg,
                                       double planet_npp) {
    RegionField f;

    // Region count from productivity-driven richness, clamped to [6, kMaxRegions].
    const double rf = astro::richness_factor(planet_npp);
    int n = static_cast<int>(std::lround(6.0 + 18.0 * rf));
    if (n < 6) n = 6;
    if (n > kMaxRegions) n = kMaxRegions;
    f.n = n;

    // Sampled global sea-level threshold in elevation space (-1..1), hashed from
    // the seed so different worlds have different land fractions but a given seed
    // is fixed.
    Stream sea_stream(seed, 0x5EA1E2E12ull);
    const double sea_level = sea_stream.range(-0.35, 0.35);

    // Insolation at the pole, used to remap the latitudinal thermal gradient so
    // the equator stays warm and the poles fall ~30-40 C colder.
    const double ins_pole = pheno::annual_mean_insolation(90.0);
    const double ins_eq = pheno::annual_mean_insolation(0.0);
    const double tilt_widen = clampd(axial_tilt_deg / 23.44, 0.0, 2.0);

    for (int i = 0; i < n; ++i) {
        Region r;

        // Spherical Fibonacci lattice: even area, deterministic, pole-to-pole.
        const double y = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(n);
        const double yc = clampd(y, -1.0, 1.0);
        const double lat = std::asin(yc);                 // radians
        const double lon = static_cast<double>(i) * kGoldenAngle;
        r.lat_deg = detail::rad2deg(lat);
        r.lon_deg = detail::wrap_lon_deg(detail::rad2deg(lon));
        r.area_frac = 1.0 / static_cast<double>(n);

        const double abslat = std::fabs(r.lat_deg);

        // Elevation: 3 octaves of value noise over the region index, in [-1,1].
        r.elevation = detail::value_noise(seed, 0x1001ull, static_cast<double>(i) + 0.5,
                                          0.7, 3);

        // Continentality: a second, lower-frequency noise field remapped to [0,1].
        const double cnoise = detail::value_noise(seed, 0x2002ull,
                                                  static_cast<double>(i) + 0.5, 0.35, 2);
        r.continentality = clampd(cnoise * 0.5 + 0.5, 0.0, 1.0);

        const bool is_ocean = r.elevation < sea_level;

        // Temperature: equatorial base scaled down toward the poles by the
        // annual-mean insolation gradient, minus an elevation lapse (~6.5 C/km
        // proxy from positive elevation), plus a small continental range widening.
        const double ins = pheno::annual_mean_insolation(r.lat_deg);
        const double grad = clampd((ins - ins_pole) / (ins_eq - ins_pole), 0.0, 1.0);
        const double lat_temp = lerp(planet_temp_c - 38.0, planet_temp_c, grad);
        const double land_height = (r.elevation > sea_level) ? (r.elevation - sea_level) : 0.0;
        const double lapse = 6.5 * land_height * 4.0; // elevation proxy (km-ish) * lapse rate
        const double cont_widen = (r.continentality - 0.5) * 4.0 * tilt_widen;
        r.temp_c = lat_temp - lapse + cont_widen;

        // Precipitation: Hadley/Ferrel latitudinal band model. ITCZ maximum at the
        // equator, subtropical-desert minimum near 30 deg, secondary maximum near
        // 60 deg, declining toward the poles. Scaled by planet_precip_mm, reduced
        // by continentality (drier inland), boosted orographically by elevation.
        const double phi = detail::deg2rad(abslat);
        // Two cosine lobes: a broad equatorial/ITCZ peak and a mid-latitude peak.
        const double itcz = std::exp(-(abslat * abslat) / (2.0 * 15.0 * 15.0));      // peak at 0
        const double subtrop_dry = std::exp(-((abslat - 30.0) * (abslat - 30.0)) / (2.0 * 12.0 * 12.0));
        const double ferrel = std::exp(-((abslat - 60.0) * (abslat - 60.0)) / (2.0 * 14.0 * 14.0));
        double band = 0.25 + 0.85 * itcz - 0.45 * subtrop_dry + 0.50 * ferrel;
        band = (band > 0.05) ? band : 0.05;
        (void)phi;
        const double cont_reducer = lerp(1.0, 0.45, r.continentality); // drier inland
        const double orographic = 1.0 + 0.35 * land_height;            // wetter highlands
        r.precip_mm = planet_precip_mm * band * cont_reducer * orographic;
        if (r.precip_mm < 0.0) r.precip_mm = 0.0;

        // Biome from the shared Whittaker model; ocean cells are explicitly Ocean.
        const astro::Biome b = is_ocean
                                   ? astro::Biome::Ocean
                                   : astro::biome_for(r.temp_c, r.precip_mm);
        r.biome = static_cast<int>(b);

        // Island-biogeography species-area pool weight (z = 0.25).
        r.pool_richness = std::pow(r.area_frac, 0.25);

        f.region[static_cast<std::size_t>(i)] = r;
    }

    return f;
}

} // namespace biogeo
} // namespace cosmos
