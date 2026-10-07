// NBodySystem::step() caches per-pair force constants and reuses the end-of-
// substep acceleration as the next start-of-substep acceleration. This test
// pins that optimization to the ORIGINAL algorithm bit for bit: a reference
// stepper below re-evaluates the original single-pass pair force twice per
// substep, from scratch, and every position/velocity must match exactly --
// across all tiers, substep counts, copies, and every kind of edit between
// steps that has to invalidate the cache (positions, masses, charges, radii,
// params, adding/removing/replacing bodies, -0.0 vs +0.0).

#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/Sandbox.hpp"
#include "cosmos/ScaleLadder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_nbody_cache_verification failed: " << message << '\n';
        ++g_failures;
    }
}

// --- Reference: the original force law and integrator, verbatim. -----------

double ref_pair_force_coeff(const Body& bi, const Body& bj, const ForceParams& p, double d2) {
    const double soft2 = d2 + p.softening * p.softening;
    const double d = std::sqrt(std::max(d2, 1.0e-18));
    double coeff = p.gravity * bi.mass * bj.mass * std::pow(soft2, -(p.exponent + 1.0) * 0.5);
    if (p.linear != 0.0) {
        coeff += p.linear;
    }
    if (p.charge != 0.0) {
        coeff += -p.charge * bi.charge * bj.charge * std::pow(soft2, -1.5);
    }
    if (p.core != 0.0) {
        const double sigma = bi.radius + bj.radius;
        coeff += p.core * p.core_power * std::pow(sigma, p.core_power) *
                 std::pow(soft2, -(p.core_power + 2.0) * 0.5);
    }
    if (p.strong != 0.0) {
        const double sigma = bi.radius + bj.radius;
        const double r0 = p.bond_range * sigma;
        const double w = std::max(p.bond_width * sigma, 1.0e-6);
        const double delta = d - r0;
        const double well = std::exp(-(delta * delta) / (2.0 * w * w));
        const double force_d = -p.strong * (delta / (w * w)) * well;
        coeff += force_d / d;
    }
    return coeff;
}

std::vector<Vec2> ref_accelerations(const NBodySystem& sys) {
    const std::size_t n = sys.bodies.size();
    std::vector<Vec2> accel(n, Vec2{});
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const Body& bi = sys.bodies[i];
            const Body& bj = sys.bodies[j];
            const Vec2 r = bj.pos - bi.pos;
            const double coeff = ref_pair_force_coeff(bi, bj, sys.params, r.length_sq());
            const Vec2 force = r * coeff;
            accel[i] += force / bi.mass;
            accel[j] -= force / bj.mass;
        }
    }
    if (sys.params.confinement > 0.0) {
        for (std::size_t i = 0; i < n; ++i) accel[i] -= sys.bodies[i].pos * sys.params.confinement;
    }
    if (sys.params.swirl != 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            const Vec2& q = sys.bodies[i].pos;
            accel[i] += Vec2{-q.y, q.x} * sys.params.swirl;
        }
    }
    if (sys.params.accel_cap > 0.0) {
        for (Vec2& a : accel) {
            const double mag = a.length();
            if (mag > sys.params.accel_cap) a = a * (sys.params.accel_cap / mag);
        }
    }
    return accel;
}

void ref_step(NBodySystem& sys, double dt, int substeps) {
    if (sys.bodies.empty() || dt <= 0.0) return;
    const int steps = std::max(1, substeps);
    const double h = dt / steps;
    const double half = h * 0.5;
    for (int s = 0; s < steps; ++s) {
        const std::vector<Vec2> a0 = ref_accelerations(sys);
        for (std::size_t i = 0; i < sys.bodies.size(); ++i) {
            sys.bodies[i].vel += a0[i] * half;
            sys.bodies[i].pos += sys.bodies[i].vel * h;
        }
        const std::vector<Vec2> a1 = ref_accelerations(sys);
        for (std::size_t i = 0; i < sys.bodies.size(); ++i) sys.bodies[i].vel += a1[i] * half;
        if (sys.params.damping > 0.0) {
            const double factor = std::max(0.0, 1.0 - sys.params.damping * h);
            for (Body& b : sys.bodies) b.vel = b.vel * factor;
        }
    }
}

