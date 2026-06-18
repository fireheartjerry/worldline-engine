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

    const bool gravity_tier = tier.gravity_weight > 0.5;

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
        if (gravity_tier) {
            const double ang = frand() * 2.0 * kPi;
            const double rad = std::sqrt(frand()) * 4.5;
            b.pos = {rad * std::cos(ang), rad * std::sin(ang)};
        } else {
            b.pos = {(frand() * 2.0 - 1.0) * 4.0, (frand() * 2.0 - 1.0) * 4.0};
        }
        b.vel = {0.0, 0.0};
        sys.bodies.push_back(b);
        total_mass += b.mass;
    }

    if (gravity_tier && total_mass > 0.0) {
        const double sign = (genome.cosmological_drift > 1.0) ? 1.0 : -1.0;
        for (Body& b : sys.bodies) {
            const double r = b.pos.length();
            if (r > 1.0e-3) {
                const Vec2 tang = {-b.pos.y / r, b.pos.x / r};
                const double speed =
                    0.30 * std::sqrt(sys.params.gravity * total_mass / std::max(r, 1.0));
                b.vel = tang * (speed * sign);
            }
        }
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

} // namespace cosmos
