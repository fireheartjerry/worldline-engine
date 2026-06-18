#include "cosmos/NBodySystem.hpp"

#include <algorithm>
#include <cmath>

namespace cosmos {

ForceParams make_force_params(const ScaleTier& tier, const LawGenome& genome) {
    ForceParams p;
    p.gravity = tier.gravity_weight * genome.coupling_gravity * 1.6;
    p.charge = tier.charge_weight * genome.coupling_em * 1.6;
    p.strong = tier.strong_weight * genome.coupling_strong * 2.5;
    p.core = tier.core_weight * 1.2;
    p.exponent = std::clamp(genome.gravity_exponent, 1.5, 2.5);
    p.softening = 0.18;
    p.bond_range = 1.05;
    p.bond_width = 0.60;
    p.core_power = 6.0;
    p.accel_cap = 80.0;
    p.damping = 0.0;
    return p;
}

namespace {

// Scalar coefficient k such that the pair force on body i is k * r, with
// r = pos_j - pos_i. Aggregates every force term (all act along r).
double pair_force_coeff(const Body& bi, const Body& bj, const ForceParams& p,
                        double d2) {
    const double soft2 = d2 + p.softening * p.softening;
    const double d = std::sqrt(std::max(d2, 1.0e-18));

    // Gravity (softened, potential-consistent for any exponent).
    double coeff = p.gravity * bi.mass * bj.mass *
                   std::pow(soft2, -(p.exponent + 1.0) * 0.5);

    // Coulomb: like charges (product > 0) push apart, hence the minus sign.
    if (p.charge != 0.0) {
        coeff += -p.charge * bi.charge * bj.charge * std::pow(soft2, -1.5);
    }

    // Soft-core exclusion: strong short-range repulsion preventing overlap.
    if (p.core != 0.0) {
        const double sigma = bi.radius + bj.radius;
        coeff += p.core * p.core_power * std::pow(sigma, p.core_power) *
                 std::pow(soft2, -(p.core_power + 2.0) * 0.5);
    }

    // Short-range binding well centered at ~contact distance.
    if (p.strong != 0.0) {
        const double sigma = bi.radius + bj.radius;
        const double r0 = p.bond_range * sigma;
        const double w = std::max(p.bond_width * sigma, 1.0e-6);
        const double delta = d - r0;
        const double well = std::exp(-(delta * delta) / (2.0 * w * w));
        // force_d = -dU/dd with U = -strong*well; project onto r via /d.
        const double force_d = -p.strong * (delta / (w * w)) * well;
        coeff += force_d / d;
    }

    return coeff;
}

} // namespace

void NBodySystem::accumulate_pair(int i, int j, std::vector<Vec2>& accel) const {
    const Body& bi = bodies[static_cast<std::size_t>(i)];
    const Body& bj = bodies[static_cast<std::size_t>(j)];
    const Vec2 r = bj.pos - bi.pos;
    const double d2 = r.length_sq();
    const double coeff = pair_force_coeff(bi, bj, params, d2);
    const Vec2 force = r * coeff; // force on i
    accel[static_cast<std::size_t>(i)] += force / bi.mass;
    accel[static_cast<std::size_t>(j)] -= force / bj.mass;
}

std::vector<Vec2> NBodySystem::accelerations() const {
    const std::size_t n = bodies.size();
    std::vector<Vec2> accel(n, Vec2{});
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            accumulate_pair(static_cast<int>(i), static_cast<int>(j), accel);
        }
    }
    if (params.accel_cap > 0.0) {
        for (Vec2& a : accel) {
            const double mag = a.length();
            if (mag > params.accel_cap) {
                a = a * (params.accel_cap / mag);
            }
        }
    }
    return accel;
}

