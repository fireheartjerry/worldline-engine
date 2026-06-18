#include "cosmos/Analysis.hpp"
#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/Sandbox.hpp"
#include "cosmos/ScaleLadder.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_verification failed: " << message << '\n';
        std::exit(1);
    }
}

void test_genome_determinism_and_bounds() {
    const LawGenome a1 = generate_law_genome(std::string("alpha-universe"));
    const LawGenome a2 = generate_law_genome(std::string("alpha-universe"));
    const LawGenome b = generate_law_genome(std::string("omega-universe"));

    require(a1.signature == a2.signature, "genome must be deterministic for a seed");
    require(a1.coupling_strong == a2.coupling_strong, "genome fields must be deterministic");
    require(a1.signature != b.signature, "different seeds should yield different genomes");

    require(a1.coupling_strong >= 0.35 && a1.coupling_strong <= 1.80, "coupling_strong bounds");
    require(a1.coupling_em >= 0.35 && a1.coupling_em <= 1.80, "coupling_em bounds");
    require(a1.coupling_gravity >= 0.40 && a1.coupling_gravity <= 1.70, "coupling_gravity bounds");
    require(a1.gravity_exponent >= 1.75 && a1.gravity_exponent <= 2.25, "gravity_exponent bounds");
    require(a1.mass_scale >= 0.60 && a1.mass_scale <= 1.60, "mass_scale bounds");
    require(a1.stability_bias >= 0.15 && a1.stability_bias <= 0.90, "stability_bias bounds");
}

void test_catalog_invariants() {
    std::vector<UniverseObject> catalog = build_object_catalog();
    require(catalog.size() > 40, "catalog should be substantial");

    // Every tier must have at least one object.
    for (std::size_t s = 0; s < kScaleCount; ++s) {
        const Scale scale = static_cast<Scale>(s);
        require(!objects_for_scale(catalog, scale).empty(),
                std::string("tier ") + scale_ladder()[s].name + " must have objects");
    }

    // Constituent references must resolve to real catalog entries.
    for (const UniverseObject& o : catalog) {
        for (const std::string& cid : o.constituents) {
            require(find_object(catalog, cid) != nullptr,
                    "constituent '" + cid + "' of '" + o.id + "' must exist");
        }
    }

    const LawGenome genome = generate_law_genome(std::string("catalog-seed"));
    std::vector<UniverseObject> a = catalog;
    std::vector<UniverseObject> b = catalog;
    apply_law_genome(a, genome);
    apply_law_genome(b, genome);

    for (std::size_t i = 0; i < a.size(); ++i) {
        require(a[i].sim_mass == b[i].sim_mass, "apply_law_genome must be deterministic");
        require(a[i].sim_mass >= 0.8 && a[i].sim_mass <= 5.0, "sim_mass conditioning bounds");
        require(a[i].sim_radius >= 0.3 && a[i].sim_radius <= 1.1, "sim_radius conditioning bounds");
        require(a[i].stability >= 0.0 && a[i].stability <= 1.0, "stability in [0,1]");
        require(a[i].abundance >= 0.0 && a[i].abundance <= 1.0, "abundance in [0,1]");
        require(a[i].generated_mass > 0.0, "generated_mass must be positive");
    }
}

NBodySystem make_two_body_orbit(double G) {
    NBodySystem sys;
    sys.params = ForceParams{};
    sys.params.gravity = G;
    sys.params.charge = 0.0;
    sys.params.strong = 0.0;
    sys.params.core = 0.0;
    sys.params.exponent = 2.0;
    sys.params.softening = 0.01;
    sys.params.accel_cap = 0.0; // disabled, keep it conservative
    sys.params.damping = 0.0;

    // Two equal masses, separation 4, circular-orbit tangential velocities.
    const double v = std::sqrt(G / 8.0);
    Body a; a.mass = 1.0; a.pos = {-2.0, 0.0}; a.vel = {0.0, v};
    Body b; b.mass = 1.0; b.pos = {2.0, 0.0}; b.vel = {0.0, -v};
    sys.bodies = {a, b};
    return sys;
}

void test_energy_conservation() {
    NBodySystem sys = make_two_body_orbit(1.6);
    const double e0 = sys.total_energy();
    double emin = e0;
    double emax = e0;
    for (int i = 0; i < 4000; ++i) {
        sys.step(0.01, 8);
        const double e = sys.total_energy();
        emin = std::min(emin, e);
        emax = std::max(emax, e);
    }
    const double drift = (emax - emin) / std::abs(e0);
    require(drift < 1.0e-3, "symplectic integrator must conserve energy (drift " +
                                std::to_string(drift) + ")");
}

