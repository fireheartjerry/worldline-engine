#include "cosmos/ProcUniverse.hpp"

#include "cosmos/Astrobio.hpp"
#include "cosmos/Ecosystem.hpp"
#include "cosmos/Organism.hpp"
#include "cosmos/Phonology.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace cosmos {

namespace {

constexpr double kTau = 6.28318530717958647692;

// SplitMix64 — fast, well-distributed, deterministic across platforms.
struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    std::uint64_t u64() {
        s += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double f01() { return static_cast<double>(u64() >> 11) * (1.0 / 9007199254740992.0); }
    double range(double lo, double hi) { return lo + (hi - lo) * f01(); }
    int irange(int lo, int hi) { return lo + static_cast<int>(f01() * (hi - lo + 1)); } // inclusive
};

std::uint64_t salt(std::uint64_t seed, std::uint64_t s) {
    Rng r(seed ^ (0x9E3779B97F4A7C15ull * (s + 1)));
    return r.u64();
}

Color8 hsv8(double h, double s, double v) {
    h -= std::floor(h);
    const double i = std::floor(h * 6.0);
    const double f = h * 6.0 - i;
    const double p = v * (1.0 - s), q = v * (1.0 - f * s), t = v * (1.0 - (1.0 - f) * s);
    double r = v, g = t, b = p;
    switch (static_cast<int>(i) % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return {static_cast<unsigned char>(r * 255.0),
            static_cast<unsigned char>(g * 255.0),
            static_cast<unsigned char>(b * 255.0)};
}

// --- Type pickers (each a deterministic function of seed, with its own salt) ---

int galaxy_morph(std::uint64_t seed) { return static_cast<int>(salt(seed, 11) % 3); } // 0 spiral,1 ellip,2 irr
const char* galaxy_morph_name(int m) { return m == 0 ? "spiral" : (m == 1 ? "elliptical" : "irregular"); }

// Star class chosen by the real stellar IMF/frequency (M dwarfs dominate).
astro::StarClass star_class_for(std::uint64_t seed) {
    return astro::pick_star_class(static_cast<double>(salt(seed, 12) % 1000000ull) / 1000000.0);
}
Color8 star_class_color(astro::StarClass c) {
    return hsv8(astro::star_info(c).hue, 0.40, 1.0);
}
std::string fmt_g(double v, int prec) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
    return buf;
}

int planet_type(std::uint64_t seed) { return static_cast<int>(salt(seed, 13) % 6); }
const char* planet_type_name(int t) {
    static const char* n[] = {"rocky", "ocean", "gas giant", "ice", "lava", "desert"};
    return n[t % 6];
}
Color8 planet_type_color(int t) {
    static const double hue[] = {0.08, 0.58, 0.10, 0.55, 0.02, 0.11};
    static const double sat[] = {0.45, 0.65, 0.55, 0.30, 0.85, 0.55};
    return hsv8(hue[t % 6], sat[t % 6], 0.92);
}
int creature_role(std::uint64_t seed) {
    // 0 producer, 1 herbivore, 2 carnivore — a rough trophic pyramid.
    const std::uint64_t v = salt(seed, 15) % 100;
    return (v < 45) ? 0 : (v < 80 ? 1 : 2);
}
const char* creature_role_name(int r) { return r == 0 ? "producer" : (r == 1 ? "herbivore" : "carnivore"); }
Color8 creature_role_color(int r) {
    return r == 0 ? hsv8(0.33, 0.6, 0.9) : (r == 1 ? hsv8(0.55, 0.55, 0.95) : hsv8(0.02, 0.7, 0.95));
}

// --- Identity: name/color/size shared by a node's preview AND its full node, so
//     a child looks the same before and after you descend into it. ------------
struct Identity {
    std::string name;
    Color8 color;
    float size = 0.5f;
};

// Per-kind language drift when deriving a node from its parent (subtle, so a
// lineage is recognizable but each level keeps its own character). Galaxies fan
// out furthest from the universe root; same-level siblings stay tight.
float drift_for(NodeKind kind) {
    switch (kind) {
    case NodeKind::Galaxy:     return 0.14f;
    case NodeKind::StarSystem: return 0.06f;
    case NodeKind::Planet:     return 0.05f;
    case NodeKind::Ecosystem:  return 0.05f;
    case NodeKind::Creature:   return 0.07f;
    default:                   return 0.10f;
    }
}

// A node's lineage language: derived from its parent's (so names share a family)
// or, with no parent (root / bare node() call), an isolated language from seed.
// Determinism holds because each seed has exactly one parent in the tree; the
// contract is to always reach a child through its parent (descent_push does).
phon::Language language_for(NodeKind kind, std::uint64_t seed, const ProcNode* parent) {
    if (parent == nullptr) return phon::make_language(seed);
    return phon::derive_language(parent->language, seed, drift_for(kind));
}

Identity identity_for(NodeKind kind, std::uint64_t seed, const phon::Language& lang) {
    Rng r(seed ^ (0xA24BAED4963EE407ull * (static_cast<std::uint64_t>(kind) + 1)));
    Identity id;
    switch (kind) {
    case NodeKind::Universe:
        id.name = "Observable Universe";
        id.color = hsv8(r.f01(), 0.30, 1.0);
        id.size = 1.0f;
        break;
    case NodeKind::Galaxy:
        id.name = phon::generate_name(lang, seed, 1.15f, 0.95f) +
                  ((galaxy_morph(seed) == 1) ? " Cloud" : " Galaxy");
        id.color = hsv8(r.range(0.52, 0.95), 0.32, 1.0);
        id.size = static_cast<float>(r.range(0.5, 1.0));
        break;
    case NodeKind::StarSystem:
        id.name = phon::generate_name(lang, seed, 0.85f, 0.8f) +
                  "-" + std::to_string(r.irange(1, 999));
        id.color = star_class_color(star_class_for(seed));
        id.size = static_cast<float>(r.range(0.45, 0.85));
        break;
    case NodeKind::Planet:
        id.name = phon::generate_name(lang, seed, 1.0f, 0.6f);
        id.color = planet_type_color(planet_type(seed));
        id.size = static_cast<float>(r.range(0.3, 0.9));
        break;
    case NodeKind::Ecosystem:
        id.name = phon::generate_name(lang, seed, 0.9f, 0.4f) + " Biome";
        id.color = hsv8(r.range(0.20, 0.45), 0.55, 0.88);
        id.size = static_cast<float>(r.range(0.5, 0.95));
        break;
    case NodeKind::Creature:
        id.name = phon::generate_name(lang, seed, 1.25f, 0.3f);
        id.color = creature_role_color(creature_role(seed));
        id.size = static_cast<float>(r.range(0.25, 1.0));
        break;
    default:
        break;
    }
    return id;
}

void add_fact(ProcNode& n, const std::string& k, const std::string& v) { n.facts.emplace_back(k, v); }

// --- Per-kind children layout. Positions are normalized to roughly [-1, 1]. ---

void gen_universe(ProcNode& n, Rng& r) {
    const int count = r.irange(16, 30);
    n.descriptor = "A web of " + std::to_string(count) + " galaxies adrift in the dark.";
    add_fact(n, "Galaxies", std::to_string(count));
    add_fact(n, "Age", std::to_string(r.irange(8, 14)) + ".0 Gyr");
    add_fact(n, "Expansion", std::to_string(r.irange(60, 75)) + " km/s/Mpc");
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::Galaxy, cs, language_for(NodeKind::Galaxy, cs, &n));
        const double ang = r.f01() * kTau;
        const double rad = std::sqrt(r.f01());
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::Galaxy;
        c.x = static_cast<float>(std::cos(ang) * rad);
        c.y = static_cast<float>(std::sin(ang) * rad);
        c.size = id.size; c.color = id.color; c.name = id.name;
        n.children.push_back(std::move(c));
    }
}