// --- Helpers -------------------------------------------------------------

bool bits_equal(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

bool same_state(const NBodySystem& a, const NBodySystem& b) {
    if (a.bodies.size() != b.bodies.size()) return false;
    for (std::size_t i = 0; i < a.bodies.size(); ++i) {
        const Body& x = a.bodies[i];
        const Body& y = b.bodies[i];
        if (!bits_equal(x.pos.x, y.pos.x) || !bits_equal(x.pos.y, y.pos.y) || !bits_equal(x.vel.x, y.vel.x) ||
            !bits_equal(x.vel.y, y.vel.y))
            return false;
    }
    return true;
}

NBodySystem make_sandbox(const std::string& seed, Scale scale, int bodies) {
    std::vector<UniverseObject> catalog = build_object_catalog();
    const LawGenome genome = generate_law_genome(seed);
    apply_law_genome(catalog, genome);
    NBodySystem sys;
    populate_sandbox(sys, catalog, genome, scale, bodies);
    return sys;
}

// Step both systems in lockstep and require bit-identical states.
bool lockstep(NBodySystem& fast, NBodySystem& ref, int steps, double dt, int substeps, const std::string& what) {
    for (int k = 0; k < steps; ++k) {
        fast.step(dt, substeps);
        ref_step(ref, dt, substeps);
        if (!same_state(fast, ref)) {
            check(false, what + ": diverged from the reference at step " + std::to_string(k));
            return false;
        }
    }
    return true;
}

// --- Tests ---------------------------------------------------------------

void test_all_tiers_match_reference() {
    for (const char* seed : {"alpha", "vega-9", "helix"}) {
        for (std::size_t s = 0; s < kScaleCount; ++s) {
            const Scale scale = static_cast<Scale>(s);
            NBodySystem fast = make_sandbox(seed, scale, 24);
            NBodySystem ref = fast;
            const std::string what = std::string(seed) + " tier " + std::to_string(s);
            lockstep(fast, ref, 40, kSandboxDt, kSandboxSubsteps, what);
            const std::vector<Vec2> a = fast.accelerations();
            const std::vector<Vec2> b = ref_accelerations(ref);
            bool same = a.size() == b.size();
            for (std::size_t i = 0; same && i < a.size(); ++i) {
                same = bits_equal(a[i].x, b[i].x) && bits_equal(a[i].y, b[i].y);
            }
            check(same, what + ": accelerations() must equal the reference force law");
        }
    }
}

void test_substeps_and_dt() {
    for (int substeps : {-3, 0, 1, 2, 4, 7}) {
        for (double dt : {1.0 / 30.0, 1.0 / 120.0, 0.0, -0.01}) {
            NBodySystem fast = make_sandbox("helix", Scale::MOLECULAR, 20);
            NBodySystem ref = fast;
            lockstep(fast, ref, 15, dt, substeps,
                     "substeps " + std::to_string(substeps) + " dt " + std::to_string(dt));
        }
    }
}

void test_edits_between_steps_invalidate() {
    struct Edit {
        const char* name;
        void (*apply)(NBodySystem&);
    };
    const Edit edits[] = {
        {"position", [](NBodySystem& s) { s.bodies[3].pos.x += 0.25; }},
        {"negative zero position", [](NBodySystem& s) { s.bodies[2].pos = {-0.0, 0.0}; s.bodies[4].pos = {0.0, -0.0}; }},
        {"velocity only", [](NBodySystem& s) { for (Body& b : s.bodies) b.vel = b.vel * 3.0; }},
        {"mass", [](NBodySystem& s) { s.bodies[5].mass *= 1.5; }},
        {"charge", [](NBodySystem& s) { s.bodies[1].charge = -s.bodies[1].charge + 0.5; }},
        {"radius", [](NBodySystem& s) { s.bodies[0].radius *= 0.8; }},
        {"damping", [](NBodySystem& s) { s.params.damping = 0.5; }},
        {"core power", [](NBodySystem& s) { s.params.core_power = 6.0; s.params.core = 0.9; }},
        {"exponent + softening", [](NBodySystem& s) { s.params.exponent = 1.7; s.params.softening = 0.2; }},
        {"strong + charge on", [](NBodySystem& s) { s.params.strong = 1.3; s.params.charge = 0.8; }},
        {"swirl + linear", [](NBodySystem& s) { s.params.swirl = 0.3; s.params.linear = 0.2; }},
        {"add body", [](NBodySystem& s) { Body b = s.bodies[0]; b.pos = {1.5, -2.0}; s.bodies.push_back(b); }},
        {"remove body", [](NBodySystem& s) { s.bodies.erase(s.bodies.begin() + 2); }},
        {"swap bodies", [](NBodySystem& s) { std::swap(s.bodies[0], s.bodies[6]); }},
        {"replace all", [](NBodySystem& s) { s = make_sandbox("vega-9", Scale::NUCLEAR, 18); }},
        {"clear", [](NBodySystem& s) { s.bodies.clear(); }},
    };
    for (const Edit& e : edits) {
        NBodySystem fast = make_sandbox("alpha", Scale::ATOMIC, 22);
        NBodySystem ref = fast;
        const std::string what = std::string("edit '") + e.name + "'";
        if (!lockstep(fast, ref, 6, kSandboxDt, kSandboxSubsteps, what + " (before)")) continue;
        e.apply(fast);
        e.apply(ref);
        if (!lockstep(fast, ref, 8, kSandboxDt, kSandboxSubsteps, what + " (after)")) continue;
        // Repopulating after an edit (what the app does on a tier switch).
        NBodySystem again = make_sandbox("alpha", Scale::GALACTIC, 16);
        fast.bodies = again.bodies;
        fast.params = again.params;
        ref.bodies = again.bodies;
        ref.params = again.params;
        lockstep(fast, ref, 5, kSandboxDt, kSandboxSubsteps, what + " (repopulated)");
    }
}

void test_copies_carry_a_valid_cache() {
    NBodySystem fast = make_sandbox("vega-9", Scale::STELLAR, 26);
    NBodySystem ref = fast;
    lockstep(fast, ref, 10, kSandboxDt, kSandboxSubsteps, "copy (warm-up)");
    NBodySystem fast_copy = fast; // carries the cache
    NBodySystem ref_copy = ref;
    lockstep(fast_copy, ref_copy, 10, kSandboxDt, kSandboxSubsteps, "copy (copied system)");
    lockstep(fast, ref, 10, kSandboxDt, kSandboxSubsteps, "copy (original keeps going)");
    // Assigning a different system over a warm one must not reuse its cache.
    NBodySystem other = make_sandbox("helix", Scale::STELLAR, 26);
    NBodySystem other_ref = other;
    fast = other;
    lockstep(fast, other_ref, 10, kSandboxDt, kSandboxSubsteps, "copy (assigned over a warm system)");
}

void test_single_body_and_empty() {
    NBodySystem fast = make_sandbox("alpha", Scale::PLANETARY, 2);
    fast.bodies.resize(1);
    NBodySystem ref = fast;
    lockstep(fast, ref, 10, kSandboxDt, kSandboxSubsteps, "single body");
    NBodySystem empty;
    empty.step(kSandboxDt, 4); // no-op
    check(empty.bodies.empty() && empty.accelerations().empty(), "empty system is a no-op");
}

} // namespace

int main() {
    test_all_tiers_match_reference();
    test_substeps_and_dt();
    test_edits_between_steps_invalidate();
    test_copies_carry_a_valid_cache();
    test_single_body_and_empty();
    if (g_failures > 0) {
        std::cerr << g_failures << " n-body cache check(s) failed\n";
        return EXIT_FAILURE;
    }
    return 0;
}