void test_momentum_conservation_and_determinism() {
    // Build a small cluster from a real tier and run the full force law.
    std::vector<UniverseObject> catalog = build_object_catalog();
    const LawGenome genome = generate_law_genome(std::string("cluster-seed"));
    apply_law_genome(catalog, genome);
    const ScaleTier& tier = tier_for(Scale::STELLAR);

    NBodySystem sys;
    sys.params = make_force_params(tier, genome);
    sys.params.accel_cap = 0.0;    // disable cap so pairwise forces stay equal-and-opposite
    sys.params.confinement = 0.0;  // external trap would break momentum conservation
    sys.params.damping = 0.0;      // damping would bleed momentum
    const auto stars = objects_for_scale(catalog, Scale::STELLAR);
    for (int i = 0; i < 6; ++i) {
        const UniverseObject& o = *stars[static_cast<std::size_t>(i % stars.size())];
        Body body;
        body.mass = o.sim_mass;
        body.radius = o.sim_radius;
        body.charge = o.sim_charge;
        const double angle = 2.0 * 3.14159265358979 * i / 6.0;
        body.pos = {3.0 * std::cos(angle), 3.0 * std::sin(angle)};
        body.vel = {0.0, 0.0};
        sys.bodies.push_back(body);
    }
    // Start with zero net momentum (all velocities zero already).
    NBodySystem copy = sys;

    const Vec2 p0 = sys.total_momentum();
    for (int i = 0; i < 600; ++i) {
        sys.step(0.01, 4);
    }
    const Vec2 p1 = sys.total_momentum();
    const double dp = (p1 - p0).length();
    std::cerr << "[info] momentum drift = " << dp << "\n";
    // Equal-and-opposite pairwise forces conserve momentum analytically; the
    // residual is pure floating-point accumulation over thousands of substeps.
    require(dp < 1.0e-6, "total momentum must be conserved (drift " + std::to_string(dp) + ")");

    // Determinism: an identical system steps to a bit-identical state.
    for (int i = 0; i < 600; ++i) {
        copy.step(0.01, 4);
    }
    for (std::size_t i = 0; i < sys.bodies.size(); ++i) {
        require(sys.bodies[i].pos.x == copy.bodies[i].pos.x &&
                    sys.bodies[i].pos.y == copy.bodies[i].pos.y &&
                    sys.bodies[i].vel.x == copy.bodies[i].vel.x &&
                    sys.bodies[i].vel.y == copy.bodies[i].vel.y,
                "N-body stepping must be deterministic");
    }
}

void test_all_tiers_stable() {
    // Every tier, across several universes, must stay finite and bounded after a
    // long run. Diagnostics are printed so the force law can be tuned per tier.
    // Kept light for CI (Debug, O(N^2)); the live app runs more bodies.
    const char* seeds[] = {"alpha", "vega-9"};
    const int kTestBodies = 40;
    for (std::size_t s = 0; s < kScaleCount; ++s) {
        const Scale scale = static_cast<Scale>(s);
        for (const char* seed : seeds) {
            std::vector<UniverseObject> catalog = build_object_catalog();
            const LawGenome genome = generate_law_genome(std::string(seed));
            apply_law_genome(catalog, genome);

            NBodySystem sys;
            populate_sandbox(sys, catalog, genome, scale, kTestBodies);
            require(static_cast<int>(sys.bodies.size()) == kTestBodies,
                    "sandbox must populate the requested body count");
            advance_sandbox(sys, 450);

            const SandboxStats st = sandbox_stats(sys);
            std::cerr << "[tier] " << scale_ladder()[s].name << " seed=" << seed
                      << " rms=" << st.rms_radius << " E=" << st.energy
                      << " virial=" << st.virial << " bound=" << st.bound_pairs << "\n";
            require(st.finite, std::string("tier ") + scale_ladder()[s].name +
                                   " must stay finite for seed " + seed);
            require(st.rms_radius < 5.0e3,
                    std::string("tier ") + scale_ladder()[s].name + " must not explode");
        }
    }
}

void test_sandbox_determinism_and_reproduction() {
    std::vector<UniverseObject> catalog = build_object_catalog();
    const LawGenome genome = generate_law_genome(std::string("repro-seed"));
    apply_law_genome(catalog, genome);

    // Two fresh populations advanced the same number of steps must match exactly.
    NBodySystem a;
    NBodySystem b;
    populate_sandbox(a, catalog, genome, Scale::STELLAR);
    populate_sandbox(b, catalog, genome, Scale::STELLAR);
    advance_sandbox(a, 500);
    advance_sandbox(b, 500);
    require(a.bodies.size() == b.bodies.size(), "repro: same body count");
    for (std::size_t i = 0; i < a.bodies.size(); ++i) {
        require(a.bodies[i].pos.x == b.bodies[i].pos.x &&
                    a.bodies[i].pos.y == b.bodies[i].pos.y,
                "sandbox reproduction must be bit-identical");
    }

    // Advancing N in one call equals N single-step calls (the save/reopen path).
    NBodySystem c;
    populate_sandbox(c, catalog, genome, Scale::STELLAR);
    for (int i = 0; i < 500; ++i) {
        advance_sandbox(c, 1);
    }
    for (std::size_t i = 0; i < a.bodies.size(); ++i) {
        require(a.bodies[i].pos.x == c.bodies[i].pos.x &&
                    a.bodies[i].pos.y == c.bodies[i].pos.y,
                "step batching must not change the trajectory");
    }
}

void test_analysis() {
    std::vector<UniverseObject> catalog = build_object_catalog();
    const LawGenome genome = generate_law_genome(std::string("analysis-seed"));
    apply_law_genome(catalog, genome);

    const UniverseObject* water = find_object(catalog, "water");
    require(water != nullptr, "water must exist");
    const auto parts = resolve_constituents(catalog, *water);
    require(parts.size() == 3, "water has three constituents");
    int oxygens = 0;
    for (const auto& p : parts) {
        require(p.object != nullptr, "water constituents must resolve");
        if (p.id == "oxygen") {
            ++oxygens;
        }
    }
    require(oxygens == 1, "water has one oxygen");

    const UniverseObject* earth = find_object(catalog, "earth");
    const UniverseObject* moon = find_object(catalog, "moon");
    require(earth != nullptr && moon != nullptr, "earth and moon must exist");
    const ObjectComparison cmp = compare_objects(*earth, *moon);
    require(cmp.mass_ratio > 1.0, "earth is heavier than the moon");
    require(cmp.mass_orders >= 1, "earth outmasses the moon by an order of magnitude");
}

} // namespace

int main() {
    test_genome_determinism_and_bounds();
    test_catalog_invariants();
    test_energy_conservation();
    test_momentum_conservation_and_determinism();
    test_all_tiers_stable();
    test_sandbox_determinism_and_reproduction();
    test_analysis();
    return 0;
}