void gen_galaxy(ProcNode& n, Rng& r) {
    const int morph = galaxy_morph(n.seed);
    const int count = r.irange(28, 60);
    n.descriptor = std::string("A ") + galaxy_morph_name(morph) + " galaxy of ~" +
                   std::to_string(count) + " charted systems.";
    add_fact(n, "Morphology", galaxy_morph_name(morph));
    add_fact(n, "Charted systems", std::to_string(count));
    add_fact(n, "Stars", std::to_string(r.irange(1, 400)) + " billion");
    const int arms = 2 + static_cast<int>(salt(n.seed, 21) % 3);
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::StarSystem, cs, language_for(NodeKind::StarSystem, cs, &n));
        const double t = static_cast<double>(i) / std::max(1, count - 1);
        double x, y;
        if (morph == 0) { // spiral
            const double arm = (i % arms) * (kTau / arms);
            const double ang = t * kTau * 1.6 + arm;
            const double rad = 0.12 + t * 0.86;
            x = std::cos(ang) * rad + r.range(-0.05, 0.05);
            y = std::sin(ang) * rad + r.range(-0.05, 0.05);
        } else if (morph == 1) { // elliptical blob
            const double ang = r.f01() * kTau;
            const double rad = std::pow(r.f01(), 0.6);
            x = std::cos(ang) * rad * 0.95;
            y = std::sin(ang) * rad * 0.7;
        } else { // irregular
            x = r.range(-0.95, 0.95);
            y = r.range(-0.85, 0.85);
        }
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::StarSystem;
        c.x = static_cast<float>(x); c.y = static_cast<float>(y);
        c.size = id.size; c.color = id.color; c.name = id.name;
        n.children.push_back(std::move(c));
    }
}

