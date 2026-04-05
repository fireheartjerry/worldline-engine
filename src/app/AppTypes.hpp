#pragma once
#include "../physics/Simulation.hpp"
#include "../renderer/Trail.hpp"
#include <array>
#include <string>

constexpr std::size_t APP_TRAIL_CAPACITY = 65536;
constexpr double APP_DEG_TO_RAD = 3.14159265358979323846 / 180.0;

enum class RunMode { STOPPED, RUNNING, PAUSED };

enum class AppScreen {
    MAIN_MENU,
    DEFAULT_UNIVERSE,
    SEEDED_UNIVERSE
};

enum class OverlayPreset {
    CLEAN,
    FORCES,
    RESISTANCE,
    FULL,
    CUSTOM
};

enum class PanelCommand {
    NONE,
    LAUNCH,
    TOGGLE_PAUSE,
    STOP,
    CLEAR_TRAIL
};

enum class PanelSection {
    NAV,
    CONTROLS,
    CONNECTOR,
    GEOMETRY,
    LAUNCH,
    BOB_DRAG,
    LINK_DRAG,
    JOINTS,
    FLOW,
    VISUALS,
    METRICS,
    COUNT
};

enum class FieldId {
    NONE,
    LENGTH1,
    LENGTH2,
    MASS1,
    MASS2,
    CONNECTOR1_MASS,
    CONNECTOR2_MASS,
    BOB1_LINEAR_DRAG,
    BOB1_QUADRATIC_DRAG,
    BOB2_LINEAR_DRAG,
    BOB2_QUADRATIC_DRAG,
    CONNECTOR1_AXIAL_LINEAR_DRAG,
    CONNECTOR1_AXIAL_QUADRATIC_DRAG,
    CONNECTOR1_NORMAL_LINEAR_DRAG,
    CONNECTOR1_NORMAL_QUADRATIC_DRAG,
    CONNECTOR2_AXIAL_LINEAR_DRAG,
    CONNECTOR2_AXIAL_QUADRATIC_DRAG,
    CONNECTOR2_NORMAL_LINEAR_DRAG,
    CONNECTOR2_NORMAL_QUADRATIC_DRAG,
    PIVOT_VISCOUS,
    PIVOT_QUADRATIC,
    PIVOT_COULOMB,
    ELBOW_VISCOUS,
    ELBOW_QUADRATIC,
    ELBOW_COULOMB,
    FLOW_WIND_X,
    FLOW_WIND_Y,
    FLOW_SHEAR_X,
    FLOW_SHEAR_Y,
    FLOW_SWIRL,
    FLOW_GUST_X,
    FLOW_GUST_Y,
    FLOW_GUST_FREQUENCY,
    ANGLE1,
    ANGLE2,
    OMEGA1,
    OMEGA2,
    TRAIL_MEMORY,
    VELOCITY_VECTOR_SCALE,
    FORCE_VECTOR_SCALE,
    FLOW_VECTOR_SCALE,
    FLOW_DENSITY
};

struct PendulumDraft {
    double l1 = 1.30;
    double l2 = 1.10;
    double m1 = 1.50;
    double m2 = 1.20;
    double connector1_mass = 0.35;
    double connector2_mass = 0.25;
    double bob1_linear_drag = 0.00;
    double bob1_quadratic_drag = 0.00;
    double bob2_linear_drag = 0.00;
    double bob2_quadratic_drag = 0.00;
    double connector1_axial_linear_drag = 0.00;
    double connector1_axial_quadratic_drag = 0.00;
    double connector1_normal_linear_drag = 0.10;
    double connector1_normal_quadratic_drag = 0.05;
    double connector2_axial_linear_drag = 0.00;
    double connector2_axial_quadratic_drag = 0.00;
    double connector2_normal_linear_drag = 0.10;
    double connector2_normal_quadratic_drag = 0.05;
    double pivot_viscous = 0.00;
    double pivot_quadratic = 0.00;
    double pivot_coulomb = 0.00;
    double elbow_viscous = 0.00;
    double elbow_quadratic = 0.00;
    double elbow_coulomb = 0.00;
    double flow_wind_x = 0.00;
    double flow_wind_y = 0.00;
    double flow_shear_x = 0.00;
    double flow_shear_y = 0.00;
    double flow_swirl = 0.00;
    double flow_gust_x = 0.00;
    double flow_gust_y = 0.00;
    double flow_gust_frequency = 0.00;
    double theta1_deg = 128.0;
    double theta2_deg = 97.0;
    double omega1_deg = 0.0;
    double omega2_deg = 0.0;
    bool rigid_connectors = true;
    bool connector_mass_enabled = false;
};

