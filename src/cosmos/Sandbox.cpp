#include "cosmos/Sandbox.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cosmos {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::uint64_t spawn_seed(const LawGenome& genome, Scale scale) {
    std::uint64_t rng = genome.signature ^ (0x9E3779B97F4A7C15ull * (scale_index(scale) + 1));
    return (rng == 0) ? 0x1234567811111111ull : rng;
}

} // namespace

void populate_sandbox(NBodySystem& sys,
                      const std::vector<UniverseObject>& catalog,
                      const LawGenome& genome,
                      Scale scale,
                      int body_count) {
    sys.bodies.clear();
    const ScaleTier& tier = tier_for(scale);
    sys.params = make_force_params(tier, genome);

    const auto objs = objects_for_scale(catalog, scale);
    if (objs.empty()) {
        return;
    }

    std::uint64_t rng = spawn_seed(genome, scale);
    auto next = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    auto frand = [&]() { return static_cast<double>(next() % 1000000ull) / 1000000.0; };

    double total_ab = 0.0;
    for (const UniverseObject* o : objs) {
        total_ab += std::max(0.05, o->abundance);
    }
    auto pick = [&]() -> const UniverseObject* {
        double r = frand() * total_ab;
        for (const UniverseObject* o : objs) {
            r -= std::max(0.05, o->abundance);
            if (r <= 0.0) {
                return o;
            }
        }
        return objs.back();
    };

    const int count = std::max(2, body_count);
    auto gaussian = [&]() { return (frand() + frand() + frand() - 1.5) * 1.15; }; // ~N(0,1)

    // Pre-pick a few cluster centers for tiers that nucleate (nuclear).
    Vec2 centers[5];
    for (Vec2& c : centers) {
        c = {(frand() * 2.0 - 1.0) * 3.2, (frand() * 2.0 - 1.0) * 3.2};
    }

    double total_mass = 0.0;
    for (int i = 0; i < count; ++i) {
        const UniverseObject* o = pick();
        Body b;
        b.mass = o->sim_mass;
        b.radius = o->sim_radius;
        b.charge = o->sim_charge;
        b.color = o->color;
        b.type = static_cast<int>(static_cast<std::size_t>(
            std::find(objs.begin(), objs.end(), o) - objs.begin()));

        // Tier-specific spawn position.
        switch (scale) {
        case Scale::SUBATOMIC: {
            const double ang = frand() * 2.0 * kPi;
            b.pos = {std::sqrt(frand()) * 2.6 * std::cos(ang), std::sqrt(frand()) * 2.6 * std::sin(ang)};
            break;
        }
        case Scale::NUCLEAR: {
            const Vec2 c = centers[i % 5];
            b.pos = {c.x + gaussian() * 0.6, c.y + gaussian() * 0.6};
            break;
        }
        case Scale::ATOMIC: {
            const int side = 9;
            const double gx = (i % side) - (side - 1) * 0.5;
            const double gy = (i / side) - (side - 1) * 0.5;
            b.pos = {gx * 0.95 + gaussian() * 0.15, gy * 0.95 + gaussian() * 0.15};
            break;
        }
        case Scale::MOLECULAR: {
            if (i % 2 == 0 || sys.bodies.empty()) {
                b.pos = {(frand() * 2.0 - 1.0) * 4.2, (frand() * 2.0 - 1.0) * 4.2};
            } else {
                // Bond to the previous body at its preferred distance.
                const Body& prev = sys.bodies.back();
                const double r0 = sys.params.bond_range * (prev.radius + b.radius);
                const double ang = frand() * 2.0 * kPi;
                b.pos = {prev.pos.x + r0 * std::cos(ang), prev.pos.y + r0 * std::sin(ang)};
            }
            break;
        }
        case Scale::NANOSCALE: {
            const double ang = frand() * 2.0 * kPi;
            b.pos = {std::sqrt(frand()) * 5.5 * std::cos(ang), std::sqrt(frand()) * 5.5 * std::sin(ang)};
            break;
        }
        default: { // gravity tiers: a disk
            const double ang = frand() * 2.0 * kPi;
            const double reach = (scale == Scale::GALACTIC || scale == Scale::COSMIC) ? 5.5 : 4.5;
            b.pos = {std::sqrt(frand()) * reach * std::cos(ang), std::sqrt(frand()) * reach * std::sin(ang)};
            break;
        }
        }

        b.vel = {0.0, 0.0};
        sys.bodies.push_back(b);
        total_mass += b.mass;
    }

    // Tier-specific initial velocities, with genome-derived magnitudes so the
    // amount of spin, expansion and orbital support varies per universe.
    auto norm = [](double v, double lo, double hi) {
        return std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
    };
    const double driftN = norm(genome.cosmological_drift, 0.70, 1.50);
    const double gravN = norm(genome.coupling_gravity, 0.40, 1.70);
    const double drift_sign = (genome.cosmological_drift > 1.0) ? 1.0 : -1.0;
    const double orbit_k = 0.38 * (0.90 + 0.25 * gravN);  // orbital support
    const double gal_orbit = 0.60 + 0.20 * driftN;        // sub-circular disk support
    const double expand_k = 0.45 + 0.35 * driftN;          // [0.45,0.80]
    for (Body& b : sys.bodies) {
        const double r = b.pos.length();
        const Vec2 tang = (r > 1.0e-3) ? Vec2{-b.pos.y / r, b.pos.x / r} : Vec2{0.0, 0.0};
        switch (scale) {
        case Scale::SUBATOMIC:
        case Scale::NANOSCALE:
            b.vel = {gaussian() * 0.25, gaussian() * 0.25}; // thermal jitter
            break;
        case Scale::PLANETARY:
        case Scale::STELLAR:
            // Orbital support: Keplerian-ish circular speed with dispersion.
            b.vel = tang * (drift_sign * orbit_k *
                            std::sqrt(sys.params.gravity * total_mass / std::max(r, 1.0)));
            if (scale == Scale::STELLAR) {
                b.vel += Vec2{gaussian() * 0.25, gaussian() * 0.25};
            }
            break;
        case Scale::GALACTIC:
            // Rotationally-supported disk: near-circular orbital speed (so the
            // disk stays ordered instead of heating into a turbulent blob).
            b.vel = tang * (drift_sign * gal_orbit *
                            std::sqrt(sys.params.gravity * total_mass / std::max(r, 0.6)));
            break;
        case Scale::COSMIC:
            // Hubble flow: outward velocity proportional to distance.
            b.vel = b.pos * expand_k;
            break;
        default:
            break; // NUCLEAR, ATOMIC, MOLECULAR start at rest and settle
        }
    }

    // Remove any net drift so the system stays centered on the stage.
    if (total_mass > 0.0) {
        const Vec2 com_vel = sys.total_momentum() / total_mass;
        for (Body& b : sys.bodies) {
            b.vel -= com_vel;
        }
    }
}

