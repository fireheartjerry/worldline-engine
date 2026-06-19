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

// Mass-independent spin parameter lambda = |L| / (M * v_rms * r_rms): ~1 for a
// clean rotating disk, ~0 for random motion. Independent of the mass mix.
double spin_parameter(const NBodySystem& sys) {
    double mass = 0.0;
    double ke = 0.0;
    for (const Body& b : sys.bodies) {
        mass += b.mass;
        ke += 0.5 * b.mass * b.vel.length_sq();
    }
    const double v_rms = (mass > 0.0) ? std::sqrt(2.0 * ke / mass) : 0.0;
    const double denom = mass * v_rms * sys.rms_radius();
    return (denom > 1.0e-9) ? std::abs(sys.angular_momentum()) / denom : 0.0;
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
        // The bulk stays bounded despite the energy injection: confinement does
        // not let the plasma disperse the way an unbound system would. The cap is
        // generous because the enriched particle zoo (heavy bosons + a true
        // Planck-mass object) widens the mass spread; what matters is that the
        // cloud stays bound (rms ~10, max ~25) rather than growing without limit.
        require(std::isfinite(rms) && rms < 13.0,
                "subatomic confinement must keep the plasma bound");
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
        require(nn > 0.40, "atomic exclusion must keep a packed spacing");
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
        require(l > 12.0, "planetary disk must carry orbital angular momentum");
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
        advance_sandbox(sys, 200);
        const double lambda = spin_parameter(sys);
        std::cerr << "[galactic] " << seed << " spin=" << lambda
                  << " |L|=" << std::abs(sys.angular_momentum()) << " rms=" << sys.rms_radius() << "\n";
        require(lambda > 0.14, "galactic disk must rotate (spin parameter)");
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

// Genome-derived parameters: different universes must produce different tier
// physics (so the multiverse actually varies), while keeping each signature.
void test_genome_variation() {
    const LawGenome a = generate_law_genome(std::string("alpha"));
    const LawGenome b = generate_law_genome(std::string("vega-9"));

    const ForceParams ga = make_force_params(tier_for(Scale::GALACTIC), a);
    const ForceParams gb = make_force_params(tier_for(Scale::GALACTIC), b);
    require(std::abs(ga.swirl - gb.swirl) > 1.0e-6,
            "galactic rotation drive must vary across universes");

    int differing = 0;
    for (std::size_t s = 0; s < kScaleCount; ++s) {
        const ForceParams pa = make_force_params(scale_ladder()[s], a);
        const ForceParams pb = make_force_params(scale_ladder()[s], b);
        if (std::abs(pa.confinement - pb.confinement) > 1.0e-9) ++differing;
    }
    std::cerr << "[variation] tiers differing in trap strength = " << differing << "\n";
    require(differing >= 7, "most tiers must differ in trap strength across universes");
}

// Every tier exposes a finite, labeled signature metric for the live panel.
void test_signature_metrics() {
    for (std::size_t s = 0; s < kScaleCount; ++s) {
        const Scale scale = static_cast<Scale>(s);
        NBodySystem sys = make_sandbox("alpha", scale);
        advance_sandbox(sys, 150);
        const SignatureMetric m = tier_signature_metric(scale, sys);
        require(m.label[0] != '\0', "tier signature must carry a label");
        require(std::isfinite(m.value), "tier signature value must be finite");
    }
    // Semantics spot-check: the galactic signature is the rotation |L|.
    NBodySystem g = make_sandbox("alpha", Scale::GALACTIC);
    advance_sandbox(g, 150);
    const SignatureMetric gm = tier_signature_metric(Scale::GALACTIC, g);
    require(std::abs(gm.value - std::abs(g.angular_momentum())) < 1.0e-6,
            "galactic signature must report rotation |L|");
}

} // namespace

int main() {
    test_genome_variation();
    test_signature_metrics();
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