struct VisualDraft {
    double trail_memory = 0.93;
    bool show_vectors = true;
    bool show_velocity_vectors = true;
    bool show_gravity_vectors = true;
    bool show_drag_vectors = true;
    bool show_reaction_vectors = true;
    bool show_net_vectors = false;
    bool show_link_drag_vectors = true;
    bool show_joint_torque_vectors = true;
    double velocity_vector_scale = 0.24;
    double force_vector_scale = 0.038;
    OverlayPreset preset = OverlayPreset::CUSTOM;
};

struct UiState {
    AppScreen screen = AppScreen::MAIN_MENU;
    FieldId active_field = FieldId::NONE;
    FieldId active_slider = FieldId::NONE;
    std::string buffer;
    int drag_handle = 0;
    float panel_scroll = 0.0f;
    bool slider_wheel_used = false;
    bool settings_open = false;
    std::array<bool, static_cast<std::size_t>(PanelSection::COUNT)> collapsed_sections{};
};

inline std::array<bool, static_cast<std::size_t>(PanelSection::COUNT)> default_collapsed_sections() {
    std::array<bool, static_cast<std::size_t>(PanelSection::COUNT)> collapsed{};
    collapsed[static_cast<std::size_t>(PanelSection::LINK_DRAG)] = true;
    collapsed[static_cast<std::size_t>(PanelSection::JOINTS)] = true;
    collapsed[static_cast<std::size_t>(PanelSection::VISUALS)] = true;
    collapsed[static_cast<std::size_t>(PanelSection::METRICS)] = true;
    return collapsed;
}

inline const PendulumDraft& default_pendulum_draft() {
    static const PendulumDraft draft{};
    return draft;
}

inline const VisualDraft& default_visual_draft() {
    static const VisualDraft draft{};
    return draft;
}

struct AppState {
    PendulumDraft draft;
    PendulumDraft applied;
    VisualDraft visuals;
    Simulation simulation;
    Trail<APP_TRAIL_CAPACITY> trail;
    UiState ui;
    RunMode mode = RunMode::STOPPED;
    double accumulator = 0.0;
    double trail_sample_timer = 0.0;

    AppState()
        : draft(),
          applied(draft),
          visuals(),
          simulation(make_state(draft), make_params(draft)) {
        ui.collapsed_sections = default_collapsed_sections();
    }

    static PendulumParams make_params(const PendulumDraft& draft) {
        PendulumParams params;
        params.l1 = draft.l1;
        params.l2 = draft.l2;
        params.m1 = draft.m1;
        params.m2 = draft.m2;
        params.g = 9.81;
        params.gravity_exponent = 2.0;
        params.bob1_drag = {draft.bob1_linear_drag, draft.bob1_quadratic_drag};
        params.bob2_drag = {draft.bob2_linear_drag, draft.bob2_quadratic_drag};
        params.connector1_drag = {
            {draft.connector1_axial_linear_drag, draft.connector1_axial_quadratic_drag},
            {draft.connector1_normal_linear_drag, draft.connector1_normal_quadratic_drag}
        };
        params.connector2_drag = {
            {draft.connector2_axial_linear_drag, draft.connector2_axial_quadratic_drag},
            {draft.connector2_normal_linear_drag, draft.connector2_normal_quadratic_drag}
        };
        params.flow_field = {};
        params.connector_mode = draft.rigid_connectors
            ? ConnectorMode::RIGID
            : ConnectorMode::ROPE;
        params.pivot_resistance = draft.rigid_connectors
            ? JointResistance{draft.pivot_viscous, draft.pivot_quadratic, draft.pivot_coulomb}
            : JointResistance{};
        params.elbow_resistance = draft.rigid_connectors
            ? JointResistance{draft.elbow_viscous, draft.elbow_quadratic, draft.elbow_coulomb}
            : JointResistance{};
        params.connector1_mass =
            (draft.rigid_connectors && draft.connector_mass_enabled)
                ? draft.connector1_mass
                : 0.0;
        params.connector2_mass =
            (draft.rigid_connectors && draft.connector_mass_enabled)
                ? draft.connector2_mass
                : 0.0;
        return params;
    }

    static PendulumState make_state(const PendulumDraft& draft) {
        PendulumState state;
        state.theta1 = draft.theta1_deg * APP_DEG_TO_RAD;
        state.theta2 = draft.theta2_deg * APP_DEG_TO_RAD;
        state.omega1 = draft.omega1_deg * APP_DEG_TO_RAD;
        state.omega2 = draft.omega2_deg * APP_DEG_TO_RAD;
        return state;
    }
};

struct FieldSpec {
    FieldId id;
    const char* label;
    double PendulumDraft::* member;
    double min;
    double max;
    int precision;
    const char* unit;
};

struct VisualFieldSpec {
    FieldId id;
    const char* label;
    double VisualDraft::* member;
    double min;
    double max;
    int precision;
    const char* unit;
};
