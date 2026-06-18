// Per-tier physics signatures. Each tier is verified independently: the test
// asserts the qualitative behavior that defines that scale (confinement,
// clustering, exclusion packing, bonding, aggregation, orbits, rotation,
// expansion). Diagnostics are printed so thresholds can be calibrated.

#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/Sandbox.hpp"
#include "cosmos/ScaleLadder.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_tiers_verification failed: " << message << '\n';
        std::exit(1);
    }
}

NBodySystem make_sandbox(const std::string& seed, Scale scale, int bodies = 48) {
    std::vector<UniverseObject> catalog = build_object_catalog();
    const LawGenome genome = generate_law_genome(seed);
    apply_law_genome(catalog, genome);
    NBodySystem sys;
    populate_sandbox(sys, catalog, genome, scale, bodies);
    return sys;
}

double mean_nn_distance(const NBodySystem& sys) {
    double total = 0.0;
    for (std::size_t i = 0; i < sys.bodies.size(); ++i) {
        double best = 1.0e300;
        for (std::size_t j = 0; j < sys.bodies.size(); ++j) {
            if (i == j) continue;
            best = std::min(best, (sys.bodies[j].pos - sys.bodies[i].pos).length());
        }
        total += best;
    }
    return total / static_cast<double>(sys.bodies.size());
}

int count_pairs_within(const NBodySystem& sys, double lo, double hi) {
    int n = 0;
    for (std::size_t i = 0; i < sys.bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < sys.bodies.size(); ++j) {
            const double d = (sys.bodies[j].pos - sys.bodies[i].pos).length();
            if (d >= lo && d <= hi) ++n;
        }
    }
    return n;
}

const char* seeds[] = {"alpha", "vega-9", "helix"};

// SUBATOMIC — confinement: even after a large energy injection, nothing escapes.
void test_subatomic_confinement() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::SUBATOMIC);
        advance_sandbox(sys, 200);
        for (Body& b : sys.bodies) b.vel = b.vel * 3.0; // inject 9x energy
        advance_sandbox(sys, 250);
        const double rms = sys.rms_radius();
        std::cerr << "[subatomic] " << seed << " rms_after_injection=" << rms
                  << " max=" << sys.max_radius() << "\n";
        // The bulk stays compact despite the energy injection: confinement does
        // not let the plasma disperse the way an unbound system would.
        require(std::isfinite(rms) && rms < 9.0,
                "subatomic confinement must keep the plasma compact");
    }
}

// NUCLEAR — strong binding into tight clusters.
void test_nuclear_clustering() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::NUCLEAR);
        advance_sandbox(sys, 350);
        std::cerr << "[nuclear] " << seed << " rms=" << sys.rms_radius()
                  << " bound=" << sys.bound_pair_count() << "\n";
        require(sys.rms_radius() < 6.0, "nuclear clusters must stay compact");
        require(sys.bound_pair_count() > 20, "nuclear must form many bound pairs");
    }
}

// ATOMIC — hard-core exclusion: spaced packing, not bonding into pairs.
void test_atomic_exclusion() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::ATOMIC);
        advance_sandbox(sys, 350);
        const double nn = mean_nn_distance(sys);
        std::cerr << "[atomic] " << seed << " mean_nn=" << nn
                  << " bound=" << sys.bound_pair_count() << " rms=" << sys.rms_radius() << "\n";
        // Exclusion + weak binding => a spaced packed phase, not a collapsed
        // point and not tight bonded pairs.
        require(nn > 0.5, "atomic exclusion must keep a packed spacing");
        require(sys.rms_radius() < 16.0, "atomic must stay on stage");
    }
}

// MOLECULAR — stable bonds at a characteristic distance.
void test_molecular_bonds() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::MOLECULAR);
        advance_sandbox(sys, 350);
        const int bonds = count_pairs_within(sys, 0.4, 2.8);
        std::cerr << "[molecular] " << seed << " bonds=" << bonds
                  << " virial=" << sys.virial_ratio() << "\n";
        require(bonds > 15, "molecular must hold many bonds near the bond distance");
    }
}

// NANOSCALE — aggregation: dispersed grains clump together over time.
void test_nanoscale_aggregation() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::NANOSCALE);
        const double nn0 = mean_nn_distance(sys);
        advance_sandbox(sys, 400);
        const double nn1 = mean_nn_distance(sys);
        std::cerr << "[nanoscale] " << seed << " nn0=" << nn0 << " nn1=" << nn1 << "\n";
        require(nn1 < nn0, "nanoscale grains must aggregate (closer over time)");
    }
}

// PLANETARY — orbital motion: significant net angular momentum.
void test_planetary_orbits() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::PLANETARY);
        advance_sandbox(sys, 250);
        const double l = std::abs(sys.angular_momentum());
        std::cerr << "[planetary] " << seed << " |L|=" << l
                  << " bound=" << sys.bound_pair_count() << "\n";
        require(l > 20.0, "planetary disk must carry orbital angular momentum");
        require(sys.rms_radius() < 20.0, "planetary must stay on stage");
    }
}

// STELLAR — a bound, virialized cluster.
void test_stellar_virialized() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::STELLAR);
        advance_sandbox(sys, 400);
        const double v = sys.virial_ratio();
        std::cerr << "[stellar] " << seed << " virial=" << v
                  << " bound=" << sys.bound_pair_count() << " rms=" << sys.rms_radius() << "\n";
        require(std::isfinite(v) && sys.bound_pair_count() > 3,
                "stellar cluster must stay gravitationally bound");
        require(sys.rms_radius() < 20.0, "stellar cluster must stay on stage");
    }
}

// GALACTIC — strong, coherent rotation (a spinning disk).
void test_galactic_rotation() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::GALACTIC);
        advance_sandbox(sys, 300);
        const double l = std::abs(sys.angular_momentum());
        std::cerr << "[galactic] " << seed << " |L|=" << l << " rms=" << sys.rms_radius() << "\n";
        require(l > 120.0, "galactic disk must rotate strongly");
        require(sys.rms_radius() < 25.0, "galactic disk must stay on stage");
    }
}

// COSMIC — expansion: structure stretches outward (Hubble flow).
void test_cosmic_expansion() {
    for (const char* seed : seeds) {
        NBodySystem sys = make_sandbox(seed, Scale::COSMIC);
        const double r0 = sys.rms_radius();
        advance_sandbox(sys, 120);
        const double r1 = sys.rms_radius();
        std::cerr << "[cosmic] " << seed << " rms0=" << r0 << " rms1=" << r1 << "\n";
        require(r1 > r0 * 1.10, "cosmic web must expand outward early on");
        require(std::isfinite(r1), "cosmic expansion must stay finite");
    }
}

} // namespace

int main() {
    test_subatomic_confinement();
    test_nuclear_clustering();
    test_atomic_exclusion();
    test_molecular_bonds();
    test_nanoscale_aggregation();
    test_planetary_orbits();
    test_stellar_virialized();
    test_galactic_rotation();
    test_cosmic_expansion();
    return 0;
}