void gen_starsystem(ProcNode& n, Rng& r) {
    const astro::StarClass sclass = star_class_for(n.seed);
    const astro::StarInfo& star = astro::star_info(sclass);
    const double lum = astro::star_luminosity(star.mass_sun);
    const astro::HabitableZone hz = astro::habitable_zone(lum);
    n.subtype = static_cast<int>(sclass);
    n.luminosity = lum;

    const int count = r.irange(2, 8);
    n.descriptor = std::string("A ") + star.name + "-class star (" +
                   fmt_g(star.lifetime_gyr, 2) + " Gyr lifetime) with " +
                   std::to_string(count) + " planets.";
    add_fact(n, "Star class", std::string(star.name) + " (" + fmt_g(star.temp_k, 4) + " K)");
    add_fact(n, "Mass", fmt_g(star.mass_sun, 2) + " Msun");
    add_fact(n, "Luminosity", fmt_g(lum, 2) + " Lsun");
    add_fact(n, "Lifetime", fmt_g(star.lifetime_gyr, 2) + " Gyr");
    add_fact(n, "Habitable zone", fmt_g(hz.inner_au, 2) + "-" + fmt_g(hz.outer_au, 2) + " AU");
    add_fact(n, "Planets", std::to_string(count));

    // Planets on Titius-Bode-ish rings, scaled by the star so HZ is meaningful.
    const double base_au = 0.25 * std::sqrt(std::max(0.02, lum));
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::Planet, cs, language_for(NodeKind::Planet, cs, &n));
        const double au = base_au * std::pow(1.6, i) * r.range(0.9, 1.1);
        const bool rocky = planet_type(cs) == 0 || planet_type(cs) == 1 || planet_type(cs) == 5;
        const bool in_hz = astro::in_habitable_zone(au, lum);
        const double phase = r.f01() * kTau;
        const double orbit = 0.18 + i * (0.80 / std::max(1, count)); // normalized layout ring
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::Planet;
        c.orbit = static_cast<float>(orbit);
        c.phase = static_cast<float>(phase);
        c.x = static_cast<float>(std::cos(phase) * orbit);
        c.y = static_cast<float>(std::sin(phase) * orbit);
        c.size = id.size; c.color = id.color; c.name = id.name;
        c.orbit_au = au;
        c.habitable = rocky && in_hz && star.habitability > 0.3; // preview hint (ring marker)
        n.children.push_back(std::move(c));
    }
}

