#pragma once
#include "AppFields.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/VectorOverlay.hpp"
#include "../ui/CanvasOverlayLayout.hpp"
#include "../ui/CanvasHud.hpp"
#include "../ui/UiPrimitives.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

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

inline unsigned char trail_fade_alpha(const VisualDraft& visuals) {
    const double clamped = std::clamp(visuals.trail_memory, 0.0, 1.0);
    const double alpha = 10.0 - clamped * 8.8;
    return static_cast<unsigned char>(std::clamp(alpha, 1.0, 10.0));
}

inline VectorOverlayConfig make_vector_overlay_config(const VisualDraft& visuals) {
    VectorOverlayConfig config;
    config.enabled = visuals.show_vectors;
    config.show_velocity = visuals.show_velocity_vectors;
    config.show_gravity = visuals.show_gravity_vectors;
    config.show_drag = visuals.show_drag_vectors;
    config.show_reaction = visuals.show_reaction_vectors;
    config.show_net = visuals.show_net_vectors;
    config.show_link_drag = visuals.show_link_drag_vectors;
    config.show_joint_torque = visuals.show_joint_torque_vectors;
    config.velocity_scale = visuals.velocity_vector_scale;
    config.force_scale = visuals.force_vector_scale;
    return config;
}

inline const char* overlay_preset_short_label(OverlayPreset preset) {
    switch (preset) {
    case OverlayPreset::CLEAN: return "CLN";
    case OverlayPreset::FORCES: return "FRC";
    case OverlayPreset::RESISTANCE: return "RES";
    case OverlayPreset::FULL: return "ALL";
    case OverlayPreset::CUSTOM: return "CST";
    }
    return "CST";
}

inline const char* overlay_preset_label(OverlayPreset preset) {
    switch (preset) {
    case OverlayPreset::CLEAN: return "Clean";
    case OverlayPreset::FORCES: return "Forces";
    case OverlayPreset::RESISTANCE: return "Resistance";
    case OverlayPreset::FULL: return "Full";
    case OverlayPreset::CUSTOM: return "Custom";
    }
    return "Custom";
}

inline void apply_overlay_preset(VisualDraft& visuals, OverlayPreset preset) {
    visuals.preset = preset;
    switch (preset) {
    case OverlayPreset::CLEAN:
        visuals.show_vectors = false;
        visuals.show_velocity_vectors = false;
        visuals.show_gravity_vectors = false;
        visuals.show_drag_vectors = false;
        visuals.show_reaction_vectors = false;
        visuals.show_net_vectors = false;
        visuals.show_link_drag_vectors = false;
        visuals.show_joint_torque_vectors = false;
        break;
    case OverlayPreset::FORCES:
        visuals.show_vectors = true;
        visuals.show_velocity_vectors = true;
        visuals.show_gravity_vectors = true;
        visuals.show_drag_vectors = false;
        visuals.show_reaction_vectors = true;
        visuals.show_net_vectors = true;
        visuals.show_link_drag_vectors = false;
        visuals.show_joint_torque_vectors = false;
        break;
    case OverlayPreset::RESISTANCE:
        visuals.show_vectors = true;
        visuals.show_velocity_vectors = false;
        visuals.show_gravity_vectors = false;
        visuals.show_drag_vectors = true;
        visuals.show_reaction_vectors = false;
        visuals.show_net_vectors = false;
        visuals.show_link_drag_vectors = true;
        visuals.show_joint_torque_vectors = true;
        break;
    case OverlayPreset::FULL:
        visuals.show_vectors = true;
        visuals.show_velocity_vectors = true;
        visuals.show_gravity_vectors = true;
        visuals.show_drag_vectors = true;
        visuals.show_reaction_vectors = true;
        visuals.show_net_vectors = true;
        visuals.show_link_drag_vectors = true;
        visuals.show_joint_torque_vectors = true;
        break;
    case OverlayPreset::CUSTOM:
        break;
    }
}

inline const char* connector_mode_label(const PendulumDraft& draft) {
    if (!draft.rigid_connectors) {
        return "Massless ropes";
    }
    if (draft.connector_mass_enabled) {
        return "Rigid rods + mass";
    }
    return "Rigid rods";
}

