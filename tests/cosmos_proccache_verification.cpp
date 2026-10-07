// ProcUniverse LRU cache: regenerate-identically under maximal thrash along a
// full-depth path (universe -> creature, with parent context), strict LRU
// eviction order, budget changes, the documented "parent may be a cache
// reference" contract (run under ASan to catch use-after-free), independence
// from the budget and from visit order, and the community memo-cache.

#include "cosmos/Ecosystem.hpp"
#include "cosmos/ProcUniverse.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_proccache_verification failed: " << message << '\n';
        ++g_failures;
    }
}

bool color_eq(const Color8& a, const Color8& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

// Every observable field, exactly (floats compared with ==: regeneration must
// be bit-identical, not merely close).
bool identical(const ProcNode& a, const ProcNode& b) {
    if (a.seed != b.seed || a.kind != b.kind || a.name != b.name || a.descriptor != b.descriptor) return false;
    if (!color_eq(a.color, b.color) || a.facts != b.facts) return false;
    if (a.subtype != b.subtype || a.luminosity != b.luminosity || a.orbit_au != b.orbit_au) return false;
    if (a.temperature_c != b.temperature_c || a.precip_mm != b.precip_mm || a.habitable != b.habitable) return false;
    if (a.phys_mass != b.phys_mass || a.phys_radius != b.phys_radius || a.phys_aux != b.phys_aux) return false;
    if (a.children.size() != b.children.size()) return false;
    for (std::size_t i = 0; i < a.children.size(); ++i) {
        const ChildRef& x = a.children[i];
        const ChildRef& y = b.children[i];
        if (x.seed != y.seed || x.kind != y.kind || x.name != y.name || x.subtype != y.subtype) return false;
        if (x.x != y.x || x.y != y.y || x.orbit != y.orbit || x.phase != y.phase || x.size != y.size) return false;
        if (!color_eq(x.color, y.color) || x.orbit_au != y.orbit_au || x.habitable != y.habitable) return false;
    }
    return true;
}

bool same_community(const eco::Community& a, const eco::Community& b) {
    if (a.species.size() != b.species.size() || a.links.size() != b.links.size()) return false;
    for (std::size_t i = 0; i < a.species.size(); ++i) {
        const eco::Species& x = a.species[i];
        const eco::Species& y = b.species[i];
        if (x.name != y.name || x.t.tau != y.t.tau || x.t.m != y.t.m || x.xeq != y.xeq || x.flux != y.flux)
            return false;
    }
    for (std::size_t i = 0; i < a.links.size(); ++i) {
        if (a.links[i].pred != b.links[i].pred || a.links[i].prey != b.links[i].prey ||
            a.links[i].pref != b.links[i].pref)
            return false;
    }
    return true;
}

// A full-depth path: universe, galaxy, system, a living planet, ecosystem,
// creature -- each generated through its parent (copies, so the reference
// survives any later eviction).
struct Path {
    std::vector<ProcNode> nodes; // [0] universe ... [5] creature
};

bool find_living_path(ProcUniverse& uni, Path& out) {
    const ProcNode u = uni.root();
    for (const ChildRef& g : u.children) {
        const ProcNode gal = uni.node(g.seed, g.kind, &u);
        for (const ChildRef& s : gal.children) {
            const ProcNode sys = uni.node(s.seed, s.kind, &gal);
            for (const ChildRef& p : sys.children) {
                const ProcNode pl = uni.node(p.seed, p.kind, &sys);
                if (!pl.habitable || pl.children.empty()) continue;
                const ProcNode eco = uni.node(pl.children.back().seed, pl.children.back().kind, &pl);
                if (eco.children.empty()) continue;
                const ChildRef& c = eco.children[eco.children.size() / 2];
                const ProcNode cr = uni.node(c.seed, c.kind, &eco);
                out.nodes = {u, gal, sys, pl, eco, cr};
                return true;
            }
        }
    }
    return false;
}

// Re-walk a recorded path through `uni`, each level from its (re-fetched)
// parent, and compare against the reference.
bool rewalk_matches(ProcUniverse& uni, const Path& ref) {
    ProcNode parent = uni.root();
    if (!identical(parent, ref.nodes[0])) return false;
    for (std::size_t d = 1; d < ref.nodes.size(); ++d) {
        const ProcNode n = uni.node(ref.nodes[d].seed, ref.nodes[d].kind, &parent);
        if (!identical(n, ref.nodes[d])) return false;
        parent = n;
    }
    return true;
}

void test_thrash_regenerates_identically() {
    ProcUniverse big(0x5EEDull);
    Path ref;
    const bool found = find_living_path(big, ref);
    check(found, "a living world must exist within the first galaxies");
    if (!found) return;
    check(ref.nodes[5].kind == NodeKind::Creature, "path must reach a creature");

    for (std::size_t budget : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{7}}) {
        ProcUniverse small(0x5EEDull);
        small.set_budget(budget);
        for (int round = 0; round < 3; ++round) {
            check(rewalk_matches(small, ref),
                  "path must regenerate bit-identically with budget " + std::to_string(budget) + " (round " +
                      std::to_string(round) + ")");
            check(small.cache_size() <= small.budget(), "cache must respect its budget");
            // Interleave unrelated traffic so the path is fully evicted.
            const ProcNode u = small.root();
            for (const ChildRef& g : u.children) small.node(g.seed, g.kind, &u);
        }
    }

    // Visiting order does not matter: deepest-first after an unrelated walk.
    ProcUniverse other(0x5EEDull);
    other.set_budget(4);
    const ProcNode u = other.root();
    for (std::size_t i = u.children.size(); i-- > 0;) other.node(u.children[i].seed, u.children[i].kind, &u);
    check(rewalk_matches(other, ref), "visit order must not change generated content");
}

