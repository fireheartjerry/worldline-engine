#include "cosmos/ObjectCatalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cosmos {
namespace {

std::uint64_t hash_id(const std::string& id) {
    std::uint64_t h = 1469598103934665603ull;
    for (char ch : id) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
        h *= 1099511628211ull;
    }
    return h;
}

// A reproducible [0,1) fraction for object `id`, salted by `stream` so an object
// can draw several independent-looking values.
double frac(const std::string& id, std::uint64_t stream) {
    std::uint64_t h = hash_id(id) ^ (stream * 0x9E3779B97F4A7C15ull);
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    return static_cast<double>(h % 1000000ull) / 1000000.0;
}

double saturate(double x) { return std::clamp(x, 0.0, 1.0); }

double map_range(double x, double a, double b, double lo, double hi) {
    if (b <= a) {
        return 0.5 * (lo + hi);
    }
    return lo + (x - a) / (b - a) * (hi - lo);
}

// Which coupling dominates an object's binding at its scale.
double scale_coupling(Scale scale, const LawGenome& g) {
    switch (scale) {
    case Scale::SUBATOMIC:
    case Scale::NUCLEAR:
        return g.coupling_strong;
    case Scale::ATOMIC:
    case Scale::MOLECULAR:
    case Scale::NANOSCALE:
        return g.coupling_em;
    default:
        return g.coupling_gravity;
    }
}

Color8 scale_palette(Scale scale) {
    static const std::array<Color8, kScaleCount> kPalette = {{
        {255, 120, 180}, // subatomic
        {255, 150, 90},  // nuclear
        {120, 200, 255}, // atomic
        {140, 255, 180}, // molecular
        {200, 255, 120}, // nanoscale
        {180, 160, 255}, // planetary
        {255, 230, 140}, // stellar
        {200, 180, 255}, // galactic
        {160, 210, 255}, // cosmic
    }};
    return kPalette[scale_index(scale)];
}

} // namespace

