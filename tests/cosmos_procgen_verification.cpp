// Verifies the procedural universe engine: deterministic generation, correct
// hierarchy structure, preview/full consistency, and a bounded LRU cache that
// regenerates evicted nodes identically.

#include "cosmos/ProcUniverse.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

int g_failures = 0;

void require(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_procgen_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}

bool color_eq(Color8 a, Color8 b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

bool nodes_equal(const ProcNode& a, const ProcNode& b) {
    if (a.seed != b.seed || a.kind != b.kind || a.name != b.name) return false;
    if (!color_eq(a.color, b.color)) return false;
    if (a.children.size() != b.children.size()) return false;
    for (std::size_t i = 0; i < a.children.size(); ++i) {
        const ChildRef& x = a.children[i];
        const ChildRef& y = b.children[i];
        if (x.seed != y.seed || x.kind != y.kind || x.name != y.name) return false;
        if (x.x != y.x || x.y != y.y) return false;
    }
    return true;
}

// Walk the hierarchy down, checking structure invariants. Passes the parent so
// context-aware generation (planet habitability, ecosystem biome) is correct.
void walk(ProcUniverse& uni, std::uint64_t seed, NodeKind kind, const ProcNode* parent,
          int depth, int& visited) {
    const ProcNode n = uni.node(seed, kind, parent); // copy (cache may move underneath)
    ++visited;

    require(!n.name.empty(), "every node must have a name");
    require(n.kind == kind, "node kind must match the requested kind");

    if (node_is_leaf(kind)) {
        require(n.children.empty(), "creature (leaf) must have no children");
        return;
    }
    // A barren planet legitimately has no children (a dead-end you can't enter).
    if (n.children.empty()) {
        require(kind == NodeKind::Planet,
                std::string(node_kind_name(kind)) + " must have children");
        return;
    }

    const NodeKind ck = node_child_kind(kind);
    for (const ChildRef& c : n.children) {
        require(c.kind == ck, "child kind must be the parent's child kind");
        require(std::isfinite(c.x) && std::isfinite(c.y), "child position must be finite");
        require(std::abs(c.x) <= 1.2f && std::abs(c.y) <= 1.2f, "child position must be in view bounds");
        require(!c.name.empty(), "child preview must carry a name");
    }

    // Descend into the first child; the preview NAME must match the full node
    // (names are seed-only; colors are now context-dependent so are not compared).
    if (depth < 5) {
        const ChildRef& first = n.children.front();
        const ProcNode child = uni.node(first.seed, first.kind, &n);
        require(child.name == first.name, "child preview name must match its full node name");
        walk(uni, first.seed, first.kind, &n, depth + 1, visited);
    }
}

} // namespace

int main() {
    // --- Determinism: two universes with the same seed are identical ----------
    {
        ProcUniverse a(0xCAFEBABEull);
        ProcUniverse b(0xCAFEBABEull);
        const ProcNode ra = a.root();
        const ProcNode rb = b.root();
        require(nodes_equal(ra, rb), "same root seed must yield identical roots");
        require(ra.kind == NodeKind::Universe, "root must be a Universe");
        require(!ra.children.empty(), "universe must contain galaxies");
        // Descend the same path in both and compare.
        const std::uint64_t g = ra.children.front().seed;
        require(nodes_equal(a.node(g, NodeKind::Galaxy), b.node(g, NodeKind::Galaxy)),
                "same galaxy seed must regenerate identically");
    }

    // --- Different seeds give different universes -----------------------------
    {
        ProcUniverse a(1ull), b(2ull);
        const ProcNode ra = a.root();
        const ProcNode rb = b.root();
        require(!nodes_equal(ra, rb), "different seeds should give different universes");
    }

    // --- Structure + preview/full consistency over a descent path -------------
    {
        ProcUniverse uni(0x1234567ull);
        int visited = 0;
        walk(uni, uni.root_seed(), NodeKind::Universe, nullptr, 0, visited);
        // Universe -> Galaxy -> StarSystem -> Planet is always reachable; the
        // first planet may be barren (a valid dead-end), so we require >= 4.
        require(visited >= 4, "should visit at least universe..planet");
    }

    // --- Life is gated: it appears somewhere, most planets are barren, and a
    //     living world descends into ecosystems -> creatures ------------------
    {
        ProcUniverse uni(0x5EED1ABEull);
        const ProcNode universe = uni.root();
        int planets = 0, living = 0;
        bool descended_life = false;
        for (const ChildRef& gref : universe.children) {
            const ProcNode galaxy = uni.node(gref.seed, gref.kind, &universe);
            for (std::size_t s = 0; s < galaxy.children.size() && s < 12; ++s) {
                const ProcNode sys = uni.node(galaxy.children[s].seed,
                                              galaxy.children[s].kind, &galaxy);
                for (const ChildRef& pref : sys.children) {
                    const ProcNode planet = uni.node(pref.seed, pref.kind, &sys);
                    ++planets;
                    if (planet.habitable) {
                        ++living;
                        require(!planet.children.empty(), "a living planet must have biomes");
                        const ProcNode eco = uni.node(planet.children.front().seed,
                                                      planet.children.front().kind, &planet);
                        require(!eco.children.empty(), "an ecosystem must have species");
                        const ProcNode creature = uni.node(eco.children.front().seed,
                                                           eco.children.front().kind, &eco);
                        require(node_is_leaf(creature.kind) && creature.children.empty(),
                                "a creature is a leaf");
                        descended_life = true;
                    } else {
                        require(planet.children.empty(), "a barren planet has no biomes");
                    }
                }
            }
            if (planets > 200) break;
        }
        require(planets > 50, "sample should contain many planets");
        require(living >= 1, "life should appear somewhere in a large sample");
        require(living < planets, "most worlds must be barren (life is rare)");
        require(descended_life, "must descend a living world to a creature");
    }

    // --- LRU cache: bounded size + identical regeneration after eviction ------
    {
        ProcUniverse uni(0xABCDEFull);
        uni.set_budget(32);
        const ProcNode root = uni.root();

        // Capture a galaxy's full node, then thrash the cache so it is evicted.
        const std::uint64_t gseed = root.children.front().seed;
        const ProcNode galaxy_before = uni.node(gseed, NodeKind::Galaxy);

        // Generate far more distinct nodes than the budget by descending widely.
        for (const ChildRef& g : root.children) {
            const ProcNode gn = uni.node(g.seed, NodeKind::Galaxy);
            for (const ChildRef& sys : gn.children) {
                uni.node(sys.seed, NodeKind::StarSystem);
            }
        }
        require(uni.cache_size() <= uni.budget(), "cache must never exceed its budget");

        // The first galaxy has long since been evicted; it must come back identical.
        const ProcNode galaxy_after = uni.node(gseed, NodeKind::Galaxy);
        require(nodes_equal(galaxy_before, galaxy_after),
                "an evicted node must regenerate bit-identically");
    }

    // --- reseed clears the cache and changes the universe ---------------------
    {
        ProcUniverse uni(7ull);
        const ProcNode r7 = uni.root();
        uni.reseed(8ull);
        require(uni.cache_size() == 0, "reseed must clear the cache");
        const ProcNode r8 = uni.root();
        require(!nodes_equal(r7, r8), "reseed must change the universe");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_procgen_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_procgen_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