void advance_sandbox(NBodySystem& sys, int steps) {
    for (int i = 0; i < steps; ++i) {
        sys.step(kSandboxDt, kSandboxSubsteps);
    }
}

SandboxStats sandbox_stats(const NBodySystem& sys) {
    SandboxStats stats;
    stats.bodies = static_cast<int>(sys.bodies.size());
    if (sys.bodies.empty()) {
        return stats;
    }
    for (const Body& b : sys.bodies) {
        if (!std::isfinite(b.pos.x) || !std::isfinite(b.pos.y) ||
            !std::isfinite(b.vel.x) || !std::isfinite(b.vel.y) ||
            std::abs(b.pos.x) > 1.0e6 || std::abs(b.pos.y) > 1.0e6) {
            stats.finite = false;
        }
    }
    stats.energy = sys.total_energy();
    stats.virial = sys.virial_ratio();
    stats.rms_radius = sys.rms_radius();
    stats.bound_pairs = sys.bound_pair_count();
    if (!std::isfinite(stats.energy) || !std::isfinite(stats.rms_radius)) {
        stats.finite = false;
    }
    return stats;
}

namespace {

double mean_nearest_neighbor(const NBodySystem& sys) {
    if (sys.bodies.size() < 2) {
        return 0.0;
    }
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

int close_pair_count(const NBodySystem& sys, double max_distance) {
    int n = 0;
    for (std::size_t i = 0; i < sys.bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < sys.bodies.size(); ++j) {
            if ((sys.bodies[j].pos - sys.bodies[i].pos).length() <= max_distance) {
                ++n;
            }
        }
    }
    return n;
}

double mean_radial_velocity(const NBodySystem& sys) {
    if (sys.bodies.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const Body& b : sys.bodies) {
        const double r = b.pos.length();
        if (r > 1.0e-6) {
            total += b.pos.dot(b.vel) / r; // outward component
        }
    }
    return total / static_cast<double>(sys.bodies.size());
}

} // namespace

SignatureMetric tier_signature_metric(Scale scale, const NBodySystem& sys) {
    switch (scale) {
    case Scale::SUBATOMIC: return {"CONFINEMENT", sys.max_radius()};
    case Scale::NUCLEAR:   return {"NUCLEON BONDS", static_cast<double>(close_pair_count(sys, 1.4))};
    case Scale::ATOMIC:    return {"LATTICE SPACING", mean_nearest_neighbor(sys)};
    case Scale::MOLECULAR: return {"MOLECULE BONDS", static_cast<double>(close_pair_count(sys, 2.8))};
    case Scale::NANOSCALE: return {"AGGREGATION", mean_nearest_neighbor(sys)};
    case Scale::PLANETARY: return {"ORBITAL L", std::abs(sys.angular_momentum())};
    case Scale::STELLAR:   return {"BOUND PAIRS", static_cast<double>(sys.bound_pair_count())};
    case Scale::GALACTIC:  return {"ROTATION L", std::abs(sys.angular_momentum())};
    case Scale::COSMIC:    return {"EXPANSION", mean_radial_velocity(sys)};
    case Scale::COUNT:     break;
    }
    return {"", 0.0};
}

} // namespace cosmos
