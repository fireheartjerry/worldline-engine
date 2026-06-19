#pragma once
// Deterministic, lazily-generated, LRU-cached procedural universe.
//
// Every place in the universe is a node identified by a 64-bit seed. A node's
// entire content is a pure function of its seed, so the same place always
// regenerates bit-identically — there is no stored world and no drift. A node
// is generated together with *references* to its children (their seeds,
// positions and a cheap preview), but NOT the children's full content: that is
// generated only when you descend into a child. This keeps the work per view
// bounded (you never generate a whole galaxy's planets at once).
//
// Generated nodes live in an LRU cache with a hard budget; least-recently-used
// nodes are evicted past the cap. Because generation is deterministic, evicted
// nodes return identically when revisited, so memory stays bounded with no
// glitches. This is the reusable engine; per-level richness is layered on top.

#include "cosmos/Ecosystem.hpp"
#include "cosmos/ObjectCatalog.hpp" // Color8
#include "cosmos/Phonology.hpp"     // phon::Language (lineage naming)

#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cosmos {

// The descent ladder. Each kind's children are the next kind down; Creature is
// a leaf. New intermediate levels can be inserted here as the system grows.
enum class NodeKind {
    Universe,
    Galaxy,
    StarSystem,
    Planet,
    Ecosystem,
    Creature,
    COUNT
};

const char* node_kind_name(NodeKind kind);   // "Universe", "Galaxy", ...
const char* node_child_noun(NodeKind kind);  // "galaxies", "systems", "planets", ...
NodeKind    node_child_kind(NodeKind kind);  // what this node's children are
bool        node_is_leaf(NodeKind kind);     // Creature (no children)

// A cheap, eagerly-computed reference to a child: enough to lay it out and draw
// a preview without generating the child's full content.
struct ChildRef {
    std::uint64_t seed = 0;
    NodeKind kind = NodeKind::Galaxy;
    float x = 0.0f;       // layout position, normalized to roughly [-1, 1]
    float y = 0.0f;
    float orbit = 0.0f;   // orbital radius (for ring/orbit layouts), 0 if unused
    float phase = 0.0f;   // initial angle for animated layouts
    float size = 0.5f;    // preview size, [0, 1]
    Color8 color;
    std::string name;
    int subtype = 0;          // star class / planet type / biome / trophic role
    double orbit_au = 0.0;    // planet orbital distance (AU), parent-assigned
    bool habitable = false;   // planet preview: lies in the habitable zone & rocky
};

// A fully generated node: its own attributes plus references to its children.
struct ProcNode {
    std::uint64_t seed = 0;
    NodeKind kind = NodeKind::Universe;
    std::string name;
    std::string descriptor;   // one-line flavor summary
    Color8 color;
    std::vector<std::pair<std::string, std::string>> facts; // inspector key/value stats
    std::vector<ChildRef> children;

    // Lineage naming: the root seeds a phonetic family; each child drifts from
    // its parent's language (derive_language), so a galaxy and its systems /
    // planets / species share a recognizable name family. Set in generate()
    // before naming. A node reached without a parent falls back to make_language.
    phon::Language language;

    // Inter-level context (set per kind; propagates parent -> child generation).
    int    subtype = 0;          // star class / planet type / biome
    double luminosity = 0.0;     // StarSystem: host-star luminosity (L_sun)
    double orbit_au = 0.0;       // Planet: orbital distance (AU)
    double temperature_c = 0.0;  // Planet/Ecosystem: surface temperature
    double precip_mm = 0.0;      // Planet/Ecosystem: precipitation
    bool   habitable = false;    // Planet: hosts life

    // Exact physical quantities for the analysis instruments (you-are-here plots).
    double phys_mass = 0.0;   // Planet: M_earth; Star: M_sun; Galaxy: M_sun (stellar)
    double phys_radius = 0.0; // Planet: R_earth; Star: effective temp (K)
    double phys_aux = 0.0;    // Star: lifetime Gyr; Galaxy: SMBH mass M_sun; Planet: axial tilt
};

// Deterministically derive a child's seed from a parent seed and child index.
std::uint64_t child_seed(std::uint64_t parent_seed, std::uint64_t index);

// Rebuild the deterministic ecosystem community for an Ecosystem node (its
// stored climate). Used by the descent view to draw/animate the food web.
eco::Community community_for_ecosystem(const ProcNode& ecosystem_node);

class ProcUniverse {
public:
    explicit ProcUniverse(std::uint64_t root_seed);

    std::uint64_t root_seed() const { return root_seed_; }

    // Generate-or-fetch a node. The result is cached (LRU); revisiting is cheap,
    // and an evicted node regenerates identically. `parent` supplies inter-level
    // context (e.g. a planet needs its star's luminosity + its own orbit); it is
    // deterministic given the child seed, so caching by seed stays valid.
    const ProcNode& node(std::uint64_t seed, NodeKind kind, const ProcNode* parent = nullptr);
    const ProcNode& root() { return node(root_seed_, NodeKind::Universe); }

    std::size_t cache_size() const { return cache_.size(); }
    std::size_t budget() const { return budget_; }
    void set_budget(std::size_t n) { budget_ = (n < 1) ? 1 : n; trim(); }

    // Re-root to a new universe seed (clears the cache).
    void reseed(std::uint64_t root_seed);

    // Memoized community for an Ecosystem node (bounded LRU keyed by its seed) so
    // descending into many creatures doesn't recompute the O(S^2) food web each
    // time. Pure: same node -> identical community, so it stays deterministic.
    const eco::Community& community_cached(const ProcNode& eco_node) const;

private:
    struct Entry {
        ProcNode node;
        std::list<std::uint64_t>::iterator lru_it;
    };
    struct EcoEntry {
        eco::Community comm;
        std::list<std::uint64_t>::iterator lru_it;
    };

    ProcNode generate(std::uint64_t seed, NodeKind kind, const ProcNode* parent) const;
    void touch(std::uint64_t key);
    void trim();

    std::uint64_t root_seed_;
    std::size_t budget_ = 1024;
    std::list<std::uint64_t> lru_;                       // front = most recently used
    std::unordered_map<std::uint64_t, Entry> cache_;

    // Community memo-cache (mutable: a pure memoization layer over const generation).
    std::size_t eco_budget_ = 64;
    mutable std::list<std::uint64_t> eco_lru_;
    mutable std::unordered_map<std::uint64_t, EcoEntry> eco_cache_;
};

} // namespace cosmos