inline CanvasOverlayView make_canvas_overlay_view(
    const AppState& app,
    const Simulation::VisualDiagnostics* diagnostics,
    double dissipation_power) {
    const PendulumDraft& active_draft =
        (app.mode == RunMode::STOPPED) ? app.draft : app.applied;
    CanvasOverlayView view;
    view.show_vectors = app.visuals.show_vectors;
    view.rigid_mode = app.simulation.rigid_connectors();
    view.mode_label = connector_mode_label(active_draft);
    view.preset_label = overlay_preset_short_label(app.visuals.preset);
    if (diagnostics != nullptr) {
        view.diagnostics = *diagnostics;
    }
    view.dissipation_power = dissipation_power;
    if (app.visuals.show_vectors) {
        view.hint =
            "Overlay is live. Use the inspector, legend, and stage labels to track motion, load paths, and joint torques.";
    } else {
        view.hint =
            app.draft.rigid_connectors
                ? "Wheel over a bob adjusts bob mass. Wheel over connector midpoints adjusts rod mass when enabled."
                : "Wheel over a bob adjusts bob mass. Rope mode can slack and catch again as the run evolves.";
    }
    return view;
}

inline std::string format_number(double value, int precision) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

inline Color joint_torque_accent() { return {255, 116, 145, 255}; }

inline double default_field_value(const FieldSpec& spec) {
    return default_pendulum_draft().*(spec.member);
}

inline double default_visual_field_value(const VisualFieldSpec& spec) {
    return default_visual_draft().*(spec.member);
}

inline bool field_matches_default(const AppState& app, const FieldSpec& spec) {
    return std::abs((app.draft.*(spec.member)) - default_field_value(spec)) < 1e-9;
}

inline bool visual_field_matches_default(const AppState& app, const VisualFieldSpec& spec) {
    return std::abs((app.visuals.*(spec.member)) - default_visual_field_value(spec)) < 1e-9;
}

inline void reset_field_to_default(AppState& app, const FieldSpec& spec) {
    app.ui.active_field = FieldId::NONE;
    app.ui.buffer.clear();
    app.ui.active_slider = FieldId::NONE;
    app.draft.*(spec.member) = default_field_value(spec);
    clamp_draft(app.draft);
    sync_preview(app);
}

inline void reset_visual_field_to_default(AppState& app, const VisualFieldSpec& spec) {
    app.ui.active_field = FieldId::NONE;
    app.ui.buffer.clear();
    app.ui.active_slider = FieldId::NONE;
    app.visuals.*(spec.member) = default_visual_field_value(spec);
}

inline void activate_field(AppState& app, FieldId id) {
    if (const VisualFieldSpec* spec = find_visual_field_spec(id)) {
        app.ui.active_field = id;
        app.ui.buffer = format_number(app.visuals.*(spec->member), spec->precision);
        return;
    }

    if (const FieldSpec* spec = find_field_spec(id)) {
        app.ui.active_field = id;
        app.ui.buffer = format_number(app.draft.*(spec->member), spec->precision);
    }
}

inline void apply_draft_change(AppState& app) {
    clamp_draft(app.draft);
    sync_preview(app);
}

inline void commit_active_field(AppState& app) {
    if (app.ui.active_field == FieldId::NONE) {
        return;
    }

    if (const VisualFieldSpec* spec = find_visual_field_spec(app.ui.active_field)) {
        char* end = nullptr;
        const double parsed = std::strtod(app.ui.buffer.c_str(), &end);
        if (end != app.ui.buffer.c_str() && end != nullptr && *end == '\0') {
            app.visuals.*(spec->member) = std::clamp(parsed, spec->min, spec->max);
        }
    } else if (const FieldSpec* spec = find_field_spec(app.ui.active_field)) {
        char* end = nullptr;
        const double parsed = std::strtod(app.ui.buffer.c_str(), &end);
        if (end != app.ui.buffer.c_str() && end != nullptr && *end == '\0') {
            app.draft.*(spec->member) = parsed;
            apply_draft_change(app);
        }
    }

    app.ui.active_field = FieldId::NONE;
    app.ui.buffer.clear();
}

inline void prepare_toggle_interaction(AppState& app) {
    commit_active_field(app);
    app.ui.active_slider = FieldId::NONE;
}

inline void cancel_active_field(AppState& app) {
    app.ui.active_field = FieldId::NONE;
    app.ui.buffer.clear();
}