// Derive a planet's climate + whether it actually hosts life, from its host
// star (parent) and its own orbit. Stacked gates leave most worlds barren.
void gen_planet(ProcNode& n, Rng& r, const ProcNode* parent) {
    const int type = planet_type(n.seed);
    n.subtype = type;

    double lum = 1.0, orbit_au = 1.0, star_life = 10.0, star_hab = 0.9;
    if (parent != nullptr && parent->kind == NodeKind::StarSystem) {
        lum = parent->luminosity > 0.0 ? parent->luminosity : 1.0;
        const astro::StarInfo& star = astro::star_info(
            static_cast<astro::StarClass>(std::clamp(parent->subtype, 0, 6)));
        star_life = star.lifetime_gyr;
        star_hab = star.habitability;
        for (const ChildRef& c : parent->children) {
            if (c.seed == n.seed) { orbit_au = c.orbit_au; break; }
        }
    }
    n.orbit_au = orbit_au;

    const astro::HabitableZone hz = astro::habitable_zone(lum);
    const bool in_hz = orbit_au >= hz.inner_au && orbit_au <= hz.outer_au;
    const bool rocky = (type == 0 || type == 1 || type == 5); // rocky/ocean/desert
    const double mass_earth = rocky ? r.range(0.3, 4.5) : r.range(10.0, 300.0);

    // Surface temperature from flux (inner edge ~ +50C, outer edge ~ -40C).
    if (in_hz) {
        const double f = (orbit_au - hz.inner_au) / std::max(1.0e-6, hz.outer_au - hz.inner_au);
        n.temperature_c = 50.0 - 90.0 * f + r.range(-8.0, 8.0);
    } else {
        n.temperature_c = (orbit_au < hz.inner_au) ? r.range(120.0, 460.0) : r.range(-160.0, -45.0);
    }
    n.precip_mm = (type == 1) ? r.range(1500.0, 4000.0)            // ocean world: wet
                : in_hz       ? r.range(150.0, 3000.0)
                              : r.range(0.0, 50.0);

    // Life gate (stacked, independent → rare): star long-lived enough for complex
    // life, planet in the HZ, rocky, right mass for atmosphere/dynamo, and a final
    // complex-life roll weighted by stellar suitability.
    const bool gate = (star_life >= 4.0) && in_hz && rocky &&
                      (mass_earth >= 0.3 && mass_earth <= 5.0);
    const double life_roll = static_cast<double>(salt(n.seed, 31) % 1000) / 1000.0;
    n.habitable = gate && (life_roll < 0.55 * star_hab);

    add_fact(n, "Type", planet_type_name(type));
    add_fact(n, "Orbit", fmt_g(orbit_au, 2) + " AU" + (in_hz ? " (in HZ)" : ""));
    add_fact(n, "Mass", fmt_g(mass_earth, 2) + " Mearth");
    add_fact(n, "Surface temp", fmt_g(n.temperature_c, 3) + " C");
    add_fact(n, "Life", n.habitable ? "present" : (gate ? "absent (chance)" : "uninhabitable"));

    if (!n.habitable) {
        n.descriptor = std::string("A ") + planet_type_name(type) + " world — " +
                       (in_hz ? "in the habitable zone but lifeless." : "outside the habitable zone, barren.");
        return; // barren: a dead-end (no ecosystems to descend into)
    }

    // Living world: biomes (ecosystems) seeded by climate, count by richness.
    const int count = 2 + static_cast<int>(astro::richness_factor(
                          astro::npp_for(astro::biome_for(n.temperature_c, n.precip_mm))) * 5.0 + 0.5);
    n.descriptor = std::string("A living ") + planet_type_name(type) + " world with " +
                   std::to_string(count) + " biomes.";
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::Ecosystem, cs, language_for(NodeKind::Ecosystem, cs, &n));
        const double ang = (static_cast<double>(i) / count) * kTau + r.range(-0.2, 0.2);
        const double rad = r.range(0.45, 0.9);
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::Ecosystem;
        c.x = static_cast<float>(std::cos(ang) * rad);
        c.y = static_cast<float>(std::sin(ang) * rad);
        c.size = id.size; c.color = id.color; c.name = id.name;
        n.children.push_back(std::move(c));
    }
}