std::vector<UniverseObject> build_object_catalog() {
    std::vector<UniverseObject> c;
    auto add = [&](const char* id, const char* name, const char* symbol, Scale scale,
                   double mass_kg, double charge_e, double radius_m, const char* desc,
                   std::vector<std::string> constituents = {}) {
        UniverseObject o;
        o.id = id;
        o.name = name;
        o.symbol = symbol;
        o.scale = scale;
        o.rest_mass_kg = mass_kg;
        o.charge_e = charge_e;
        o.radius_m = radius_m;
        o.description = desc;
        o.constituents = std::move(constituents);
        c.push_back(std::move(o));
    };

    // --- Subatomic ----------------------------------------------------------
    add("up_quark", "Up quark", "u", Scale::SUBATOMIC, 3.85e-30, 0.6667, 1.0e-19,
        "First-generation up-type quark; pairs into nucleons.");
    add("down_quark", "Down quark", "d", Scale::SUBATOMIC, 8.50e-30, -0.3333, 1.0e-19,
        "First-generation down-type quark.");
    add("electron", "Electron", "e-", Scale::SUBATOMIC, 9.109e-31, -1.0, 1.0e-18,
        "Stable charged lepton; sets chemistry.");
    add("muon", "Muon", "mu", Scale::SUBATOMIC, 1.883e-28, -1.0, 1.0e-18,
        "Heavy unstable lepton.");
    add("neutrino", "Electron neutrino", "nu", Scale::SUBATOMIC, 1.0e-37, 0.0, 1.0e-19,
        "Nearly massless, weakly interacting lepton.");
    add("photon", "Photon", "y", Scale::SUBATOMIC, 1.0e-41, 0.0, 1.0e-19,
        "Massless carrier of the electromagnetic force.");

    // --- Nuclear ------------------------------------------------------------
    add("proton", "Proton", "p+", Scale::NUCLEAR, 1.6726e-27, 1.0, 8.4e-16,
        "Bound uud quark triplet; the lightest baryon.", {"up_quark", "up_quark", "down_quark"});
    add("neutron", "Neutron", "n", Scale::NUCLEAR, 1.6749e-27, 0.0, 8.0e-16,
        "Bound udd quark triplet.", {"up_quark", "down_quark", "down_quark"});
    add("deuteron", "Deuteron", "2H", Scale::NUCLEAR, 3.3436e-27, 1.0, 2.1e-15,
        "Proton + neutron; lightest compound nucleus.", {"proton", "neutron"});
    add("alpha", "Alpha particle", "4He", Scale::NUCLEAR, 6.6447e-27, 2.0, 1.7e-15,
        "Doubly-magic helium nucleus.", {"proton", "proton", "neutron", "neutron"});
    add("carbon_nucleus", "Carbon-12 nucleus", "12C", Scale::NUCLEAR, 1.9926e-26, 6.0, 2.7e-15,
        "Triple-alpha fusion product.");

    // --- Atomic -------------------------------------------------------------
    add("hydrogen", "Hydrogen", "H", Scale::ATOMIC, 1.6735e-27, 0.0, 5.3e-11,
        "Simplest atom; one proton, one electron.", {"proton", "electron"});
    add("helium", "Helium", "He", Scale::ATOMIC, 6.6465e-27, 0.0, 3.1e-11,
        "Inert noble gas.", {"alpha", "electron", "electron"});
    add("carbon", "Carbon", "C", Scale::ATOMIC, 1.9945e-26, 0.0, 7.0e-11,
        "Backbone of organic chemistry.");
    add("oxygen", "Oxygen", "O", Scale::ATOMIC, 2.6566e-26, 0.0, 6.0e-11,
        "Reactive; third most abundant element.");
    add("iron", "Iron", "Fe", Scale::ATOMIC, 9.273e-26, 0.0, 1.26e-10,
        "Peak of nuclear binding; stellar fusion endpoint.");
    add("gold", "Gold", "Au", Scale::ATOMIC, 3.2707e-25, 0.0, 1.74e-10,
        "Heavy element forged in cataclysms.");
    add("uranium", "Uranium", "U", Scale::ATOMIC, 3.9529e-25, 0.0, 1.75e-10,
        "Heaviest primordial element; fissile.");

    // --- Molecular ----------------------------------------------------------
    add("h2", "Hydrogen gas", "H2", Scale::MOLECULAR, 3.347e-27, 0.0, 1.5e-10,
        "Diatomic hydrogen.", {"hydrogen", "hydrogen"});
    add("water", "Water", "H2O", Scale::MOLECULAR, 2.992e-26, 0.0, 2.8e-10,
        "Polar solvent of life.", {"hydrogen", "hydrogen", "oxygen"});
    add("co2", "Carbon dioxide", "CO2", Scale::MOLECULAR, 7.31e-26, 0.0, 3.3e-10,
        "Linear greenhouse molecule.", {"carbon", "oxygen", "oxygen"});
    add("methane", "Methane", "CH4", Scale::MOLECULAR, 2.66e-26, 0.0, 3.8e-10,
        "Simplest hydrocarbon.", {"carbon", "hydrogen", "hydrogen", "hydrogen", "hydrogen"});
    add("ammonia", "Ammonia", "NH3", Scale::MOLECULAR, 2.83e-26, 0.0, 3.2e-10,
        "Nitrogen hydride.");
    add("glucose", "Glucose", "C6H12O6", Scale::MOLECULAR, 2.99e-25, 0.0, 7.0e-10,
        "Primary metabolic sugar.");
    add("dna_bp", "DNA base pair", "bp", Scale::MOLECULAR, 1.0e-24, 0.0, 2.0e-9,
        "Single rung of the genetic ladder.");

    // --- Nanoscale ----------------------------------------------------------
    add("buckyball", "Buckminsterfullerene", "C60", Scale::NANOSCALE, 1.2e-24, 0.0, 7.0e-10,
        "Hollow carbon cage.");
    add("protein", "Protein", "prot", Scale::NANOSCALE, 2.4e-23, 0.0, 2.0e-9,
        "Folded molecular machine.");
    add("virus", "Virus", "vir", Scale::NANOSCALE, 5.0e-19, 0.0, 5.0e-8,
        "Self-replicating nanostructure.");
    add("dust", "Dust grain", "dust", Scale::NANOSCALE, 1.0e-15, 0.0, 1.0e-6,
        "Interstellar/terrestrial particulate.");
    add("bacterium", "Bacterium", "bact", Scale::NANOSCALE, 1.0e-15, 0.0, 1.0e-6,
        "Single-celled organism.");

    // --- Planetary ----------------------------------------------------------
    add("asteroid", "Asteroid (Ceres)", "Cer", Scale::PLANETARY, 9.4e20, 0.0, 4.7e5,
        "Dwarf-planet-sized rubble pile.");
    add("moon", "Moon", "Luna", Scale::PLANETARY, 7.342e22, 0.0, 1.737e6,
        "Rocky natural satellite.");
    add("mars", "Mars", "Mars", Scale::PLANETARY, 6.417e23, 0.0, 3.39e6,
        "Cold desert world.");
    add("earth", "Earth", "Earth", Scale::PLANETARY, 5.972e24, 0.0, 6.371e6,
        "Habitable terrestrial planet.");
    add("jupiter", "Jupiter", "Jup", Scale::PLANETARY, 1.898e27, 0.0, 6.99e7,
        "Gas giant; failed star.");

    // --- Stellar ------------------------------------------------------------
    add("red_dwarf", "Red dwarf", "M*", Scale::STELLAR, 2.4e29, 0.0, 1.4e8,
        "Small, long-lived main-sequence star.");
    add("sun", "Sun-like star", "G*", Scale::STELLAR, 1.989e30, 0.0, 6.96e8,
        "G-type main-sequence star.");
    add("blue_giant", "Blue giant", "O*", Scale::STELLAR, 4.0e31, 0.0, 5.0e9,
        "Massive, short-lived, hot star.");
    add("white_dwarf", "White dwarf", "WD", Scale::STELLAR, 1.2e30, 0.0, 7.0e6,
        "Degenerate stellar remnant.");
    add("neutron_star", "Neutron star", "NS", Scale::STELLAR, 2.8e30, 0.0, 1.2e4,
        "City-sized ball of neutrons.");
    add("black_hole", "Stellar black hole", "BH", Scale::STELLAR, 2.0e31, 0.0, 3.0e4,
        "Collapsed remnant; event horizon.");

    // --- Galactic -----------------------------------------------------------
    add("dwarf_galaxy", "Dwarf galaxy", "dGal", Scale::GALACTIC, 2.0e39, 0.0, 3.0e18,
        "Small satellite galaxy.");
    add("milky_way", "Milky Way", "MW", Scale::GALACTIC, 1.5e42, 0.0, 5.0e20,
        "Barred spiral; our home galaxy.");
    add("andromeda", "Andromeda", "M31", Scale::GALACTIC, 2.4e42, 0.0, 6.5e20,
        "Nearest large spiral galaxy.");
    add("elliptical", "Giant elliptical", "gE", Scale::GALACTIC, 1.0e43, 0.0, 1.0e21,
        "Spheroidal swarm of old stars.");

    // --- Cosmic -------------------------------------------------------------
    add("globular", "Globular cluster", "GC", Scale::COSMIC, 1.0e36, 0.0, 3.0e17,
        "Dense, ancient star cluster.");
    add("galaxy_cluster", "Galaxy cluster", "Virgo", Scale::COSMIC, 2.0e45, 0.0, 1.0e23,
        "Gravitationally bound swarm of galaxies.");
    add("supercluster", "Supercluster", "SC", Scale::COSMIC, 2.0e46, 0.0, 1.0e24,
        "Loose collection of clusters.");
    add("void", "Cosmic void", "void", Scale::COSMIC, 1.0e44, 0.0, 1.0e24,
        "Vast underdense region of the cosmic web.");
    add("universe", "Observable universe", "Obs", Scale::COSMIC, 1.0e53, 0.0, 8.8e26,
        "Everything within the cosmic horizon.");

    return c;
}