inline void handle_active_field_input(AppState& app) {
    if (app.ui.active_field == FieldId::NONE) {
        return;
    }

    int ch = GetCharPressed();
    while (ch > 0) {
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '.') {
            app.ui.buffer.push_back(static_cast<char>(ch));
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !app.ui.buffer.empty()) {
        app.ui.buffer.pop_back();
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        commit_active_field(app);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        cancel_active_field(app);
    }
}

inline bool point_in_circle(Vector2 point, Vector2 centre, float radius) {
    const float dx = point.x - centre.x;
    const float dy = point.y - centre.y;
    return dx * dx + dy * dy <= radius * radius;
}

inline int record_trail_sample(AppState& app,
                               double dt,
                               bool force = false) {
    app.trail_sample_timer += dt;
    const Vec2 position = app.simulation.bob2_pos();
    const double omega = app.simulation.omega2();
    const double sample_interval = 1.0 / 240.0;
    const double min_distance = std::max(0.0035, app.simulation.reach() * 0.0018);

    if (!force && !app.trail.empty()) {
        const Vec2 delta = position - app.trail.newest().pos;
        if (app.trail_sample_timer < sample_interval
            && delta.length_sq() < min_distance * min_distance) {
            return 0;
        }
    }

    app.trail.push(position, omega);
    app.trail_sample_timer = 0.0;
    return 1;
}

inline void launch_simulation(AppState& app, Renderer& renderer) {
    commit_active_field(app);
    clamp_draft(app.draft);
    app.applied = app.draft;
    app.simulation.reset(AppState::make_state(app.draft), AppState::make_params(app.draft));
    app.accumulator = 0.0;
    app.mode = RunMode::RUNNING;
    app.trail.clear();
    app.trail_sample_timer = 0.0;
    renderer.reset_trail();
    record_trail_sample(app, 0.0, true);
}

inline void stop_simulation(AppState& app, Renderer& renderer) {
    commit_active_field(app);
    app.mode = RunMode::STOPPED;
    app.accumulator = 0.0;
    app.trail_sample_timer = 0.0;
    app.ui.drag_handle = 0;
    app.trail.clear();
    renderer.reset_trail();
    sync_preview(app);
}

inline void clear_trail(AppState& app, Renderer& renderer) {
    app.trail.clear();
    app.trail_sample_timer = 0.0;
    renderer.reset_trail();
    if (app.mode != RunMode::STOPPED) {
        record_trail_sample(app, 0.0, true);
    }
}

inline void draw_editor_handles(const AppState& app,
                                const Renderer& renderer,
                                const PendulumLayout& layout) {
    if (app.mode != RunMode::STOPPED) {
        return;
    }

    const Vector2 b1 = renderer.to_screen(app.simulation.bob1_pos(), layout);
    const Vector2 b2 = renderer.to_screen(app.simulation.bob2_pos(), layout);
    const float r1 = renderer.bob_radius(app.draft.m1) + 8.0f;
    const float r2 = renderer.bob_radius(app.draft.m2) + 8.0f;
    const float hud_scale = canvas_overlay_scale(layout.viewport);

    DrawCircleLinesV(b1, r1, {80, 220, 255, 190});
    DrawCircleLinesV(b2, r2, {255, 186, 98, 205});

    draw_text("Drag to place the upper bob",
              {b1.x + 18.0f * hud_scale, b1.y - 30.0f * hud_scale},
              15.0f * hud_scale,
              {160, 227, 245, 220});
    draw_text("Drag to place the lower bob",
              {b2.x + 18.0f * hud_scale, b2.y + 12.0f * hud_scale},
              15.0f * hud_scale,
              {255, 214, 168, 220});

    if (app.draft.rigid_connectors && app.draft.connector_mass_enabled) {
        const Vector2 c1 = renderer.to_screen(app.simulation.connector1_com(), layout);
        const Vector2 c2 = renderer.to_screen(app.simulation.connector2_com(), layout);
        DrawCircleLinesV(c1, 11.0f, {255, 208, 122, 170});
        DrawCircleLinesV(c2, 11.0f, {255, 208, 122, 170});
    }
}