// Rebuild the deterministic community for an ecosystem node from its stored
// climate (so the ecosystem panel and each creature it contains agree).
eco::Community ecosystem_community(const ProcNode& eco_node) {
    eco::BiomeParams b;
    b.temp_c = eco_node.temperature_c;
    b.precip_mm = eco_node.precip_mm;
    const astro::Biome biome = astro::biome_for(b.temp_c, b.precip_mm);
    b.npp = astro::npp_for(biome);
    b.aquatic = (biome == astro::Biome::Ocean);
    // Pass the ecosystem's lineage language so species names share the world's
    // phonetic family (and creature preview == creature full node).
    return eco::generate_community(eco_node.seed, b, &eco_node.language);
}

void gen_ecosystem(ProcNode& n, Rng& r, const ProcNode* parent, const ProcUniverse* self) {
    // Biome from the planet's climate, perturbed per-ecosystem (latitude band).
    double temp = 18.0, precip = 1200.0;
    if (parent != nullptr && parent->kind == NodeKind::Planet) {
        temp = parent->temperature_c + r.range(-12.0, 12.0);
        precip = std::max(0.0, parent->precip_mm * r.range(0.5, 1.4));
    }
    const astro::Biome biome = astro::biome_for(temp, precip);
    n.subtype = static_cast<int>(biome);
    n.temperature_c = temp; // stored so creatures rebuild the same community
    n.precip_mm = precip;
    n.color = hsv8(astro::biome_hue(biome), 0.6, 0.85);

    // Assemble a full continuous trait-based community (the real ecosystem).
    const eco::Community& comm = self ? self->community_cached(n) : ecosystem_community(n);
    const double npp = astro::npp_for(biome);
    n.descriptor = std::string(astro::biome_name(biome)) + " — " +
                   std::to_string(comm.species.size()) + " species across " +
                   fmt_g(comm.stats.n_levels + 1.0, 2) + " trophic levels.";

    // Lay species out as a food web: y = trophic level (producers low, apex high),
    // x = log body mass within the level (Hutchinson spacing reads as gaps).
    const double maxtau = std::max(1.0, comm.stats.n_levels);
    for (std::size_t i = 0; i < comm.species.size(); ++i) {
        const eco::Species& sp = comm.species[i];
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::Creature;
        c.y = static_cast<float>(0.82 - 1.64 * (sp.t.tau / maxtau));
        c.x = static_cast<float>(clampd((sp.t.m + 4.0) / 9.0, 0.0, 1.0) * 1.6 - 0.8 +
                                 0.06 * std::sin(sp.t.phi * kTau));
        c.phase = static_cast<float>(sp.t.phi * kTau);
        c.size = static_cast<float>(clampd(0.3 + 0.11 * (sp.t.m + 4.0), 0.2, 1.0));
        c.color = sp.color; c.name = sp.name;
        c.subtype = static_cast<int>(clampd(sp.t.tau, 0.0, 3.0));
        c.orbit_au = sp.t.tau; // stash trophic level for the renderer
        n.children.push_back(std::move(c));
    }

    add_fact(n, "Biome", astro::biome_name(biome));
    add_fact(n, "Temperature", fmt_g(temp, 3) + " C");
    add_fact(n, "NPP", fmt_g(npp, 2) + " g/m2/yr");
    add_fact(n, "Species", std::to_string(comm.stats.n_species));
    add_fact(n, "Trophic levels", fmt_g(comm.stats.n_levels + 1.0, 2));
    add_fact(n, "Food-web links", std::to_string(comm.stats.n_links));
    add_fact(n, "Connectance", fmt_g(comm.stats.connectance, 2));
    add_fact(n, "Stability margin", fmt_g(comm.stats.stability_margin, 2));
    add_fact(n, "Total biomass", fmt_g(comm.stats.total_biomass, 2) + " kg/m2");
    if (comm.stats.keystone >= 0 && comm.stats.keystone < static_cast<int>(comm.species.size()))
        add_fact(n, "Keystone", comm.species[static_cast<std::size_t>(comm.stats.keystone)].name);
}

