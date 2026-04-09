#pragma once

#include "math/Vec2.hpp"
#include "seed/MetaSpec.hpp"

struct LawState {
    Vec2 q{};
    Vec2 v{};
    double p = 0.0;

    LawState operator+(const LawState& other) const {
        return {q + other.q, v + other.v, p + other.p};
    }

    LawState operator*(double scalar) const {
        return {q * scalar, v * scalar, p * scalar};
    }
};

inline LawState operator*(double scalar, const LawState& state) {
    return state * scalar;
}

class LawSpec {
public:
    explicit LawSpec(const MetaSpec& meta_spec);

    const MetaSpec& meta_spec() const { return meta_spec_; }
    LawState initial_state() const;
    LawState derivative(const LawState& state) const;
    Vec2 acceleration(const LawState& state) const;
    LawState step(const LawState& state, double dt) const;
    void step_in_place(LawState& state, double dt) const;

    double potential_linear_gain() const { return potential_linear_gain_; }
    double bounded_potential_spectral_norm() const { return bounded_potential_spectral_norm_; }
    double acceleration_ceiling() const { return acceleration_ceiling_; }
    double seeded_p() const { return seeded_p_; }

private:
    MetaSpec meta_spec_{};
    double metric_inverse_[2][2]{};
    double potential_matrix_[2][2]{};
    double bounded_potential_spectral_norm_ = 0.0;
    double potential_linear_gain_ = 1.0;
    double acceleration_ceiling_ = 12.0;
    double min_substep_ = 1.0e-4;
    int max_halvings_ = 10;
    double seeded_p_ = 0.0;

    LawState clamp_state(LawState state) const;
    LawState step_recursive(const LawState& state, double dt, int depth) const;
};
