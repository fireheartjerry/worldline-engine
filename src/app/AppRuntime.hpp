#pragma once
// AppRuntime — canvas editing, simulation lifecycle, and per-frame stepping.
// Lower-level helpers live in AppDraft / AppOverlay / AppFieldEditing.
#include "AppFieldEditing.hpp"

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

