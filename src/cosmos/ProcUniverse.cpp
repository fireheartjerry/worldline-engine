#include "cosmos/ProcUniverse.hpp"

#include <array>
#include <cctype>
#include <cmath>

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

std::string gen_name(Rng& r, int min_syl, int max_syl) {
    static const char* on[] = {"ar", "ve", "lo", "ta", "ne", "xi", "qu", "za", "mo", "el",
                               "ka", "sy", "dra", "th", "vor", "lyr", "cae", "nyx", "io", "or",
                               "an", "ul", "is", "ae", "ze", "pho", "rin", "sol", "vel", "cy"};
    static const char* nu[] = {"a", "e", "i", "o", "u", "ae", "ia", "or", "yn", "ar",
                               "el", "is", "on", "us", "ix", "ea"};
    const int n = r.irange(min_syl, max_syl);
    std::string s;
    for (int i = 0; i < n; ++i) {
        s += on[r.u64() % (sizeof(on) / sizeof(on[0]))];
        if (i + 1 < n) s += nu[r.u64() % (sizeof(nu) / sizeof(nu[0]))];
    }
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

// --- Type pickers (each a deterministic function of seed, with its own salt) ---

int galaxy_morph(std::uint64_t seed) { return static_cast<int>(salt(seed, 11) % 3); } // 0 spiral,1 ellip,2 irr
const char* galaxy_morph_name(int m) { return m == 0 ? "spiral" : (m == 1 ? "elliptical" : "irregular"); }

int star_class(std::uint64_t seed) {
    // Weighted toward cooler stars, like the real IMF tail.
    static const int table[] = {6, 6, 6, 6, 5, 5, 5, 4, 4, 3, 2, 1, 0}; // index -> O..M (0..6)
    return table[salt(seed, 12) % (sizeof(table) / sizeof(table[0]))];
}
const char* star_class_name(int c) {
    static const char* n[] = {"O", "B", "A", "F", "G", "K", "M"};
    return n[c % 7];
}
Color8 star_class_color(int c) {
    static const double hue[] = {0.62, 0.60, 0.58, 0.15, 0.12, 0.07, 0.02};
    return hsv8(hue[c % 7], 0.45, 1.0);
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
bool planet_has_life(std::uint64_t seed) {
    const int t = planet_type(seed);
    const double base = (t == 0 || t == 1) ? 0.55 : (t == 5 ? 0.12 : 0.22); // rocky/ocean likelier
    return (static_cast<double>(salt(seed, 14) % 1000) / 1000.0) < base;
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

Identity identity_for(NodeKind kind, std::uint64_t seed) {
    Rng r(seed ^ (0xA24BAED4963EE407ull * (static_cast<std::uint64_t>(kind) + 1)));
    Identity id;
    switch (kind) {
    case NodeKind::Universe:
        id.name = "Observable Universe";
        id.color = hsv8(r.f01(), 0.30, 1.0);
        id.size = 1.0f;
        break;
    case NodeKind::Galaxy:
        id.name = gen_name(r, 2, 3) + ((galaxy_morph(seed) == 1) ? " Cloud" : " Galaxy");
        id.color = hsv8(r.range(0.52, 0.95), 0.32, 1.0);
        id.size = static_cast<float>(r.range(0.5, 1.0));
        break;
    case NodeKind::StarSystem:
        id.name = gen_name(r, 1, 2) + "-" + std::to_string(r.irange(1, 999));
        id.color = star_class_color(star_class(seed));
        id.size = static_cast<float>(r.range(0.45, 0.85));
        break;
    case NodeKind::Planet:
        id.name = gen_name(r, 2, 3);
        id.color = planet_type_color(planet_type(seed));
        id.size = static_cast<float>(r.range(0.3, 0.9));
        break;
    case NodeKind::Ecosystem:
        id.name = gen_name(r, 1, 2) + " Biome";
        id.color = hsv8(r.range(0.20, 0.45), 0.55, 0.88);
        id.size = static_cast<float>(r.range(0.5, 0.95));
        break;
    case NodeKind::Creature:
        id.name = gen_name(r, 1, 2);
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
        const Identity id = identity_for(NodeKind::Galaxy, cs);
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
        const Identity id = identity_for(NodeKind::StarSystem, cs);
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
    const int sclass = star_class(n.seed);
    const int count = r.irange(2, 8);
    n.descriptor = std::string("A ") + star_class_name(sclass) + "-class star with " +
                   std::to_string(count) + " planets.";
    add_fact(n, "Star class", star_class_name(sclass));
    add_fact(n, "Planets", std::to_string(count));
    add_fact(n, "Habitable zone", std::to_string(r.irange(1, count)) + " AU");
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::Planet, cs);
        const double orbit = 0.18 + i * (0.80 / std::max(1, count));
        const double phase = r.f01() * kTau;
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::Planet;
        c.orbit = static_cast<float>(orbit);
        c.phase = static_cast<float>(phase);
        c.x = static_cast<float>(std::cos(phase) * orbit);
        c.y = static_cast<float>(std::sin(phase) * orbit);
        c.size = id.size; c.color = id.color; c.name = id.name;
        n.children.push_back(std::move(c));
    }
}

void gen_planet(ProcNode& n, Rng& r) {
    const int type = planet_type(n.seed);
    const bool life = planet_has_life(n.seed);
    const int count = life ? r.irange(3, 6) : r.irange(2, 3);
    n.descriptor = std::string("A ") + planet_type_name(type) + " world" +
                   (life ? ", teeming with life." : ", barren but for its biomes.");
    add_fact(n, "Type", planet_type_name(type));
    add_fact(n, "Biomes", std::to_string(count));
    add_fact(n, "Life", life ? "present" : "none detected");
    add_fact(n, "Gravity", std::to_string(r.irange(3, 25)) + ".0 m/s2");
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::Ecosystem, cs);
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

void gen_ecosystem(ProcNode& n, Rng& r) {
    const int count = r.irange(12, 36);
    int producers = 0, herbivores = 0, carnivores = 0;
    n.descriptor = "A living biome of " + std::to_string(count) + " interacting species.";
    for (int i = 0; i < count; ++i) {
        const std::uint64_t cs = child_seed(n.seed, static_cast<std::uint64_t>(i));
        const Identity id = identity_for(NodeKind::Creature, cs);
        const int role = creature_role(cs);
        if (role == 0) ++producers; else if (role == 1) ++herbivores; else ++carnivores;
        // Cluster a little by role so the food web reads spatially.
        const double ang = r.f01() * kTau;
        const double rad = std::sqrt(r.f01()) * 0.92;
        ChildRef c;
        c.seed = cs; c.kind = NodeKind::Creature;
        c.x = static_cast<float>(std::cos(ang) * rad);
        c.y = static_cast<float>(std::sin(ang) * rad);
        c.phase = static_cast<float>(r.f01() * kTau);
        c.size = id.size; c.color = id.color; c.name = id.name;
        n.children.push_back(std::move(c));
    }
    add_fact(n, "Species", std::to_string(count));
    add_fact(n, "Producers", std::to_string(producers));
    add_fact(n, "Herbivores", std::to_string(herbivores));
    add_fact(n, "Carnivores", std::to_string(carnivores));
}

void gen_creature(ProcNode& n, Rng& r) {
    const int role = creature_role(n.seed);
    n.descriptor = std::string("A ") + creature_role_name(role) + " of this biome.";
    add_fact(n, "Diet", creature_role_name(role));
    add_fact(n, "Size", std::to_string(r.irange(1, 900)) + " cm");
    add_fact(n, "Speed", std::to_string(r.irange(1, 80)) + " km/h");
    add_fact(n, "Population", std::to_string(r.irange(1, 9)) + "e" + std::to_string(r.irange(2, 9)));
    // Leaf: no children.
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

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
}

ProcNode ProcUniverse::generate(std::uint64_t seed, NodeKind kind) const {
    ProcNode n;
    n.seed = seed;
    n.kind = kind;
    const Identity id = identity_for(kind, seed);
    n.name = id.name;
    n.color = id.color;
    Rng r(salt(seed, 7)); // independent stream for layout/facts
    switch (kind) {
    case NodeKind::Universe:   gen_universe(n, r); break;
    case NodeKind::Galaxy:     gen_galaxy(n, r); break;
    case NodeKind::StarSystem: gen_starsystem(n, r); break;
    case NodeKind::Planet:     gen_planet(n, r); break;
    case NodeKind::Ecosystem:  gen_ecosystem(n, r); break;
    case NodeKind::Creature:   gen_creature(n, r); break;
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

const ProcNode& ProcUniverse::node(std::uint64_t seed, NodeKind kind) {
    auto it = cache_.find(seed);
    if (it != cache_.end()) {
        touch(seed);
        return it->second.node;
    }
    Entry e;
    e.node = generate(seed, kind);
    lru_.push_front(seed);
    e.lru_it = lru_.begin();
    auto res = cache_.emplace(seed, std::move(e));
    trim();
    return res.first->second.node;
}

} // namespace cosmos
