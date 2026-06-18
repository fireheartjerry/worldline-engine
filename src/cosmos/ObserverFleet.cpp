#include "cosmos/ObserverFleet.hpp"

#include <algorithm>
#include <cmath>

namespace cosmos {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

void ObserverFleet::deploy(int count, double radius, std::uint64_t seed) {
    agents.clear();
    std::uint64_t rng = seed ? seed : 0x9E3779B97F4A7C15ull;
    auto nx = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    auto fr = [&]() { return static_cast<double>(nx() % 1000000ull) / 1000000.0; };
    for (int i = 0; i < std::max(1, count); ++i) {
        Observer a;
        const double ang = fr() * 2.0 * kPi;
        const double r = std::sqrt(fr()) * radius;
        a.pos = {r * std::cos(ang), r * std::sin(ang)};
        agents.push_back(a);
    }
}

void ObserverFleet::sample(Observer& a, const NBodySystem& sys) const {
    int count = 0;
    double best = 1.0e300;
    int best_type = -1;
    for (const Body& b : sys.bodies) {
        const double d = (b.pos - a.pos).length();
        if (d < sense_radius) ++count;
        if (d < best) {
            best = d;
            best_type = b.type;
        }
    }
    a.local_density = static_cast<double>(count) / (kPi * sense_radius * sense_radius);
    a.nearest_type = best_type;
    a.nearest_dist = (best < 1.0e299) ? best : -1.0;
}

void ObserverFleet::update(const NBodySystem& sys, double dt) {
    for (Observer& a : agents) {
        // Seek matter: steer toward the local center of mass of nearby bodies.
        Vec2 com{};
        double mass = 0.0;
        for (const Body& b : sys.bodies) {
            if ((b.pos - a.pos).length() < sense_radius * 1.5) {
                com += b.pos * b.mass;
                mass += b.mass;
            }
        }
        Vec2 steer = (mass > 0.0) ? (com / mass - a.pos) * 0.6 : a.pos * -0.10;
        steer -= a.pos * 0.05; // soft confinement keeps probes in view
        a.vel += steer * dt;
        a.vel = a.vel * std::max(0.0, 1.0 - 0.9 * dt); // damping
        const double sp = a.vel.length();
        if (sp > 3.0) a.vel = a.vel * (3.0 / sp);
        a.pos += a.vel * dt;
        sample(a, sys);
    }
}

FleetReport ObserverFleet::report() const {
    FleetReport r;
    r.agents = static_cast<int>(agents.size());
    if (agents.empty()) return r;

    double sum_density = 0.0;
    double sum_speed = 0.0;
    int near = 0;
    std::vector<int> seen;
    for (const Observer& a : agents) {
        if (!std::isfinite(a.pos.x) || !std::isfinite(a.pos.y)) r.finite = false;
        sum_density += a.local_density;
        r.peak_density = std::max(r.peak_density, a.local_density);
        sum_speed += a.vel.length();
        if (a.nearest_dist >= 0.0 && a.nearest_dist < sense_radius) ++near;
        if (a.nearest_type >= 0 &&
            std::find(seen.begin(), seen.end(), a.nearest_type) == seen.end()) {
            seen.push_back(a.nearest_type);
        }
    }
    r.mean_density = sum_density / agents.size();
    r.mean_speed = sum_speed / agents.size();
    r.coverage = static_cast<double>(near) / agents.size();
    r.species_seen = static_cast<int>(seen.size());
    return r;
}

} // namespace cosmos
