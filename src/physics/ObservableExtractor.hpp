#pragma once

#include "app/AppTypes.hpp"
#include "physics/LawSpec.hpp"
#include "physics/PendulumState.hpp"

class ObservableExtractor {
public:
    explicit ObservableExtractor(const LawSpec& law_spec);

    const PendulumDraft& draft() const { return draft_; }
    PendulumState observe(const LawState& state, double dt = 0.0);
    void reset_drift_phase(double phase = 0.0) { drift_phase_ = phase; }

private:
    LawSpec law_spec_;
    PendulumDraft draft_{};
    Vec2 g_major_axis_{1.0, 0.0};
    Vec2 g_minor_axis_{0.0, 1.0};
    Vec2 v_soft_axis_{1.0, 0.0};
    Vec2 v_hard_axis_{0.0, 1.0};
    double q_scale_major_ = 1.0;
    double q_scale_minor_ = 1.0;
    double v_scale_major_ = 1.0;
    double v_scale_minor_ = 1.0;
    double drift_phase_ = 0.0;
};
