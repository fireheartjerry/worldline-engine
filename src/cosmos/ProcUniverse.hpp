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

#include "cosmos/ObjectCatalog.hpp" // Color8

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
};

// Deterministically derive a child's seed from a parent seed and child index.
std::uint64_t child_seed(std::uint64_t parent_seed, std::uint64_t index);

class ProcUniverse {
public:
    explicit ProcUniverse(std::uint64_t root_seed);

    std::uint64_t root_seed() const { return root_seed_; }

    // Generate-or-fetch a node. The result is cached (LRU); revisiting is cheap,
    // and an evicted node regenerates identically.
    const ProcNode& node(std::uint64_t seed, NodeKind kind);
    const ProcNode& root() { return node(root_seed_, NodeKind::Universe); }

    std::size_t cache_size() const { return cache_.size(); }
    std::size_t budget() const { return budget_; }
    void set_budget(std::size_t n) { budget_ = (n < 1) ? 1 : n; trim(); }

    // Re-root to a new universe seed (clears the cache).
    void reseed(std::uint64_t root_seed);

private:
    struct Entry {
        ProcNode node;
        std::list<std::uint64_t>::iterator lru_it;
    };

    ProcNode generate(std::uint64_t seed, NodeKind kind) const;
    void touch(std::uint64_t key);
    void trim();

    std::uint64_t root_seed_;
    std::size_t budget_ = 1024;
    std::list<std::uint64_t> lru_;                       // front = most recently used
    std::unordered_map<std::uint64_t, Entry> cache_;
};

} // namespace cosmos
