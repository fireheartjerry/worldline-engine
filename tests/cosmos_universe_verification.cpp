// Robustness of the generation pipeline: classification, end-to-end validation,
// cross-platform determinism (quantization), and a property/fuzz sweep over
// thousands of seeds asserting no universe is ever malformed.

#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/Nav3D.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/ObserverFleet.hpp"
#include "cosmos/Sandbox.hpp"
#include "cosmos/ScaleLadder.hpp"
#include "cosmos/Universe.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_universe_verification failed: " << message << '\n';
        std::exit(1);
    }
}

bool is_quantized(double v) {
    return std::abs(v * 1.0e6 - std::round(v * 1.0e6)) < 1.0e-6;
}

void test_classification() {
    const Universe a1 = generate_universe("alpha");
    const Universe a2 = generate_universe("alpha");
    require(a1.classification.codename == a2.classification.codename,
            "classification must be deterministic");
    require(!a1.classification.class_name.empty(), "class name must be set");
    require(a1.classification.codename.rfind("WL-", 0) == 0, "codename must be WL-prefixed");
    require(!a1.classification.traits.empty(), "classification must list traits");
    require(a1.classification.complexity >= 0.0 && a1.classification.complexity <= 1.0,
            "complexity must be in [0,1]");

    // Different universes should usually classify differently; require variety
    // across a small spread of seeds.
    int distinct_codenames = 0;
    std::string last;
    for (int i = 0; i < 8; ++i) {
        const Universe u = generate_universe("vary-" + std::to_string(i));
        if (u.classification.codename != last) ++distinct_codenames;
        last = u.classification.codename;
    }
    require(distinct_codenames >= 5, "classifications must vary across universes");
}

void test_validation_passes_and_catches_corruption() {
    Universe u = generate_universe("validate-seed");
    require(validate_universe(u).ok, "a freshly generated universe must validate");

    // Corrupt a generated field -> validation must catch it.
    Universe bad = u;
    bad.catalog[0].generated_mass = -1.0;
    const ValidationReport r1 = validate_universe(bad);
    require(!r1.ok && !r1.issues.empty(), "validation must reject a bad generated_mass");

    Universe bad2 = u;
    bad2.catalog[3].sim_mass = std::nan("");
    require(!validate_universe(bad2).ok, "validation must reject a non-finite sim field");

    Universe bad3 = u;
    bad3.classification.class_name.clear();
    require(!validate_universe(bad3).ok, "validation must reject an empty classification");

    Universe bad4 = u;
    bad4.catalog[1].constituents.push_back("not-a-real-object");
    require(!validate_universe(bad4).ok, "validation must reject an unresolved constituent");
}

void test_genome_quantization() {
    const LawGenome g = generate_law_genome(std::string("quantize-seed"));
    require(is_quantized(g.coupling_strong), "coupling_strong must be quantized");
    require(is_quantized(g.coupling_em), "coupling_em must be quantized");
    require(is_quantized(g.coupling_gravity), "coupling_gravity must be quantized");
    require(is_quantized(g.gravity_exponent), "gravity_exponent must be quantized");
    require(is_quantized(g.mass_scale), "mass_scale must be quantized");
    require(is_quantized(g.cosmological_drift), "cosmological_drift must be quantized");
    require(is_quantized(g.stability_bias), "stability_bias must be quantized");
}

void test_palette() {
    const Universe a = generate_universe("alpha");
    const Universe b = generate_universe("alpha");
    require(a.palette.primary_hue == b.palette.primary_hue, "palette must be deterministic");
    require(a.palette.star_density >= 0.0 && a.palette.star_density <= 1.0,
            "star density must be in [0,1]");
    const Color8 acc = a.palette.accent;
    require(static_cast<int>(acc.r) + acc.g + acc.b > 30, "accent color must be visible");

    int distinct = 0;
    double last = -1.0;
    for (int i = 0; i < 8; ++i) {
        const Universe u = generate_universe("pal-" + std::to_string(i));
        if (u.palette.primary_hue != last) ++distinct;
        last = u.palette.primary_hue;
    }
    require(distinct >= 5, "palettes must vary across universes");
}

// Property/fuzz sweep: no seed may ever produce a malformed universe.
void test_fuzz_generation() {
    const int kSeeds = 600;
    for (int i = 0; i < kSeeds; ++i) {
        const std::string seed = "fuzz-" + std::to_string(i * 2654435761u % 1000003u);
        const Universe u = generate_universe(seed);
        const ValidationReport r = validate_universe(u);
        if (!r.ok) {
            std::cerr << "seed '" << seed << "' produced issues:\n";
            for (const std::string& issue : r.issues) std::cerr << "  - " << issue << "\n";
            require(false, "every generated universe must validate");
        }
        // Deterministic regeneration (sampled; the seed machine is the heavy part).
        if (i % 8 == 0) {
            require(generate_law_genome(seed).signature == u.genome.signature,
                    "regeneration must be deterministic");
        }
    }
    std::cerr << "[fuzz] validated " << kSeeds << " universes\n";
}

