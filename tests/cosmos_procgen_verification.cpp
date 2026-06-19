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

// Walk the whole hierarchy down to creatures, checking structure invariants.
void walk(ProcUniverse& uni, std::uint64_t seed, NodeKind kind, int depth, int& visited) {
    const ProcNode n = uni.node(seed, kind); // copy (cache may move underneath)
    ++visited;

    require(!n.name.empty(), "every node must have a name");
    require(n.kind == kind, "node kind must match the requested kind");

    if (node_is_leaf(kind)) {
        require(n.children.empty(), "creature (leaf) must have no children");
        return;
    }
    require(!n.children.empty(), std::string(node_kind_name(kind)) + " must have children");

    const NodeKind ck = node_child_kind(kind);
    for (const ChildRef& c : n.children) {
        require(c.kind == ck, "child kind must be the parent's child kind");
        require(std::isfinite(c.x) && std::isfinite(c.y), "child position must be finite");
        require(std::abs(c.x) <= 1.2f && std::abs(c.y) <= 1.2f, "child position must be in view bounds");
        require(!c.name.empty(), "child preview must carry a name");
    }

    // Descend into the first child of each level (one path to the leaf), and
    // verify the preview identity matches the full node identity.
    if (depth < 5) {
        const ChildRef& first = n.children.front();
        const ProcNode child = uni.node(first.seed, first.kind);
        require(child.name == first.name, "child preview name must match its full node name");
        require(color_eq(child.color, first.color), "child preview color must match its full node color");
        walk(uni, first.seed, first.kind, depth + 1, visited);
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

    // --- Structure + preview/full consistency over a full descent path --------
    {
        ProcUniverse uni(0x1234567ull);
        int visited = 0;
        walk(uni, uni.root_seed(), NodeKind::Universe, 0, visited);
        require(visited >= 6, "should visit at least one node per level (6 levels)");
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