void NBodySystem::step(double dt, int substeps) {
    if (bodies.empty() || dt <= 0.0) {
        return;
    }
    const int steps = std::max(1, substeps);
    const double h = dt / steps;
    const double half = h * 0.5;

    for (int s = 0; s < steps; ++s) {
        // Velocity-Verlet (kick-drift-kick); acceleration is velocity-independent.
        std::vector<Vec2> a0 = accelerations();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            bodies[i].vel += a0[i] * half;
            bodies[i].pos += bodies[i].vel * h;
        }
        std::vector<Vec2> a1 = accelerations();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            bodies[i].vel += a1[i] * half;
        }
        if (params.damping > 0.0) {
            const double factor = std::max(0.0, 1.0 - params.damping * h);
            for (Body& b : bodies) {
                b.vel = b.vel * factor;
            }
        }
    }
}

double NBodySystem::kinetic_energy() const {
    double ke = 0.0;
    for (const Body& b : bodies) {
        ke += 0.5 * b.mass * b.vel.length_sq();
    }
    return ke;
}

double NBodySystem::pair_potential(int i, int j) const {
    const Body& bi = bodies[static_cast<std::size_t>(i)];
    const Body& bj = bodies[static_cast<std::size_t>(j)];
    const Vec2 r = bj.pos - bi.pos;
    const double soft2 = r.length_sq() + params.softening * params.softening;
    const double softd = std::sqrt(soft2);

    double u = 0.0;
    // Gravity potential consistent with the softened force.
    if (std::abs(params.exponent - 1.0) > 1.0e-6) {
        u += -params.gravity * bi.mass * bj.mass /
             ((params.exponent - 1.0) * std::pow(soft2, (params.exponent - 1.0) * 0.5));
    } else {
        u += params.gravity * bi.mass * bj.mass * std::log(softd);
    }
    if (params.charge != 0.0) {
        u += params.charge * bi.charge * bj.charge / softd;
    }
    if (params.core != 0.0) {
        const double sigma = bi.radius + bj.radius;
        u += params.core * std::pow(sigma, params.core_power) *
             std::pow(soft2, -params.core_power * 0.5);
    }
    if (params.strong != 0.0) {
        const double sigma = bi.radius + bj.radius;
        const double r0 = params.bond_range * sigma;
        const double w = std::max(params.bond_width * sigma, 1.0e-6);
        const double d = std::sqrt(std::max(r.length_sq(), 0.0));
        const double delta = d - r0;
        u += -params.strong * std::exp(-(delta * delta) / (2.0 * w * w));
    }
    return u;
}

double NBodySystem::potential_energy() const {
    double pe = 0.0;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            pe += pair_potential(static_cast<int>(i), static_cast<int>(j));
        }
    }
    return pe;
}

Vec2 NBodySystem::total_momentum() const {
    Vec2 p{};
    for (const Body& b : bodies) {
        p += b.vel * b.mass;
    }
    return p;
}

Vec2 NBodySystem::center_of_mass() const {
    Vec2 c{};
    double m = 0.0;
    for (const Body& b : bodies) {
        c += b.pos * b.mass;
        m += b.mass;
    }
    return (m > 0.0) ? c / m : Vec2{};
}

double NBodySystem::virial_ratio() const {
    const double pe = std::abs(potential_energy());
    return (pe > 1.0e-12) ? (2.0 * kinetic_energy() / pe) : 0.0;
}

int NBodySystem::bound_pair_count() const {
    int count = 0;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Body& bi = bodies[i];
            const Body& bj = bodies[j];
            const double mu = (bi.mass * bj.mass) / std::max(bi.mass + bj.mass, 1.0e-12);
            const Vec2 dv = bj.vel - bi.vel;
            const double pair_ke = 0.5 * mu * dv.length_sq();
            if (pair_ke + pair_potential(static_cast<int>(i), static_cast<int>(j)) < 0.0) {
                ++count;
            }
        }
    }
    return count;
}

double NBodySystem::rms_radius() const {
    if (bodies.empty()) {
        return 0.0;
    }
    const Vec2 com = center_of_mass();
    double sum = 0.0;
    for (const Body& b : bodies) {
        sum += (b.pos - com).length_sq();
    }
    return std::sqrt(sum / static_cast<double>(bodies.size()));
}

} // namespace cosmos