void test_parent_may_be_a_cache_reference() {
    // The documented contract: a reference returned by node() may be passed as
    // `parent` even if generating the child evicts it (budget 1 guarantees it).
    ProcUniverse uni(0x5EEDull);
    uni.set_budget(1);
    Path ref;
    ProcUniverse big(0x5EEDull);
    if (!find_living_path(big, ref)) return;
    const ProcNode* parent = &uni.root();
    for (std::size_t d = 1; d < ref.nodes.size(); ++d) {
        const ProcNode& n = uni.node(ref.nodes[d].seed, ref.nodes[d].kind, parent);
        check(identical(n, ref.nodes[d]), "child generated from a cache-reference parent must match");
        check(!uni.is_cached(ref.nodes[d - 1].seed), "budget 1 evicts the parent");
        parent = &n; // still valid: nothing has been generated since
    }
}

void test_lru_eviction_order() {
    ProcUniverse uni(42ull);
    const ProcNode u = uni.root();
    check(u.children.size() >= 5, "universe needs at least five galaxies for the LRU test");
    if (u.children.size() < 5) return;
    uni.set_budget(3);
    check(uni.cache_size() == 1 && uni.is_cached(u.seed), "set_budget keeps the most recent node");

    const std::uint64_t a = u.children[0].seed, b = u.children[1].seed, c = u.children[2].seed;
    const std::uint64_t d = u.children[3].seed, e = u.children[4].seed;
    uni.node(a, NodeKind::Galaxy, &u);
    uni.node(b, NodeKind::Galaxy, &u);
    uni.node(c, NodeKind::Galaxy, &u); // root evicted (LRU)
    check(!uni.is_cached(u.seed) && uni.is_cached(a) && uni.is_cached(b) && uni.is_cached(c),
          "inserting past the budget evicts the least recently used");
    uni.node(a, NodeKind::Galaxy, &u); // touch a -> b is now the LRU
    uni.node(d, NodeKind::Galaxy, &u);
    check(uni.is_cached(a) && !uni.is_cached(b) && uni.is_cached(c) && uni.is_cached(d),
          "a cache hit must refresh recency (b evicted, not a)");
    check(uni.cache_size() == 3, "cache holds exactly the budget");

    uni.set_budget(2); // shrink trims immediately, keeping the two most recent
    check(uni.cache_size() == 2 && uni.is_cached(a) && uni.is_cached(d) && !uni.is_cached(c),
          "shrinking the budget trims the least recently used first");
    uni.set_budget(0);
    check(uni.budget() == 1 && uni.cache_size() == 1 && uni.is_cached(d), "budget 0 clamps to 1");

    uni.set_budget(64);
    uni.node(e, NodeKind::Galaxy, &u);
    check(uni.cache_size() == 2, "growing the budget keeps existing entries");
    uni.reseed(43ull);
    check(uni.cache_size() == 0 && !uni.is_cached(e), "reseed clears the cache");
    check(uni.root_seed() == 43ull && uni.root().seed == 43ull, "reseed changes the root");
    uni.reseed(0ull);
    check(uni.root_seed() != 0ull, "seed 0 is remapped to a valid root seed");
}

