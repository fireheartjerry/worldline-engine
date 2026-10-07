#pragma once
// AppRuntime — canvas editing, simulation lifecycle, and per-frame stepping.
// Lower-level helpers live in AppDraft / AppOverlay / AppFieldEditing.
#include "AppFieldEditing.hpp"

inline bool point_in_circle(Vector2 point, Vector2 centre, float radius) {
    const float dx = point.x - centre.x;
    const float dy = point.y - centre.y;
    return dx * dx + dy * dy <= radius * radius;
}

// True if `point` lies on a Reference Lab HUD element: title bar, simulation
// dock, hint bar, force inspector / vector legend (when shown), or the shell's
// "< Back" key (geometry of draw_back_to_menu_button(canvas,
// canvas_overlay_scale(canvas))). Those panels are drawn over the stage, but
// canvas editing runs before the HUD, so without this check a click on a panel
// would also grab a bob lying underneath it.
inline bool point_on_canvas_hud(Vector2 point,
                                const PendulumLayout& layout,
                                bool show_vectors,
                                bool rigid_mode) {
    const CanvasOverlayRects hud = make_canvas_overlay_layout(layout.viewport, show_vectors, rigid_mode);
    const float s = hud.hud_scale;
    const Rectangle back_key{layout.viewport.x + 20.0f * s, layout.viewport.y + 20.0f * s, 160.0f * s, 36.0f * s};
    return CheckCollisionPointRec(point, hud.title_bar)
        || CheckCollisionPointRec(point, hud.sim_dock)
        || CheckCollisionPointRec(point, hud.hint_bar)
        || CheckCollisionPointRec(point, back_key)
        || (hud.show_inspector && CheckCollisionPointRec(point, hud.inspector))
        || (hud.show_legend && CheckCollisionPointRec(point, hud.legend));
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

    // Hint labels prefer the right of their bob, then its left, then centred
    // just beyond their own bob's ring (above the upper bob, below the lower
    // one): the first spot that stays on the stage (not under the vector
    // legend / off the canvas) and does not cover the other bob wins. The
    // result is clamped inside the stage so no HUD panel covers it.
    const Rectangle stage = layout.stage_rect;
    const float label_size = 15.0f * hud_scale;
    const float label_gap = 18.0f * hud_scale;
    const auto draw_handle_label = [&](const char* text, Vector2 bob, float ring, float dy, bool above,
                                       Vector2 other, float other_ring, Color color) {
        const float width = measure_ui_text(text, label_size).x;
        const float right_edge = stage.x + stage.width;
        const auto covers_other = [&](float lx, float ly) {
            const Vector2 nearest{std::clamp(other.x, lx, lx + width), std::clamp(other.y, ly, ly + label_size)};
            return point_in_circle(nearest, other, other_ring);
        };
        const auto fits = [&](float lx, float ly) {
            return lx >= stage.x && lx + width <= right_edge && !covers_other(lx, ly);
        };
        float x = bob.x + label_gap;
        float y = bob.y + dy;
        if (!fits(x, y)) {
            const float left_x = bob.x - label_gap - width;
            if (fits(left_x, y)) {
                x = left_x;
            } else {
                x = bob.x - width * 0.5f;
                y = above ? bob.y - ring - 4.0f * hud_scale - label_size : bob.y + ring + 4.0f * hud_scale;
            }
        }
        x = std::clamp(x, stage.x, std::max(stage.x, right_edge - width));
        y = std::clamp(y, stage.y, std::max(stage.y, stage.y + stage.height - label_size));
        draw_text(text, {x, y}, label_size, color);
    };
    draw_handle_label("Drag to place the upper bob", b1, r1, -30.0f * hud_scale, true, b2, r2,
                      {160, 227, 245, 220});
    draw_handle_label("Drag to place the lower bob", b2, r2, 12.0f * hud_scale, false, b1, r1,
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

    // Clicks and wheel turns over a HUD panel belong to the panel, not to a
    // bob that happens to lie underneath it.
    const bool over_hud =
        point_on_canvas_hud(mouse, layout, app.visuals.show_vectors, app.simulation.rigid_connectors());

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !over_hud) {
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

    if (app.ui.drag_handle == 0 && !over_hud) {
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

