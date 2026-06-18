#include "cosmos/NBodySystem.hpp"

#include <algorithm>
#include <cmath>

namespace cosmos {

ForceParams make_force_params(const ScaleTier& tier, const LawGenome& genome) {
    // Each tier has its own characteristic force law. The genome modulates every
    // knob within a band that preserves the qualitative signature (confinement,
    // bonding, orbits, rotation, expansion) while making universes differ
    // dramatically: trap tightness, damping, rotation, exclusion and bonding all
    // shift with the universe's constants.
    const double em = genome.coupling_em;
    const double strong = genome.coupling_strong;
    const double grav = genome.coupling_gravity;

    // Normalized genome drivers in [0,1].
    auto norm = [](double v, double lo, double hi) {
        return std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
    };
    const double stabN = norm(genome.stability_bias, 0.15, 0.90);
    const double driftN = norm(genome.cosmological_drift, 0.70, 1.50);
    const double massN = norm(genome.mass_scale, 0.60, 1.60);
    const double emN = norm(em, 0.35, 1.80);
    const double strongN = norm(strong, 0.35, 1.80);

    // Universe-wide modulators (shared across tiers).
    const double trap = 0.75 + 0.65 * stabN;   // [0.75,1.40] tighter when stable
    const double drag = 0.70 + 0.75 * stabN;   // [0.70,1.45]
    const double exclude = 0.80 + 0.55 * emN;  // [0.80,1.35] core stiffness
    const double bond_d = 0.92 + 0.18 * massN; // [0.92,1.10] bond distance
    const double spin = 0.60 + 1.05 * driftN;  // [0.60,1.65] rotation drive
    const double bind = 0.65 + 0.85 * strongN; // [0.65,1.50] confinement/binding

    ForceParams p;
    p.exponent = std::clamp(genome.gravity_exponent, 1.5, 2.5);
    p.softening = 0.30;
    p.core_power = 4.0;
    p.accel_cap = 50.0;

    switch (tier.scale) {
    case Scale::SUBATOMIC:
        p.charge = 1.2 * em;
        p.strong = 1.2 * strong;
        p.core = 0.5 * exclude;
        p.linear = 0.45 * strong * bind;
        p.confinement = 1.5 * trap;
        p.damping = 0.18 * drag;
        break;
    case Scale::NUCLEAR:
        p.charge = 0.30 * em;
        p.strong = 2.6 * strong * bind;
        p.core = 0.75 * exclude;
        p.bond_range = 1.0 * bond_d;
        p.bond_width = 0.42;
        p.confinement = 1.0 * trap;
        p.damping = 0.16 * drag;
        break;
    case Scale::ATOMIC:
        p.gravity = 0.18;
        p.charge = 0.6 * em;
        p.strong = 0.8 * strong;
        p.core = 1.0 * exclude;
        p.confinement = 1.0 * trap;
        p.damping = 0.15 * drag;
        break;
    case Scale::MOLECULAR:
        p.charge = 0.20 * em;
        p.strong = 2.2 * strong * bind;
        p.core = 0.85 * exclude;
        p.bond_range = 1.15 * bond_d;
        p.bond_width = 0.5;
        p.confinement = 0.9 * trap;
        p.damping = 0.14 * drag;
        break;
    case Scale::NANOSCALE:
        p.gravity = 0.12;
        p.strong = 0.6 * strong;
        p.core = 0.6 * exclude;
        p.bond_range = 1.5 * bond_d;
        p.bond_width = 0.95;
        p.confinement = 0.7 * trap;
        p.damping = 0.24 * drag;
        break;
    case Scale::PLANETARY:
        p.gravity = 1.3 * grav;
        p.core = 0.95 * exclude;
        p.confinement = 0.9 * trap;
        p.damping = 0.06 * drag;
        break;
    case Scale::STELLAR:
        p.gravity = 1.4 * grav;
        p.core = 0.7 * exclude;
        p.confinement = 0.8 * trap;
        p.damping = 0.03 * drag;
        break;
    case Scale::GALACTIC:
        p.gravity = 1.2 * grav;
        p.core = 0.55 * exclude;
        p.swirl = 0.6 * spin;
        p.confinement = 0.7 * trap;
        p.damping = 0.04 * drag;
        break;
    case Scale::COSMIC:
        p.gravity = 1.0 * grav;
        p.core = 0.4 * exclude;
        p.confinement = 0.35 * trap;
        p.damping = 0.02 * drag;
        break;
    case Scale::COUNT:
        break;
    }
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

    // Linear confinement: attraction that grows with separation (quark-style).
    if (p.linear != 0.0) {
        coeff += p.linear;
    }

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
    if (params.confinement > 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            accel[i] -= bodies[i].pos * params.confinement;
        }
    }
    if (params.swirl != 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            const Vec2& q = bodies[i].pos;
            accel[i] += Vec2{-q.y, q.x} * params.swirl;
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
    if (params.linear != 0.0) {
        u += -0.5 * params.linear * r.length_sq();
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
    if (params.confinement > 0.0) {
        for (const Body& b : bodies) {
            pe += 0.5 * params.confinement * b.mass * b.pos.length_sq();
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

double NBodySystem::angular_momentum() const {
    double l = 0.0;
    for (const Body& b : bodies) {
        l += b.mass * (b.pos.x * b.vel.y - b.pos.y * b.vel.x);
    }
    return l;
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

double NBodySystem::max_radius() const {
    const Vec2 com = center_of_mass();
    double m = 0.0;
    for (const Body& b : bodies) {
        m = std::max(m, (b.pos - com).length());
    }
    return m;
}

} // namespace cosmos