void test_budget_independence() {
    // The same exploration with different budgets yields identical nodes.
    ProcUniverse a(777ull), b(777ull);
    a.set_budget(1);
    b.set_budget(100000);
    const ProcNode ua = a.root(), ub = b.root();
    check(identical(ua, ub), "roots identical across budgets");
    int compared = 0;
    for (std::size_t gi = 0; gi < ua.children.size() && gi < 3; ++gi) {
        const ProcNode ga = a.node(ua.children[gi].seed, ua.children[gi].kind, &ua);
        const ProcNode gb = b.node(ub.children[gi].seed, ub.children[gi].kind, &ub);
        check(identical(ga, gb), "galaxies identical across budgets");
        for (std::size_t si = 0; si < ga.children.size() && si < 6; ++si) {
            const ProcNode sa = a.node(ga.children[si].seed, ga.children[si].kind, &ga);
            const ProcNode sb = b.node(gb.children[si].seed, gb.children[si].kind, &gb);
            check(identical(sa, sb), "systems identical across budgets");
            for (const ChildRef& p : sa.children) {
                const ProcNode pa = a.node(p.seed, p.kind, &sa);
                const ProcNode pb = b.node(p.seed, p.kind, &sb);
                check(identical(pa, pb), "planets identical across budgets");
                ++compared;
            }
        }
    }
    check(compared > 10, "budget-independence sample must include many planets");
}

void test_community_memo_cache() {
    // Collect more ecosystems than the memo-cache holds (64) so it evicts, then
    // verify every memoized community equals a fresh, uncached build -- both
    // before and after eviction.
    ProcUniverse uni(0xC0FFEEull);
    std::vector<ProcNode> ecosystems;
    const ProcNode u = uni.root();
    for (const ChildRef& g : u.children) {
        const ProcNode gal = uni.node(g.seed, g.kind, &u);
        for (const ChildRef& s : gal.children) {
            const ProcNode sys = uni.node(s.seed, s.kind, &gal);
            for (const ChildRef& p : sys.children) {
                const ProcNode pl = uni.node(p.seed, p.kind, &sys);
                for (const ChildRef& e : pl.children) ecosystems.push_back(uni.node(e.seed, e.kind, &pl));
            }
            if (ecosystems.size() > 80) break;
        }
        if (ecosystems.size() > 80) break;
    }
    check(ecosystems.size() > 64, "need more ecosystems than the community cache budget (got " +
                                      std::to_string(ecosystems.size()) + ")");
    for (int pass = 0; pass < 2; ++pass) {
        for (const ProcNode& e : ecosystems) {
            const eco::Community& memo = uni.community_cached(e);
            const eco::Community fresh = community_for_ecosystem(e);
            if (!same_community(memo, fresh)) {
                check(false, "memoized community must equal a fresh build (pass " + std::to_string(pass) + ")");
                return;
            }
            check(memo.species.size() == e.children.size(), "ecosystem children mirror its species");
        }
    }
}

} // namespace

int main() {
    test_thrash_regenerates_identically();
    test_parent_may_be_a_cache_reference();
    test_lru_eviction_order();
    test_budget_independence();
    test_community_memo_cache();
    if (g_failures > 0) {
        std::cerr << g_failures << " procedural cache check(s) failed\n";
        return EXIT_FAILURE;
    }
    return 0;
}
