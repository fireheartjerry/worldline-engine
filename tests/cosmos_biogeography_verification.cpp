// Verifies the planetary biogeography module: deterministic Fibonacci-lattice
// region placement, the bounded region count, area-fraction partition of unity,
// pole-to-pole latitude span, the latitudinal temperature gradient, the
// island-biogeography species-area law, the great-circle distance + dispersal
// kernel, valid Whittaker biome indices with cross-region variation, and that
// every produced value is finite.

#include "cosmos/Astrobio.hpp"
#include "cosmos/Biogeography.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace cosmos::biogeo;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_biogeography_verification FAILED: " << what << "\n"; ++g_failures; }
}

bool finite_region(const Region& r) {
    return std::isfinite(r.lat_deg) && std::isfinite(r.lon_deg) &&
           std::isfinite(r.elevation) && std::isfinite(r.temp_c) &&
           std::isfinite(r.precip_mm) && std::isfinite(r.continentality) &&
           std::isfinite(r.area_frac) && std::isfinite(r.pool_richness);
}

} // namespace

int main() {
    const std::uint64_t seed = 0xC0FFEEull;
    const double planet_temp = 22.0;   // temperate planet
    const double planet_precip = 1100.0;
    const double tilt = 23.44;
    const double planet_npp = 1200.0;  // temperate-forest-ish productivity

    const RegionField f = build_region_field(seed, planet_temp, planet_precip, tilt, planet_npp);

    // --- Region count bounded [6, 24] -----------------------------------------
    check(f.n >= 6 && f.n <= kMaxRegions, "region count within [6,24]");
    check(f.n <= 24, "region count never exceeds array capacity");

    // --- Determinism: same inputs -> identical field --------------------------
    const RegionField g = build_region_field(seed, planet_temp, planet_precip, tilt, planet_npp);
    bool identical = (f.n == g.n);
    for (int i = 0; identical && i < f.n; ++i) {
        const Region& a = f.region[static_cast<std::size_t>(i)];
        const Region& b = g.region[static_cast<std::size_t>(i)];
        identical = (a.lat_deg == b.lat_deg) && (a.lon_deg == b.lon_deg) &&
                    (a.temp_c == b.temp_c) && (a.biome == b.biome) &&
                    (a.precip_mm == b.precip_mm);
    }
    check(identical, "deterministic: identical field for identical inputs");

    // Different seed should generally produce a different field (elevation noise).
    const RegionField h = build_region_field(seed ^ 0x12345ull, planet_temp,
                                              planet_precip, tilt, planet_npp);
    bool any_diff = false;
    for (int i = 0; i < f.n && i < h.n; ++i) {
        if (f.region[static_cast<std::size_t>(i)].elevation !=
            h.region[static_cast<std::size_t>(i)].elevation) { any_diff = true; break; }
    }
    check(any_diff, "different seed yields a different elevation field");

    // --- area_frac sums to ~1 and is uniform ----------------------------------
    double area_sum = 0.0;
    for (int i = 0; i < f.n; ++i) area_sum += f.region[static_cast<std::size_t>(i)].area_frac;
    check(std::fabs(area_sum - 1.0) < 1e-6, "area_frac sums to ~1");

    // --- Latitude span pole-to-pole; longitude within [-180,180] --------------
    double min_lat = 1e9, max_lat = -1e9;
    bool lon_ok = true;
    for (int i = 0; i < f.n; ++i) {
        const Region& r = f.region[static_cast<std::size_t>(i)];
        if (r.lat_deg < min_lat) min_lat = r.lat_deg;
        if (r.lat_deg > max_lat) max_lat = r.lat_deg;
        if (r.lon_deg < -180.0 || r.lon_deg > 180.0) lon_ok = false;
        check(r.lat_deg >= -90.0 && r.lat_deg <= 90.0, "latitude within [-90,90]");
    }
    check(min_lat < -45.0, "Fibonacci lattice reaches southern high latitudes");
    check(max_lat > 45.0, "Fibonacci lattice reaches northern high latitudes");
    check(lon_ok, "longitudes consistently within [-180,180]");

    // --- Temperature falls with |latitude| ------------------------------------
    double hi_sum = 0.0, eq_sum = 0.0;
    int hi_n = 0, eq_n = 0;
    for (int i = 0; i < f.n; ++i) {
        const Region& r = f.region[static_cast<std::size_t>(i)];
        if (std::fabs(r.lat_deg) > 55.0) { hi_sum += r.temp_c; ++hi_n; }
        if (std::fabs(r.lat_deg) < 25.0) { eq_sum += r.temp_c; ++eq_n; }
    }
    check(hi_n > 0 && eq_n > 0, "have both high-latitude and near-equator regions");
    if (hi_n > 0 && eq_n > 0) {
        check((hi_sum / hi_n) < (eq_sum / eq_n),
              "mean temp of high-|lat| regions < near-equator regions");
    }

    // --- species_area: monotone in area; doubling -> 2^0.25 (~1.19) -----------
    const double pool = 500.0;
    const double s_small = species_area_richness(pool, 0.1);
    const double s_big = species_area_richness(pool, 0.4);
    check(s_big > s_small, "species_area increases with area_frac");
    const double s_a = species_area_richness(pool, 0.2);
    const double s_2a = species_area_richness(pool, 0.4);
    check(std::fabs(s_2a / s_a - std::pow(2.0, 0.25)) < 1e-9,
          "z=0.25 -> doubling area multiplies richness by 2^0.25");
    check(std::fabs(species_area_richness(pool, 0.0)) < 1e-12,
          "zero area -> zero richness");

    // --- great_circle_deg: self=0, antipode~180, symmetric --------------------
    Region p{}; p.lat_deg = 12.0; p.lon_deg = 40.0;
    Region q{}; q.lat_deg = -12.0; q.lon_deg = 40.0 + 180.0; // antipode of p
    q.lon_deg = -140.0; // = wrap(220) ; antipode longitude
    check(great_circle_deg(p, p) < 1e-9, "great_circle self-distance is 0");
    const double anti = great_circle_deg(p, q);
    check(std::fabs(anti - 180.0) < 1e-6, "great_circle antipode ~180 deg");
    check(std::fabs(great_circle_deg(p, q) - great_circle_deg(q, p)) < 1e-9,
          "great_circle is symmetric");
    // A non-trivial pair between two real regions is in [0,180].
    if (f.n >= 2) {
        const double d01 = great_circle_deg(f.region[0], f.region[1]);
        check(std::isfinite(d01) && d01 >= 0.0 && d01 <= 180.0 + 1e-9,
              "great_circle between regions within [0,180]");
    }

    // --- dispersal_weight in (0,1], decreasing with distance ------------------
    const double w0 = dispersal_weight(0.0, 30.0);
    const double w30 = dispersal_weight(30.0, 30.0);
    const double w90 = dispersal_weight(90.0, 30.0);
    check(std::fabs(w0 - 1.0) < 1e-12, "dispersal_weight at distance 0 is 1");
    check(w30 > 0.0 && w30 < 1.0, "dispersal_weight in (0,1) at positive distance");
    check(w90 < w30 && w30 < w0, "dispersal_weight decreases with distance");

    // --- biome indices valid; some variation on a temperate planet -----------
    std::set<int> biomes;
    bool biome_valid = true;
    for (int i = 0; i < f.n; ++i) {
        const int bi = f.region[static_cast<std::size_t>(i)].biome;
        if (bi < 0 || bi >= static_cast<int>(cosmos::astro::Biome::COUNT)) biome_valid = false;
        biomes.insert(bi);
    }
    check(biome_valid, "all biome indices are valid astro::Biome values");
    check(biomes.size() >= 2, "temperate planet shows biome variation across regions");

    // --- All produced values finite -------------------------------------------
    bool all_finite = true;
    for (int i = 0; i < f.n; ++i) {
        if (!finite_region(f.region[static_cast<std::size_t>(i)])) all_finite = false;
    }
    check(all_finite, "all region values are finite");

    // --- Region count responds to productivity bounds -------------------------
    const RegionField lo = build_region_field(seed, planet_temp, planet_precip, tilt, 1.0);
    const RegionField hhi = build_region_field(seed, planet_temp, planet_precip, tilt, 5000.0);
    check(lo.n == 6, "minimal productivity -> minimum region count (6)");
    check(hhi.n == kMaxRegions, "maximal productivity -> maximum region count (24)");

    if (g_failures == 0) {
        std::cout << "cosmos_biogeography_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_biogeography_verification: " << g_failures << " check(s) failed\n";
    return 1;
}