inline void handle_canvas_editing(AppState& app,
                                  const Renderer& renderer,
                                  const PendulumLayout& layout) {
    if (app.mode != RunMode::STOPPED) {
        app.ui.drag_handle = 0;
        return;
    }

    const Vector2 mouse = GetMousePosition();
    const Vector2 b1 = renderer.to_screen(app.simulation.bob1_pos(), layout);
    const Vector2 b2 = renderer.to_screen(app.simulation.bob2_pos(), layout);
    const Vector2 c1 = renderer.to_screen(app.simulation.connector1_com(), layout);
    const Vector2 c2 = renderer.to_screen(app.simulation.connector2_com(), layout);
    const float handle1 = renderer.bob_radius(app.draft.m1) + 12.0f;
    const float handle2 = renderer.bob_radius(app.draft.m2) + 12.0f;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (point_in_circle(mouse, b2, handle2)) {
            commit_active_field(app);
            app.ui.active_slider = FieldId::NONE;
            app.ui.drag_handle = 2;
        } else if (point_in_circle(mouse, b1, handle1)) {
            commit_active_field(app);
            app.ui.active_slider = FieldId::NONE;
            app.ui.drag_handle = 1;
        }
    }

    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        app.ui.drag_handle = 0;
    }

    if (app.ui.drag_handle == 1) {
        const Vec2 world = renderer.to_world(mouse, layout);
        const double radius = std::sqrt(world.x * world.x + world.y * world.y);
        app.draft.l1 = std::clamp(radius, APP_MIN_LENGTH, APP_MAX_LENGTH);
        if (radius > 1e-4) {
            app.draft.theta1_deg = std::atan2(world.x, world.y) * APP_RAD_TO_DEG;
        }
        apply_draft_change(app);
    } else if (app.ui.drag_handle == 2) {
        const Vec2 world = renderer.to_world(mouse, layout);
        const Vec2 origin = app.simulation.bob1_pos();
        const Vec2 local = {world.x - origin.x, world.y - origin.y};
        const double radius = std::sqrt(local.x * local.x + local.y * local.y);
        app.draft.l2 = std::clamp(radius, APP_MIN_LENGTH, APP_MAX_LENGTH);
        if (radius > 1e-4) {
            app.draft.theta2_deg = std::atan2(local.x, local.y) * APP_RAD_TO_DEG;
        }
        apply_draft_change(app);
    }

    if (app.ui.drag_handle == 0) {
        const double wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0) {
            if (point_in_circle(mouse, b1, handle1)) {
                app.draft.m1 = std::clamp(app.draft.m1 + wheel * 0.12, APP_MIN_MASS, APP_MAX_MASS);
                apply_draft_change(app);
            } else if (point_in_circle(mouse, b2, handle2)) {
                app.draft.m2 = std::clamp(app.draft.m2 + wheel * 0.12, APP_MIN_MASS, APP_MAX_MASS);
                apply_draft_change(app);
            } else if (app.draft.rigid_connectors && app.draft.connector_mass_enabled && point_in_circle(mouse, c1, 14.0f)) {
                app.draft.connector1_mass = std::clamp(app.draft.connector1_mass + wheel * 0.08, APP_MIN_CONNECTOR_MASS, APP_MAX_CONNECTOR_MASS);
                apply_draft_change(app);
            } else if (app.draft.rigid_connectors && app.draft.connector_mass_enabled && point_in_circle(mouse, c2, 14.0f)) {
                app.draft.connector2_mass = std::clamp(app.draft.connector2_mass + wheel * 0.08, APP_MIN_CONNECTOR_MASS, APP_MAX_CONNECTOR_MASS);
                apply_draft_change(app);
            }
        }
    }
}

inline void handle_shortcuts(AppState& app, Renderer& renderer) {
    if (app.ui.active_field != FieldId::NONE) {
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        launch_simulation(app, renderer);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        if (app.mode == RunMode::RUNNING) {
            app.mode = RunMode::PAUSED;
        } else if (app.mode == RunMode::PAUSED) {
            app.mode = RunMode::RUNNING;
        }
    }

    if (IsKeyPressed(KEY_R)) {
        stop_simulation(app, renderer);
    }

    if (IsKeyPressed(KEY_C)) {
        clear_trail(app, renderer);
    }
}

inline int step_live_simulation(AppState& app, float frame_time) {
    int new_samples = 0;
    if (app.mode == RunMode::RUNNING) {
        int step_count = 0;
        app.accumulator += std::min(frame_time, 0.033f);
        while (app.accumulator >= APP_PHYS_DT && step_count < APP_MAX_STEPS_PER_FRAME) {
            app.simulation.step(APP_PHYS_DT);
            new_samples += record_trail_sample(app, APP_PHYS_DT);
            app.accumulator -= APP_PHYS_DT;
            ++step_count;
        }
    }
    return new_samples;
}