void apply_law_genome(std::vector<UniverseObject>& catalog, const LawGenome& genome) {
    // Pass 1: per-scale log ranges so sandbox params are well-conditioned and
    // tier-relative regardless of the absolute SI magnitudes.
    struct Range { double lm_lo = 1e300, lm_hi = -1e300, lr_lo = 1e300, lr_hi = -1e300; };
    std::array<Range, kScaleCount> ranges;
    auto log_mass = [](double m) { return std::log10(std::max(m, 1.0e-45)); };
    auto log_radius = [](double r) { return std::log10(std::max(r, 1.0e-22)); };

    for (const UniverseObject& o : catalog) {
        Range& rg = ranges[scale_index(o.scale)];
        rg.lm_lo = std::min(rg.lm_lo, log_mass(o.rest_mass_kg));
        rg.lm_hi = std::max(rg.lm_hi, log_mass(o.rest_mass_kg));
        rg.lr_lo = std::min(rg.lr_lo, log_radius(o.radius_m));
        rg.lr_hi = std::max(rg.lr_hi, log_radius(o.radius_m));
    }

    auto norm = [](double v, double lo, double hi) {
        return std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
    };
    const double strongN = norm(genome.coupling_strong, 0.35, 1.80);
    const double gravN = norm(genome.coupling_gravity, 0.40, 1.70);

    // Pass 2: generated + sandbox fields.
    for (UniverseObject& o : catalog) {
        const Range& rg = ranges[scale_index(o.scale)];
        const double coupling = scale_coupling(o.scale, genome);

        o.mass_factor = std::clamp(genome.mass_scale * (0.90 + 0.20 * frac(o.id, 1)), 0.5, 2.0);
        o.generated_mass = o.rest_mass_kg * o.mass_factor;
        o.binding = saturate(0.55 * coupling * (0.6 + 0.8 * frac(o.id, 2)));
        o.stability = saturate(genome.stability_bias * (0.5 + 0.6 * frac(o.id, 3)) + 0.35 * o.binding);

        const double mass_norm = map_range(log_mass(o.rest_mass_kg), rg.lm_lo, rg.lm_hi, 0.0, 1.0);
        // Nucleosynthesis flavor: heavy elements (and heavy bodies) become more
        // common when the strong force (or gravity, at large scales) is strong,
        // so each universe gets a visibly different abundance profile.
        const bool matter = (o.scale == Scale::NUCLEAR || o.scale == Scale::ATOMIC ||
                             o.scale == Scale::MOLECULAR);
        const bool cosmic = (o.scale == Scale::PLANETARY || o.scale == Scale::STELLAR ||
                             o.scale == Scale::GALACTIC || o.scale == Scale::COSMIC);
        const double heavy_pref = matter ? strongN : (cosmic ? gravN : 0.5);
        const double mass_pref = (1.0 - 0.55 * mass_norm) * (1.0 - heavy_pref) +
                                 (0.45 + 0.55 * mass_norm) * heavy_pref;
        o.abundance = saturate(o.stability * (0.45 + 0.5 * frac(o.id, 4)) * mass_pref);

        o.sim_mass = map_range(log_mass(o.rest_mass_kg), rg.lm_lo, rg.lm_hi, 0.8, 5.0);
        o.sim_radius = map_range(log_radius(o.radius_m), rg.lr_lo, rg.lr_hi, 0.3, 1.1);
        o.sim_charge = std::clamp(o.charge_e * (0.6 * genome.coupling_em), -3.0, 3.0);

        const Color8 base = scale_palette(o.scale);
        const double bright = 0.55 + 0.45 * o.stability;
        o.color = {
            static_cast<unsigned char>(std::clamp(base.r * bright, 0.0, 255.0)),
            static_cast<unsigned char>(std::clamp(base.g * bright, 0.0, 255.0)),
            static_cast<unsigned char>(std::clamp(base.b * bright, 0.0, 255.0)),
        };
    }
}

std::vector<const UniverseObject*> objects_for_scale(
    const std::vector<UniverseObject>& catalog, Scale scale) {
    std::vector<const UniverseObject*> out;
    for (const UniverseObject& o : catalog) {
        if (o.scale == scale) {
            out.push_back(&o);
        }
    }
    return out;
}

const UniverseObject* find_object(
    const std::vector<UniverseObject>& catalog, const std::string& id) {
    for (const UniverseObject& o : catalog) {
        if (o.id == id) {
            return &o;
        }
    }
    return nullptr;
}

} // namespace cosmos