void gen_creature(ProcNode& n, Rng& r, const ProcNode* parent, const ProcUniverse* self) {
    if (parent != nullptr && parent->kind == NodeKind::Ecosystem) {
        const eco::Community& comm = self ? self->community_cached(*parent) : ecosystem_community(*parent);
        int idx = -1;
        for (std::size_t k = 0; k < parent->children.size(); ++k)
            if (parent->children[k].seed == n.seed) { idx = static_cast<int>(k); break; }
        if (idx >= 0 && idx < static_cast<int>(comm.species.size())) {
            const eco::Species& s = comm.species[static_cast<std::size_t>(idx)];
            n.name = s.name; n.color = s.color;
            n.subtype = static_cast<int>(clampd(s.t.tau, 0.0, 3.0));
            n.descriptor = std::string("A ") + eco::role_label(s.t.tau) +
                           " — trophic level " + fmt_g(s.t.tau + (s.t.tau >= 0.5 ? 0.0 : 0.0), 2) + ".";
            add_fact(n, "Trophic role", eco::role_label(s.t.tau));
            add_fact(n, "Trophic level", fmt_g(s.t.tau, 2));
            add_fact(n, "Body mass", s.mass_kg < 1.0 ? fmt_g(s.mass_kg * 1000.0, 2) + " g"
                                                     : fmt_g(s.mass_kg, 2) + " kg");
            // Full organism physiology from the Metabolic Theory of Ecology.
            const bio::Organism org = bio::derive_organism(s.t, s.t.T_opt);
            add_fact(n, "Metabolism (BMR)", fmt_g(org.bmr_w, 2) + " W");
            add_fact(n, "Thermoregulation", org.endothermy > 0.66 ? "endotherm"
                                          : (org.endothermy < 0.33 ? "ectotherm" : "mesotherm"));
            add_fact(n, "Lifespan", fmt_g(org.lifespan_yr, 2) + " yr");
            add_fact(n, "Maturity age", fmt_g(org.age_maturity_yr, 2) + " yr");
            add_fact(n, "Max speed", fmt_g(org.speed_ms, 2) + " m/s");
            add_fact(n, "Home range", org.home_range_m2 > 1.0e6
                         ? fmt_g(org.home_range_m2 / 1.0e6, 2) + " km^2"
                         : fmt_g(org.home_range_m2, 2) + " m^2");
            add_fact(n, "Population", fmt_g(s.population, 2));
            add_fact(n, "Thermal optimum", fmt_g(s.t.T_opt, 3) + " C");
            add_fact(n, "Activity", s.t.phi < 0.25 || s.t.phi > 0.75 ? "diurnal" : "nocturnal");
            add_fact(n, "Strategy", s.t.rho > 0.6 ? "r (fast, many young)"
                                  : (s.t.rho < 0.4 ? "K (slow, few young)" : "intermediate"));
            add_fact(n, "Defense", fmt_g(s.t.defense, 2));
            // Top prey from the food web.
            int best = -1; double bestp = 0.0;
            for (const eco::Link& lk : comm.links)
                if (lk.pred == idx && lk.pref > bestp) { bestp = lk.pref; best = lk.prey; }
            if (best >= 0)
                add_fact(n, "Main prey", comm.species[static_cast<std::size_t>(best)].name);
            return;
        }
    }
    // Fallback (no ecosystem context): a lone organism.
    const int role = creature_role(n.seed);
    n.subtype = role;
    n.descriptor = "An organism.";
    add_fact(n, "Trophic role", creature_role_name(role));
    add_fact(n, "Body mass", fmt_g(r.range(0.01, 100.0), 2) + " kg");
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

eco::Community community_for_ecosystem(const ProcNode& ecosystem_node) {
    return ecosystem_community(ecosystem_node);
}

const char* node_kind_name(NodeKind kind) {
    switch (kind) {
    case NodeKind::Universe:   return "Universe";
    case NodeKind::Galaxy:     return "Galaxy";
    case NodeKind::StarSystem: return "Star system";
    case NodeKind::Planet:     return "Planet";
    case NodeKind::Ecosystem:  return "Ecosystem";
    case NodeKind::Creature:   return "Creature";
    default:                   return "?";
    }
}

const char* node_child_noun(NodeKind kind) {
    switch (kind) {
    case NodeKind::Universe:   return "galaxies";
    case NodeKind::Galaxy:     return "systems";
    case NodeKind::StarSystem: return "planets";
    case NodeKind::Planet:     return "biomes";
    case NodeKind::Ecosystem:  return "species";
    default:                   return "";
    }
}

NodeKind node_child_kind(NodeKind kind) {
    switch (kind) {
    case NodeKind::Universe:   return NodeKind::Galaxy;
    case NodeKind::Galaxy:     return NodeKind::StarSystem;
    case NodeKind::StarSystem: return NodeKind::Planet;
    case NodeKind::Planet:     return NodeKind::Ecosystem;
    case NodeKind::Ecosystem:  return NodeKind::Creature;
    default:                   return NodeKind::Creature;
    }
}

bool node_is_leaf(NodeKind kind) { return kind == NodeKind::Creature; }

std::uint64_t child_seed(std::uint64_t parent_seed, std::uint64_t index) {
    Rng r(parent_seed ^ (0x9E3779B97F4A7C15ull * (index + 1)));
    return r.u64();
}

ProcUniverse::ProcUniverse(std::uint64_t root_seed) : root_seed_(root_seed ? root_seed : 1ull) {}

void ProcUniverse::reseed(std::uint64_t root_seed) {
    root_seed_ = root_seed ? root_seed : 1ull;
    cache_.clear();
    lru_.clear();
    eco_cache_.clear();
    eco_lru_.clear();
}

const eco::Community& ProcUniverse::community_cached(const ProcNode& eco_node) const {
    const std::uint64_t key = eco_node.seed;
    auto it = eco_cache_.find(key);
    if (it != eco_cache_.end()) {
        eco_lru_.erase(it->second.lru_it);
        eco_lru_.push_front(key);
        it->second.lru_it = eco_lru_.begin();
        return it->second.comm;
    }
    EcoEntry e;
    e.comm = ecosystem_community(eco_node); // pure: same node -> identical community
    eco_lru_.push_front(key);
    e.lru_it = eco_lru_.begin();
    auto res = eco_cache_.emplace(key, std::move(e));
    while (eco_cache_.size() > eco_budget_ && !eco_lru_.empty()) {
        const std::uint64_t old = eco_lru_.back();
        eco_lru_.pop_back();
        eco_cache_.erase(old);
    }
    return res.first->second.comm;
}

ProcNode ProcUniverse::generate(std::uint64_t seed, NodeKind kind, const ProcNode* parent) const {
    ProcNode n;
    n.seed = seed;
    n.kind = kind;
    n.language = language_for(kind, seed, parent); // lineage family (before naming)
    const Identity id = identity_for(kind, seed, n.language);
    n.name = id.name;
    n.color = id.color;
    Rng r(salt(seed, 7)); // independent stream for layout/facts
    switch (kind) {
    case NodeKind::Universe:   gen_universe(n, r); break;
    case NodeKind::Galaxy:     gen_galaxy(n, r); break;
    case NodeKind::StarSystem: gen_starsystem(n, r); break;
    case NodeKind::Planet:     gen_planet(n, r, parent); break;
    case NodeKind::Ecosystem:  gen_ecosystem(n, r, parent, this); break;
    case NodeKind::Creature:   gen_creature(n, r, parent, this); break;
    default: break;
    }
    return n;
}

void ProcUniverse::touch(std::uint64_t key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return;
    lru_.erase(it->second.lru_it);
    lru_.push_front(key);
    it->second.lru_it = lru_.begin();
}

void ProcUniverse::trim() {
    while (cache_.size() > budget_ && !lru_.empty()) {
        const std::uint64_t key = lru_.back();
        lru_.pop_back();
        cache_.erase(key);
    }
}

const ProcNode& ProcUniverse::node(std::uint64_t seed, NodeKind kind, const ProcNode* parent) {
    auto it = cache_.find(seed);
    if (it != cache_.end()) {
        touch(seed);
        return it->second.node;
    }
    Entry e;
    e.node = generate(seed, kind, parent);
    lru_.push_front(seed);
    e.lru_it = lru_.begin();
    auto res = cache_.emplace(seed, std::move(e));
    trim();
    return res.first->second.node;
}

} // namespace cosmos
