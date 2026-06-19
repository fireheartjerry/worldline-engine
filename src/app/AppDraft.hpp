#pragma once
// AppRuntime — draft clamping, equality, preview sync.
#include "AppRuntimeCommon.hpp"

inline double wrap_degrees(double degrees) {
    while (degrees <= -180.0) {
        degrees += 360.0;
    }
    while (degrees > 180.0) {
        degrees -= 360.0;
    }
    return degrees;
}

inline void clamp_draft(PendulumDraft& draft) {
    draft.l1 = std::clamp(draft.l1, APP_MIN_LENGTH, APP_MAX_LENGTH);
    draft.l2 = std::clamp(draft.l2, APP_MIN_LENGTH, APP_MAX_LENGTH);
    draft.m1 = std::clamp(draft.m1, APP_MIN_MASS, APP_MAX_MASS);
    draft.m2 = std::clamp(draft.m2, APP_MIN_MASS, APP_MAX_MASS);
    draft.connector1_mass = std::clamp(draft.connector1_mass, APP_MIN_CONNECTOR_MASS, APP_MAX_CONNECTOR_MASS);
    draft.connector2_mass = std::clamp(draft.connector2_mass, APP_MIN_CONNECTOR_MASS, APP_MAX_CONNECTOR_MASS);
    draft.bob1_linear_drag = std::clamp(draft.bob1_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG);
    draft.bob1_quadratic_drag = std::clamp(draft.bob1_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG);
    draft.bob2_linear_drag = std::clamp(draft.bob2_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG);
    draft.bob2_quadratic_drag = std::clamp(draft.bob2_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG);
    draft.connector1_axial_linear_drag = std::clamp(draft.connector1_axial_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG);
    draft.connector1_axial_quadratic_drag = std::clamp(draft.connector1_axial_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG);
    draft.connector1_normal_linear_drag = std::clamp(draft.connector1_normal_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG);
    draft.connector1_normal_quadratic_drag = std::clamp(draft.connector1_normal_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG);
    draft.connector2_axial_linear_drag = std::clamp(draft.connector2_axial_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG);
    draft.connector2_axial_quadratic_drag = std::clamp(draft.connector2_axial_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG);
    draft.connector2_normal_linear_drag = std::clamp(draft.connector2_normal_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG);
    draft.connector2_normal_quadratic_drag = std::clamp(draft.connector2_normal_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG);
    draft.pivot_viscous = std::clamp(draft.pivot_viscous, APP_MIN_JOINT_VISCOUS, APP_MAX_JOINT_VISCOUS);
    draft.pivot_quadratic = std::clamp(draft.pivot_quadratic, APP_MIN_JOINT_QUADRATIC, APP_MAX_JOINT_QUADRATIC);
    draft.pivot_coulomb = std::clamp(draft.pivot_coulomb, APP_MIN_JOINT_COULOMB, APP_MAX_JOINT_COULOMB);
    draft.elbow_viscous = std::clamp(draft.elbow_viscous, APP_MIN_JOINT_VISCOUS, APP_MAX_JOINT_VISCOUS);
    draft.elbow_quadratic = std::clamp(draft.elbow_quadratic, APP_MIN_JOINT_QUADRATIC, APP_MAX_JOINT_QUADRATIC);
    draft.elbow_coulomb = std::clamp(draft.elbow_coulomb, APP_MIN_JOINT_COULOMB, APP_MAX_JOINT_COULOMB);
    draft.theta1_deg = std::clamp(wrap_degrees(draft.theta1_deg), APP_MIN_ANGLE_DEG, APP_MAX_ANGLE_DEG);
    draft.theta2_deg = std::clamp(wrap_degrees(draft.theta2_deg), APP_MIN_ANGLE_DEG, APP_MAX_ANGLE_DEG);
    draft.omega1_deg = std::clamp(draft.omega1_deg, APP_MIN_OMEGA_DEG, APP_MAX_OMEGA_DEG);
    draft.omega2_deg = std::clamp(draft.omega2_deg, APP_MIN_OMEGA_DEG, APP_MAX_OMEGA_DEG);

    if (!draft.rigid_connectors) {
        draft.connector_mass_enabled = false;
    }
}

inline bool drafts_equal(const PendulumDraft& a, const PendulumDraft& b) {
    auto close = [](double lhs, double rhs) {
        return std::abs(lhs - rhs) < 1e-6;
    };
    return close(a.l1, b.l1)
        && close(a.l2, b.l2)
        && close(a.m1, b.m1)
        && close(a.m2, b.m2)
        && close(a.connector1_mass, b.connector1_mass)
        && close(a.connector2_mass, b.connector2_mass)
        && close(a.bob1_linear_drag, b.bob1_linear_drag)
        && close(a.bob1_quadratic_drag, b.bob1_quadratic_drag)
        && close(a.bob2_linear_drag, b.bob2_linear_drag)
        && close(a.bob2_quadratic_drag, b.bob2_quadratic_drag)
        && close(a.connector1_axial_linear_drag, b.connector1_axial_linear_drag)
        && close(a.connector1_axial_quadratic_drag, b.connector1_axial_quadratic_drag)
        && close(a.connector1_normal_linear_drag, b.connector1_normal_linear_drag)
        && close(a.connector1_normal_quadratic_drag, b.connector1_normal_quadratic_drag)
        && close(a.connector2_axial_linear_drag, b.connector2_axial_linear_drag)
        && close(a.connector2_axial_quadratic_drag, b.connector2_axial_quadratic_drag)
        && close(a.connector2_normal_linear_drag, b.connector2_normal_linear_drag)
        && close(a.connector2_normal_quadratic_drag, b.connector2_normal_quadratic_drag)
        && close(a.pivot_viscous, b.pivot_viscous)
        && close(a.pivot_quadratic, b.pivot_quadratic)
        && close(a.pivot_coulomb, b.pivot_coulomb)
        && close(a.elbow_viscous, b.elbow_viscous)
        && close(a.elbow_quadratic, b.elbow_quadratic)
        && close(a.elbow_coulomb, b.elbow_coulomb)
        && close(a.theta1_deg, b.theta1_deg)
        && close(a.theta2_deg, b.theta2_deg)
        && close(a.omega1_deg, b.omega1_deg)
        && close(a.omega2_deg, b.omega2_deg)
        && a.rigid_connectors == b.rigid_connectors
        && a.connector_mass_enabled == b.connector_mass_enabled;
}

inline void sync_preview(AppState& app) {
    if (app.mode == RunMode::STOPPED) {
        clamp_draft(app.draft);
        app.simulation.reset(AppState::make_state(app.draft), AppState::make_params(app.draft));
    }
}