// A handful of fuzzed universes must produce finite, bounded sandboxes on every
// tier (the generation feeding the simulation cannot blow it up).
void test_fuzz_sandbox_stability() {
    for (int i = 0; i < 3; ++i) {
        const Universe u = generate_universe("sbx-" + std::to_string(i));
        for (std::size_t s = 0; s < kScaleCount; ++s) {
            NBodySystem sys;
            populate_sandbox(sys, u.catalog, u.genome, static_cast<Scale>(s), 24);
            advance_sandbox(sys, 100);
            const SandboxStats st = sandbox_stats(sys);
            require(st.finite && st.rms_radius < 5.0e3,
                    std::string("fuzz sandbox must stay finite: ") + scale_ladder()[s].name);
        }
    }
}

void test_classification_granularity() {
    const Universe u = generate_universe("granular-seed");
    const UniverseClassification& c = u.classification;
    require(!c.sub_class.empty(), "sub-class must be set");
    require(!c.era.empty(), "era must be set");
    const UniverseObservation& o = c.observation;
    require(o.baryon_richness >= 0.0 && o.baryon_richness <= 1.0, "baryon richness bounds");
    require(o.structure_index >= 0.0 && o.structure_index <= 1.0, "structure index bounds");
    require(o.entropy_index >= 0.0 && o.entropy_index <= 1.0, "entropy index bounds");
    require(o.habitability_tier >= 0 && o.habitability_tier <= 4, "habitability bounds");

    // The finer metrics must vary across universes.
    int distinct = 0;
    double last = -1.0;
    for (int i = 0; i < 10; ++i) {
        const Universe v = generate_universe("g-" + std::to_string(i));
        if (v.classification.observation.structure_index != last) ++distinct;
        last = v.classification.observation.structure_index;
    }
    require(distinct >= 6, "observation metrics must vary across universes");
}

void test_observer_fleet() {
    const Universe u = generate_universe("fleet-seed");
    NBodySystem sys;
    populate_sandbox(sys, u.catalog, u.genome, Scale::STELLAR, 40);
    advance_sandbox(sys, 60);

    ObserverFleet fleet;
    fleet.deploy(20, 6.0, u.genome.signature);
    require(fleet.agents.size() == 20, "fleet must deploy the requested agents");
    for (int i = 0; i < 200; ++i) {
        fleet.update(sys, 1.0 / 60.0);
    }
    const FleetReport r = fleet.report();
    require(r.finite, "fleet must stay finite");
    require(r.coverage >= 0.0 && r.coverage <= 1.0, "coverage must be a fraction");
    require(r.species_seen >= 0, "species seen must be non-negative");
    require(r.mean_density >= 0.0, "mean density must be non-negative");
    // Probes must stay in view (soft confinement).
    for (const Observer& a : fleet.agents) {
        require(std::isfinite(a.pos.x) && std::abs(a.pos.x) < 50.0, "probes must stay bounded");
    }
}

void test_nav3d_camera() {
    using namespace nav3d;
    OrbitCamera cam;
    const Vec3 e0 = cam.eye();
    require(std::isfinite(e0.x) && std::isfinite(e0.y) && std::isfinite(e0.z), "eye must be finite");

    cam.zoom(0.5);
    require(cam.distance < 12.0 && cam.distance >= cam.min_distance, "zoom must move in and clamp");
    cam.zoom(1.0e9);
    require(cam.distance <= cam.max_distance, "zoom out must clamp to max");

    cam.orbit(0.0, 100.0); // huge pitch
    require(cam.pitch < 1.571 && cam.pitch > -1.571, "pitch must clamp away from poles");

    // Orthonormal-ish basis.
    const Vec3 f = cam.forward();
    require(std::abs(f.length() - 1.0) < 1.0e-6, "forward must be unit length");
    require(std::abs(f.dot(cam.right())) < 1.0e-6, "right must be perpendicular to forward");

    cam.frame(Vec3{0, 0, 0}, 20.0);
    require(cam.distance > 0.0 && std::isfinite(cam.distance), "frame must set a finite distance");
}

} // namespace

int main() {
    test_classification();
    test_classification_granularity();
    test_observer_fleet();
    test_nav3d_camera();
    test_validation_passes_and_catches_corruption();
    test_genome_quantization();
    test_palette();
    test_fuzz_generation();
    test_fuzz_sandbox_stability();
    return 0;
}
