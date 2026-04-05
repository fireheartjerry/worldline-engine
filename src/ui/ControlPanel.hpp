#pragma once
#include "UiPrimitives.hpp"
#include "../app/AppRuntime.hpp"
#include <algorithm>
#include <cmath>
#include <string>

struct ControlPanelResult {
    float max_scroll = 0.0f;
    PanelCommand command = PanelCommand::NONE;
    PanelSection toggled_section = PanelSection::COUNT;
};

inline float panel_ui_scale(Rectangle panel) {
    const float width_factor = panel.width / 430.0f;
    const float height_factor = panel.height / 860.0f;
    return std::clamp(std::min(width_factor, height_factor) + 0.12f, 1.06f, 1.28f);
}

inline std::size_t panel_section_index(PanelSection section) {
    return static_cast<std::size_t>(section);
}

inline bool section_collapsed(const AppState& app,
                              PanelSection section) {
    return app.ui.collapsed_sections[panel_section_index(section)];
}

inline float section_card_height(const AppState& app,
                                 PanelSection section,
                                 float expanded_height,
                                 float scale) {
    return section_collapsed(app, section) ? 56.0f * scale : expanded_height;
}

inline float section_note_height(const std::string& note,
                                 float card_width,
                                 float scale) {
    if (note.empty()) {
        return 0.0f;
    }
    return measure_wrapped_ui_text_height(note,
                                          card_width - 104.0f * scale,
                                          14.5f * scale,
                                          1.0f * scale);
}

inline float section_body_offset(const std::string& note,
                                 float card_width,
                                 float scale) {
    if (note.empty()) {
        return 52.0f * scale;
    }
    return 35.0f * scale + section_note_height(note, card_width, scale) + 14.0f * scale;
}

inline float checkbox_card_height(const std::string& note,
                                  float width,
                                  float scale) {
    const float note_height = measure_wrapped_ui_text_height(note,
                                                             width - 52.0f * scale,
                                                             14.0f * scale,
                                                             2.0f * scale);
    return std::max(66.0f * scale, 34.0f * scale + note_height + 12.0f * scale);
}

inline bool draw_section_toggle(Rectangle rect,
                                bool collapsed,
                                Color accent,
                                float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    DrawRectangleRounded(rect, 0.45f, 10, hot ? with_alpha(accent, 90) : Color{17, 31, 43, 220});
    DrawRectangleRoundedLines(rect, 0.45f, 10, 1.0f, hot ? with_alpha(accent, 180) : with_alpha(accent, 90));
    draw_text(collapsed ? "Show" : "Hide",
              {rect.x + 10.0f * scale, rect.y + 5.0f * scale},
              13.0f * scale,
              {232, 240, 245, 255});
    return pressed;
}

inline bool draw_section_shell(AppState& app,
                               ControlPanelResult& result,
                               PanelSection section,
                               Rectangle rect,
                               const char* title,
                               const std::string& note,
                               Color accent,
                               float scale) {
    const bool collapsed = section_collapsed(app, section);
    draw_card(rect, {7, 15, 22, 185}, {40, 72, 84, 120});
    draw_text(title, {rect.x + 16.0f * scale, rect.y + 12.0f * scale}, 22.0f * scale, {237, 243, 247, 255});
    if (draw_section_toggle({rect.x + rect.width - 76.0f * scale, rect.y + 10.0f * scale, 58.0f * scale, 24.0f * scale},
                            collapsed,
                            accent,
                            scale)) {
        result.toggled_section = section;
    }

    if (!collapsed && !note.empty()) {
        const float note_height = section_note_height(note, rect.width, scale);
        draw_text_block(note,
                        {rect.x + 16.0f * scale,
                         rect.y + 35.0f * scale,
                         rect.width - 104.0f * scale,
                         note_height + 2.0f * scale},
                        14.5f * scale,
                        {137, 177, 193, 220},
                        1.0f * scale);
    }
    return !collapsed;
}

inline bool draw_slider(AppState& app,
                        FieldId id,
                        Rectangle rect,
                        double& value,
                        double min,
                        double max,
                        Color accent,
                        float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    bool changed = false;

    if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        commit_active_field(app);
        app.ui.active_slider = id;
    }

    if (app.ui.active_slider == id) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            const double t = std::clamp((mouse.x - rect.x) / rect.width, 0.0f, 1.0f);
            value = min + (max - min) * t;
            changed = true;
        } else {
            app.ui.active_slider = FieldId::NONE;
        }
    }

    const double wheel = GetMouseWheelMove();
    if (hot && std::abs(wheel) > 0.0 && app.ui.active_slider == FieldId::NONE) {
        const double step = (max - min) / 160.0;
        value = std::clamp(value + wheel * step, min, max);
        changed = true;
        app.ui.slider_wheel_used = true;
    }

    const float t = static_cast<float>((value - min) / (max - min));
    const Rectangle track = {rect.x, rect.y + rect.height * 0.5f - 3.0f * scale, rect.width, 6.0f * scale};
    const Rectangle fill = {track.x, track.y, track.width * t, track.height};
    const Vector2 knob = {track.x + track.width * t, rect.y + rect.height * 0.5f};

    DrawRectangleRounded(track, 0.8f, 12, {28, 42, 54, 255});
    DrawRectangleRounded(fill, 0.8f, 12, with_alpha(accent, 215));
    DrawCircleV(knob, 8.5f * scale, {240, 245, 248, 255});
    DrawCircleLinesV(knob, 8.5f * scale, with_alpha(accent, 240));

    return changed;
}

inline bool draw_number_box(AppState& app,
                            Rectangle rect,
                            FieldId id,
                            double current_value,
                            int precision,
                            const char* unit,
                            Color accent,
                            float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    const bool active = app.ui.active_field == id;

    if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !active) {
        commit_active_field(app);
        activate_field(app, id);
    }

    DrawRectangleRounded(rect, 0.16f, 10, active ? Color{12, 23, 32, 255} : Color{10, 18, 26, 240});
    DrawRectangleRoundedLines(rect, 0.16f, 10, 1.4f, active ? accent : with_alpha(accent, 80));

    std::string text = active ? app.ui.buffer : format_number(current_value, precision);
    if (active && (static_cast<int>(GetTime() * 2.0) % 2 == 0)) {
        text += "_";
    }

    const Vector2 unit_size = measure_ui_text(unit, 15.0f * scale);
    const float value_size = fit_ui_text_size(text,
                                              rect.width - unit_size.x - 28.0f * scale,
                                              18.0f * scale,
                                              13.0f * scale);
    draw_text(text, {rect.x + 11.0f * scale, rect.y + 8.0f * scale}, value_size, {233, 241, 247, 255});
    draw_text(unit,
              {rect.x + rect.width - unit_size.x - 10.0f * scale, rect.y + 10.0f * scale},
              15.0f * scale,
              {114, 162, 182, 220});
    return hot;
}

inline bool draw_visual_number_box(AppState& app,
                                   Rectangle rect,
                                   const VisualFieldSpec& spec,
                                   Color accent,
                                   float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    const bool active = app.ui.active_field == spec.id;

    if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !active) {
        commit_active_field(app);
        activate_field(app, spec.id);
    }

    DrawRectangleRounded(rect, 0.16f, 10, active ? Color{12, 23, 32, 255} : Color{10, 18, 26, 240});
    DrawRectangleRoundedLines(rect, 0.16f, 10, 1.4f, active ? accent : with_alpha(accent, 80));

    std::string text = active ? app.ui.buffer : format_number(app.visuals.*(spec.member), spec.precision);
    if (active && (static_cast<int>(GetTime() * 2.0) % 2 == 0)) {
        text += "_";
    }

    const Vector2 unit_size = measure_ui_text(spec.unit, 15.0f * scale);
    const float value_size = fit_ui_text_size(text,
                                              rect.width - unit_size.x - 28.0f * scale,
                                              18.0f * scale,
                                              13.0f * scale);
    draw_text(text, {rect.x + 11.0f * scale, rect.y + 8.0f * scale}, value_size, {233, 241, 247, 255});
    draw_text(spec.unit,
              {rect.x + rect.width - unit_size.x - 10.0f * scale, rect.y + 10.0f * scale},
              15.0f * scale,
              {114, 162, 182, 220});
    return hot;
}

inline bool draw_parameter_row(AppState& app,
                               Rectangle row,
                               const FieldSpec& spec,
                               Color accent,
                               float scale) {
    double& value = app.draft.*(spec.member);

    draw_text(spec.label, {row.x, row.y}, 18.0f * scale, {226, 237, 244, 255});

    Rectangle slider = {row.x, row.y + 22.0f * scale, row.width - 124.0f * scale, 18.0f * scale};
    Rectangle box = {row.x + row.width - 110.0f * scale, row.y + 1.0f * scale, 110.0f * scale, 36.0f * scale};

    const bool changed = draw_slider(app, spec.id, slider, value, spec.min, spec.max, accent, scale);
    draw_number_box(app, box, spec.id, value, spec.precision, spec.unit, accent, scale);
    return changed;
}

inline bool draw_visual_row(AppState& app,
                            Rectangle row,
                            const VisualFieldSpec& spec,
                            Color accent,
                            float scale) {
    double& value = app.visuals.*(spec.member);

    draw_text(spec.label, {row.x, row.y}, 18.0f * scale, {226, 237, 244, 255});

    Rectangle slider = {row.x, row.y + 22.0f * scale, row.width - 124.0f * scale, 18.0f * scale};
    Rectangle box = {row.x + row.width - 110.0f * scale, row.y + 1.0f * scale, 110.0f * scale, 36.0f * scale};

    const bool changed = draw_slider(app, spec.id, slider, value, spec.min, spec.max, accent, scale);
    draw_visual_number_box(app, box, spec, accent, scale);
    return changed;
}

inline bool draw_preset_button(Rectangle rect,
                               OverlayPreset current,
                               OverlayPreset preset,
                               float scale) {
    const bool active = current == preset;
    return draw_button(rect,
                       overlay_preset_label(preset),
                       active ? Color{18, 88, 92, 235} : Color{19, 37, 53, 230},
                       active ? Color{28, 118, 122, 255} : Color{28, 55, 76, 255},
                       active ? Color{234, 248, 248, 255} : Color{220, 232, 241, 255},
                       true,
                       scale);
}

inline bool draw_jump_button(Rectangle rect,
                             const char* label,
                             Color accent,
                             float scale) {
    return draw_button(rect,
                       label,
                       with_alpha(accent, 72),
                       with_alpha(accent, 112),
                       {232, 241, 246, 255},
                       true,
                       scale);
}

inline void handle_control_panel_wheel_scroll(AppState& app,
                                              Rectangle panel,
                                              float max_panel_scroll) {
    if (!app.ui.slider_wheel_used && app.ui.active_field == FieldId::NONE) {
        const Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, panel)) {
            const float wheel = GetMouseWheelMove();
            if (std::abs(wheel) > 0.001f) {
                app.ui.panel_scroll = std::clamp(
                    app.ui.panel_scroll - wheel * 40.0f,
                    0.0f,
                    max_panel_scroll);
            }
        }
    }
}

inline ControlPanelResult draw_control_panel(AppState& app,
                                             Rectangle panel) {
    app.ui.slider_wheel_used = false;
    ControlPanelResult result;
    const float ui = panel_ui_scale(panel);

    const bool pending_restart = app.mode != RunMode::STOPPED && !drafts_equal(app.draft, app.applied);
    const bool show_connector_masses = app.draft.rigid_connectors && app.draft.connector_mass_enabled;

    const std::string nav_note = "Jump straight to the model area you want to tune.";
    const std::string actions_note = "Launch, pause, stop, or wipe the after-image trail.";
    const std::string connector_note =
        "Choose rigid rods or ideal ropes, and optionally give the rods distributed mass.";
    const std::string geometry_note = "Drag the stage handles or fine-tune the numbers.";
    const std::string launch_note = "Initial angles and spin. Enter restarts from these values.";
    const std::string bob_drag_note = "Per-bob viscous and air-drag coefficients.";
    const std::string link_drag_note =
        "Axial terms act along the link. Normal terms resist cross-flow and usually dominate.";
    const std::string joints_note =
        "Pivot and elbow damping or dry friction. These only act in rigid mode.";
    const std::string flow_note =
        "Uniform wind, linear shear, and swirl all feed the drag model through relative fluid velocity.";
    const std::string visuals_note =
        "Presets let you switch between presentation and debugging views without re-toggling every overlay.";
    const std::string metrics_note =
        "A live force-and-power readout for the current frame.";

    const float card_width = panel.width - 36.0f * ui;
    const float nav_body_offset = section_body_offset(nav_note, card_width, ui);
    const float actions_body_offset = section_body_offset(actions_note, card_width, ui);
    const float connector_body_offset = section_body_offset(connector_note, card_width, ui);
    const float geometry_body_offset = section_body_offset(geometry_note, card_width, ui);
    const float launch_body_offset = section_body_offset(launch_note, card_width, ui);
    const float bob_drag_body_offset = section_body_offset(bob_drag_note, card_width, ui);
    const float link_drag_body_offset = section_body_offset(link_drag_note, card_width, ui);
    const float joints_body_offset = section_body_offset(joints_note, card_width, ui);
    const float flow_body_offset = section_body_offset(flow_note, card_width, ui);
    const float visuals_body_offset = section_body_offset(visuals_note, card_width, ui);
    const float metrics_body_offset = section_body_offset(metrics_note, card_width, ui);

    const float jump_button_height = 34.0f * ui;
    const float jump_row_gap = 10.0f * ui;
    const float action_button_height = 36.0f * ui;
    const float action_row_gap = 10.0f * ui;
    const float connector_check_width = card_width - 28.0f * ui;
    const float rigid_checkbox_height = checkbox_card_height(
        "Rod constraints. Toggle off for massless ropes with slack/re-catch.",
        connector_check_width,
        ui);
    const float mass_checkbox_height = checkbox_card_height(
        "Treat each rod as a uniform rigid body instead of ideal massless links.",
        connector_check_width,
        ui);

    const float nav_height = section_card_height(app,
                                                 PanelSection::NAV,
                                                 nav_body_offset + jump_button_height * 3.0f + jump_row_gap * 2.0f + 18.0f * ui,
                                                 ui);
    const float actions_height = section_card_height(app,
                                                     PanelSection::CONTROLS,
                                                     actions_body_offset + action_button_height * 2.0f + action_row_gap + 18.0f * ui,
                                                     ui);
    float connector_expanded_height =
        connector_body_offset + rigid_checkbox_height + 14.0f * ui;
    if (app.draft.rigid_connectors) {
        connector_expanded_height += mass_checkbox_height + 12.0f * ui;
    }
    if (show_connector_masses) {
        connector_expanded_height += static_cast<float>(CONNECTOR_FIELDS.size()) * 40.0f * ui + 10.0f * ui;
    }
    connector_expanded_height += 14.0f * ui;
    const float connector_height = section_card_height(app,
                                                       PanelSection::CONNECTOR,
                                                       connector_expanded_height,
                                                       ui);
    const float geometry_height = section_card_height(app,
                                                      PanelSection::GEOMETRY,
                                                      geometry_body_offset + static_cast<float>(GEOMETRY_FIELDS.size()) * 38.0f * ui + 20.0f * ui,
                                                      ui);
    const float launch_height = section_card_height(app,
                                                    PanelSection::LAUNCH,
                                                    launch_body_offset + static_cast<float>(LAUNCH_FIELDS.size()) * 38.0f * ui + 20.0f * ui,
                                                    ui);
    const float bob_drag_height = section_card_height(app,
                                                      PanelSection::BOB_DRAG,
                                                      bob_drag_body_offset + static_cast<float>(BOB_RESISTANCE_FIELDS.size()) * 38.0f * ui + 20.0f * ui,
                                                      ui);
    const float connector_drag_height = section_card_height(app,
                                                            PanelSection::LINK_DRAG,
                                                            link_drag_body_offset + static_cast<float>(CONNECTOR_DRAG_FIELDS.size()) * 38.0f * ui + 22.0f * ui,
                                                            ui);
    const float joints_height = section_card_height(app,
                                                    PanelSection::JOINTS,
                                                    joints_body_offset + static_cast<float>(JOINT_RESISTANCE_FIELDS.size()) * 38.0f * ui
                                                        + (!app.draft.rigid_connectors ? 34.0f * ui : 18.0f * ui),
                                                    ui);
    const float flow_height = section_card_height(app,
                                                  PanelSection::FLOW,
                                                  flow_body_offset + static_cast<float>(FLOW_FIELDS.size()) * 38.0f * ui + 58.0f * ui,
                                                  ui);
    const float visuals_height = section_card_height(app, PanelSection::VISUALS, 690.0f * ui, ui);
    const float metrics_height = section_card_height(app,
                                                     PanelSection::METRICS,
                                                     metrics_body_offset + 350.0f * ui,
                                                     ui);
    const float card_gap = 14.0f * ui;
    const float badge_height = pending_restart ? 64.0f * ui : 46.0f * ui;

    float content_y = 18.0f * ui;
    const float title_offset = content_y;
    content_y += 38.0f * ui;
    const float subtitle_offset = content_y;
    content_y += 30.0f * ui;
    const float badge_offset = content_y;
    content_y += badge_height;
    const float nav_offset = content_y;
    content_y += nav_height;
    const float actions_offset = content_y;
    content_y += actions_height + card_gap;
    const float connector_offset = content_y;
    content_y += connector_height + card_gap;
    const float geometry_offset = content_y;
    content_y += geometry_height + card_gap;
    const float launch_offset = content_y;
    content_y += launch_height + card_gap;
    const float bob_drag_offset = content_y;
    content_y += bob_drag_height + card_gap;
    const float connector_drag_offset = content_y;
    content_y += connector_drag_height + card_gap;
    const float joints_offset = content_y;
    content_y += joints_height + card_gap;
    const float flow_offset = content_y;
    content_y += flow_height + card_gap;
    const float visuals_offset = content_y;
    content_y += visuals_height + card_gap;
    const float metrics_offset = content_y;
    content_y += metrics_height + 18.0f * ui;

    const float max_panel_scroll = std::max(0.0f, content_y - panel.height);
    app.ui.panel_scroll = std::clamp(app.ui.panel_scroll, 0.0f, max_panel_scroll);

    if (app.ui.active_field == FieldId::NONE) {
        if (IsKeyDown(KEY_DOWN)) {
            app.ui.panel_scroll = std::clamp(app.ui.panel_scroll + 5.0f * ui, 0.0f, max_panel_scroll);
        }
        if (IsKeyDown(KEY_UP)) {
            app.ui.panel_scroll = std::clamp(app.ui.panel_scroll - 5.0f * ui, 0.0f, max_panel_scroll);
        }
    }

    auto panel_y = [&](float offset) {
        return panel.y + offset - app.ui.panel_scroll;
    };
    auto jump_to = [&](float offset) {
        prepare_toggle_interaction(app);
        app.ui.panel_scroll = std::clamp(offset - 10.0f * ui, 0.0f, max_panel_scroll);
    };

    draw_card(panel, {6, 11, 18, 220}, {44, 79, 94, 140});

    BeginScissorMode(static_cast<int>(panel.x),
                     static_cast<int>(panel.y),
                     static_cast<int>(panel.width),
                     static_cast<int>(panel.height));

    draw_text("Pendulum Lab", {panel.x + 20.0f * ui, panel_y(title_offset)}, 32.0f * ui, {238, 244, 248, 255});
    draw_text("Pick a connector model, shape the launch, then run it.",
              {panel.x + 20.0f * ui, panel_y(subtitle_offset)},
              16.0f * ui,
              {132, 173, 191, 220});

    const float badge_y = panel_y(badge_offset);
    if (app.mode == RunMode::RUNNING) {
        draw_badge({panel.x + 20.0f * ui, badge_y, 102.0f * ui, 30.0f * ui}, "LIVE", {16, 82, 70, 220}, {216, 251, 243, 255}, ui);
    } else if (app.mode == RunMode::PAUSED) {
        draw_badge({panel.x + 20.0f * ui, badge_y, 112.0f * ui, 30.0f * ui}, "PAUSED", {92, 78, 20, 220}, {255, 243, 205, 255}, ui);
    } else {
        draw_badge({panel.x + 20.0f * ui, badge_y, 122.0f * ui, 30.0f * ui}, "EDIT MODE", {17, 68, 98, 220}, {220, 239, 255, 255}, ui);
    }
    draw_badge({panel.x + 152.0f * ui, badge_y, 162.0f * ui, 30.0f * ui}, connector_mode_label(app.draft), {34, 51, 67, 210}, {223, 235, 243, 255}, ui);
    if (pending_restart) {
        draw_text("Edited settings are staged for the next restart.",
                  {panel.x + 20.0f * ui, badge_y + 36.0f * ui},
                  15.0f * ui,
                  {255, 202, 186, 220});
    }

    Rectangle nav_card = {panel.x + 18.0f * ui, panel_y(nav_offset), panel.width - 36.0f * ui, nav_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::NAV,
                           nav_card,
                           "Quick Jump",
                           nav_note,
                           {83, 220, 255, 255},
                           ui)) {
        const float jump_gap = 8.0f * ui;
        const float jump_width = (nav_card.width - 32.0f * ui - jump_gap) * 0.5f;
        const float jump_row1 = nav_card.y + nav_body_offset;
        const float jump_row2 = jump_row1 + jump_button_height + jump_row_gap;
        const float jump_row3 = jump_row2 + jump_button_height + jump_row_gap;
        if (draw_jump_button({nav_card.x + 16.0f * ui, jump_row1, jump_width, jump_button_height}, "Setup", {83, 220, 255, 255}, ui)) {
            jump_to(connector_offset);
        }
        if (draw_jump_button({nav_card.x + 16.0f * ui + jump_width + jump_gap, jump_row1, jump_width, jump_button_height}, "Launch", {255, 188, 92, 255}, ui)) {
            jump_to(launch_offset);
        }
        if (draw_jump_button({nav_card.x + 16.0f * ui, jump_row2, jump_width, jump_button_height}, "Loads", {255, 159, 102, 255}, ui)) {
            jump_to(bob_drag_offset);
        }
        if (draw_jump_button({nav_card.x + 16.0f * ui + jump_width + jump_gap, jump_row2, jump_width, jump_button_height}, "Flow", {112, 224, 255, 255}, ui)) {
            jump_to(flow_offset);
        }
        if (draw_jump_button({nav_card.x + 16.0f * ui, jump_row3, jump_width, jump_button_height}, "View", {194, 127, 255, 255}, ui)) {
            jump_to(visuals_offset);
        }
        if (draw_jump_button({nav_card.x + 16.0f * ui + jump_width + jump_gap, jump_row3, jump_width, jump_button_height}, "Readout", {255, 110, 191, 255}, ui)) {
            jump_to(metrics_offset);
        }
    }

    Rectangle actions = {panel.x + 18.0f * ui, panel_y(actions_offset), panel.width - 36.0f * ui, actions_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::CONTROLS,
                           actions,
                           "Controls",
                           actions_note,
                           {16, 88, 92, 255},
                           ui)) {
        const float button_gap = 10.0f * ui;
        const float button_width = (actions.width - button_gap * 3.0f) * 0.5f;
        const float action_row1 = actions.y + actions_body_offset;
        const float action_row2 = action_row1 + action_button_height + action_row_gap;
        if (draw_button({actions.x + button_gap, action_row1, button_width, action_button_height},
                        app.mode == RunMode::STOPPED ? "Launch" : "Restart",
                        {16, 88, 92, 235}, {24, 118, 122, 255}, {228, 249, 248, 255}, true, ui)) {
            result.command = PanelCommand::LAUNCH;
        }
        if (draw_button({actions.x + button_gap * 2.0f + button_width, action_row1, button_width, action_button_height},
                        app.mode == RunMode::PAUSED ? "Resume" : "Pause",
                        {33, 50, 72, 235}, {46, 67, 94, 255}, {228, 235, 246, 255},
                        app.mode != RunMode::STOPPED,
                        ui)) {
            result.command = PanelCommand::TOGGLE_PAUSE;
        }
        if (draw_button({actions.x + button_gap, action_row2, button_width, action_button_height},
                        "Stop",
                        {92, 38, 28, 230}, {118, 49, 36, 255}, {255, 233, 225, 255},
                        app.mode != RunMode::STOPPED,
                        ui)) {
            result.command = PanelCommand::STOP;
        }
        if (draw_button({actions.x + button_gap * 2.0f + button_width, action_row2, button_width, action_button_height},
                        "Clear Trail",
                        {19, 37, 53, 230}, {28, 55, 76, 255}, {228, 235, 246, 255}, true, ui)) {
            result.command = PanelCommand::CLEAR_TRAIL;
        }
    }

    Rectangle connector_card = {panel.x + 18.0f * ui, panel_y(connector_offset), panel.width - 36.0f * ui, connector_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::CONNECTOR,
                           connector_card,
                           "Connector Behavior",
                           connector_note,
                           {255, 200, 118, 255},
                           ui)) {
        float connector_y = connector_card.y + connector_body_offset;
        if (draw_checkbox({connector_card.x + 14.0f * ui, connector_y, connector_card.width - 28.0f * ui, rigid_checkbox_height},
                          app.draft.rigid_connectors,
                          "Rigid connectors",
                          "Rod constraints. Toggle off for massless ropes with slack/re-catch.",
                          {87, 220, 205, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.draft.rigid_connectors = !app.draft.rigid_connectors;
            if (!app.draft.rigid_connectors) {
                app.draft.connector_mass_enabled = false;
            }
            apply_draft_change(app);
        }

        if (app.draft.rigid_connectors) {
            connector_y += rigid_checkbox_height + 12.0f * ui;
            if (draw_checkbox({connector_card.x + 14.0f * ui, connector_y, connector_card.width - 28.0f * ui, mass_checkbox_height},
                              app.draft.connector_mass_enabled,
                              "Connector mass enabled",
                              "Treat each rod as a uniform rigid body instead of ideal massless links.",
                              {255, 200, 118, 255},
                              ui)) {
                prepare_toggle_interaction(app);
                app.draft.connector_mass_enabled = !app.draft.connector_mass_enabled;
                if (app.draft.connector_mass_enabled && app.draft.connector1_mass < 1e-6 && app.draft.connector2_mass < 1e-6) {
                    app.draft.connector1_mass = 0.35;
                    app.draft.connector2_mass = 0.25;
                }
                apply_draft_change(app);
            }
        }

        if (show_connector_masses) {
            connector_y += mass_checkbox_height + 14.0f * ui;
            float row_y = connector_y;
            for (const FieldSpec& spec : CONNECTOR_FIELDS) {
                if (draw_parameter_row(app, {connector_card.x + 16.0f * ui, row_y, connector_card.width - 32.0f * ui, 42.0f * ui}, spec, {255, 200, 118, 255}, ui)) {
                    apply_draft_change(app);
                }
                row_y += 40.0f * ui;
            }
        }
    }

    Rectangle geometry = {panel.x + 18.0f * ui, panel_y(geometry_offset), panel.width - 36.0f * ui, geometry_height};
    float row_y = 0.0f;
    if (draw_section_shell(app,
                           result,
                           PanelSection::GEOMETRY,
                           geometry,
                           "Stage Geometry",
                           geometry_note,
                           {83, 220, 255, 255},
                           ui)) {
        row_y = geometry.y + geometry_body_offset;
        for (const FieldSpec& spec : GEOMETRY_FIELDS) {
            if (draw_parameter_row(app, {geometry.x + 16.0f * ui, row_y, geometry.width - 32.0f * ui, 42.0f * ui}, spec, {83, 220, 255, 255}, ui)) {
                apply_draft_change(app);
            }
            row_y += 38.0f * ui;
        }
    }

    Rectangle launch = {panel.x + 18.0f * ui, panel_y(launch_offset), panel.width - 36.0f * ui, launch_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::LAUNCH,
                           launch,
                           "Launch State",
                           launch_note,
                           {255, 188, 92, 255},
                           ui)) {
        row_y = launch.y + launch_body_offset;
        for (const FieldSpec& spec : LAUNCH_FIELDS) {
            if (draw_parameter_row(app, {launch.x + 16.0f * ui, row_y, launch.width - 32.0f * ui, 42.0f * ui}, spec, {255, 188, 92, 255}, ui)) {
                apply_draft_change(app);
            }
            row_y += 38.0f * ui;
        }
    }

    Rectangle bob_drag = {panel.x + 18.0f * ui, panel_y(bob_drag_offset), panel.width - 36.0f * ui, bob_drag_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::BOB_DRAG,
                           bob_drag,
                           "Bob Drag",
                           bob_drag_note,
                           {180, 151, 255, 255},
                           ui)) {
        row_y = bob_drag.y + bob_drag_body_offset;
        for (const FieldSpec& spec : BOB_RESISTANCE_FIELDS) {
            if (draw_parameter_row(app, {bob_drag.x + 16.0f * ui, row_y, bob_drag.width - 32.0f * ui, 42.0f * ui}, spec, {180, 151, 255, 255}, ui)) {
                apply_draft_change(app);
            }
            row_y += 38.0f * ui;
        }
    }

    Rectangle connector_drag = {panel.x + 18.0f * ui, panel_y(connector_drag_offset), panel.width - 36.0f * ui, connector_drag_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::LINK_DRAG,
                           connector_drag,
                           "Link Drag",
                           link_drag_note,
                           {112, 203, 255, 255},
                           ui)) {
        row_y = connector_drag.y + link_drag_body_offset + 6.0f * ui;
        for (const FieldSpec& spec : CONNECTOR_DRAG_FIELDS) {
            if (draw_parameter_row(app, {connector_drag.x + 16.0f * ui, row_y, connector_drag.width - 32.0f * ui, 42.0f * ui}, spec, {112, 203, 255, 255}, ui)) {
                apply_draft_change(app);
            }
            row_y += 38.0f * ui;
        }
    }

    Rectangle joints = {panel.x + 18.0f * ui, panel_y(joints_offset), panel.width - 36.0f * ui, joints_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::JOINTS,
                           joints,
                           "Joint Resistance",
                           joints_note,
                           {255, 169, 121, 255},
                           ui)) {
        row_y = joints.y + joints_body_offset;
        for (const FieldSpec& spec : JOINT_RESISTANCE_FIELDS) {
            if (draw_parameter_row(app, {joints.x + 16.0f * ui, row_y, joints.width - 32.0f * ui, 42.0f * ui}, spec, {255, 169, 121, 255}, ui)) {
                apply_draft_change(app);
            }
            row_y += 38.0f * ui;
        }
        if (!app.draft.rigid_connectors) {
            draw_text("Joint torques are stored but ignored while the model is using ropes.",
                      {joints.x + 16.0f * ui, joints.y + joints.height - 22.0f * ui},
                      14.0f * ui,
                      {238, 196, 171, 220});
        }
    }

    Rectangle flow = {panel.x + 18.0f * ui, panel_y(flow_offset), panel.width - 36.0f * ui, flow_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::FLOW,
                           flow,
                           "Ambient Flow",
                           flow_note,
                           {112, 224, 255, 255},
                           ui)) {
        draw_badge({flow.x + flow.width - 104.0f * ui, flow.y + 12.0f * ui, 88.0f * ui, 24.0f * ui},
                   app.simulation.ambient_flow_enabled() ? "ACTIVE" : "STILL",
                   app.simulation.ambient_flow_enabled() ? Color{18, 88, 92, 220} : Color{34, 51, 67, 210},
                   {231, 243, 248, 255},
                   ui);

        row_y = flow.y + flow_body_offset + 6.0f * ui;
        for (const FieldSpec& spec : FLOW_FIELDS) {
            if (draw_parameter_row(app, {flow.x + 16.0f * ui, row_y, flow.width - 32.0f * ui, 42.0f * ui}, spec, {112, 224, 255, 255}, ui)) {
                apply_draft_change(app);
            }
            row_y += 38.0f * ui;
        }
        draw_text_block("Model: u(x, y) = wind + shear + swirl about the pivot. Drag always follows body velocity relative to this field.",
                        {flow.x + 16.0f * ui, flow.y + flow.height - 44.0f * ui, flow.width - 32.0f * ui, 34.0f * ui},
                        13.5f * ui,
                        {168, 198, 212, 220},
                        1.0f * ui);
    }

    Rectangle visuals = {panel.x + 18.0f * ui, panel_y(visuals_offset), panel.width - 36.0f * ui, visuals_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::VISUALS,
                           visuals,
                           "Field Visuals",
                           visuals_note,
                           {194, 127, 255, 255},
                           ui)) {
        const float preset_gap = 10.0f * ui;
        const float preset_width = (visuals.width - 32.0f * ui - preset_gap) * 0.5f;
        float preset_y = visuals.y + visuals_body_offset + 6.0f * ui;
        if (draw_preset_button({visuals.x + 16.0f * ui, preset_y, preset_width, 34.0f * ui}, app.visuals.preset, OverlayPreset::CLEAN, ui)) {
            prepare_toggle_interaction(app);
            apply_overlay_preset(app.visuals, OverlayPreset::CLEAN);
        }
        if (draw_preset_button({visuals.x + 16.0f * ui + preset_width + preset_gap, preset_y, preset_width, 34.0f * ui}, app.visuals.preset, OverlayPreset::FORCES, ui)) {
            prepare_toggle_interaction(app);
            apply_overlay_preset(app.visuals, OverlayPreset::FORCES);
        }
        preset_y += 40.0f * ui;
        if (draw_preset_button({visuals.x + 16.0f * ui, preset_y, preset_width, 34.0f * ui}, app.visuals.preset, OverlayPreset::RESISTANCE, ui)) {
            prepare_toggle_interaction(app);
            apply_overlay_preset(app.visuals, OverlayPreset::RESISTANCE);
        }
        if (draw_preset_button({visuals.x + 16.0f * ui + preset_width + preset_gap, preset_y, preset_width, 34.0f * ui}, app.visuals.preset, OverlayPreset::FULL, ui)) {
            prepare_toggle_interaction(app);
            apply_overlay_preset(app.visuals, OverlayPreset::FULL);
        }

        row_y = preset_y + 48.0f * ui;
        for (const VisualFieldSpec& spec : VISUAL_FIELDS) {
            if (draw_visual_row(app, {visuals.x + 16.0f * ui, row_y, visuals.width - 32.0f * ui, 42.0f * ui}, spec, {194, 127, 255, 255}, ui)) {
                app.visuals.*(spec.member) = std::clamp(app.visuals.*(spec.member), spec.min, spec.max);
            }
            row_y += 38.0f * ui;
        }

        const float check_y = row_y + 10.0f * ui;
        const float check_w = (visuals.width - 40.0f * ui - 10.0f * ui) * 0.5f;
        const float left_x = visuals.x + 16.0f * ui;
        const float right_x = left_x + check_w + 10.0f * ui;

        if (draw_checkbox({left_x, check_y, visuals.width - 32.0f * ui, 58.0f * ui},
                          app.visuals.show_vectors,
                          "Vector field overlay",
                          "Master switch for arrows, streamlines, torque rings, the inspector, and the legend.",
                          {212, 146, 255, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_vectors = !app.visuals.show_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }

        float check_row = check_y + 64.0f * ui;
        if (draw_checkbox({left_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_velocity_vectors,
                          "Velocity",
                          "Motion direction and speed for both bobs.",
                          {102, 232, 255, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_velocity_vectors = !app.visuals.show_velocity_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }
        if (draw_checkbox({right_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_gravity_vectors,
                          "Gravity",
                          "Weight acting on each bob.",
                          {112, 142, 255, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_gravity_vectors = !app.visuals.show_gravity_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }

        check_row += 64.0f * ui;
        if (draw_checkbox({left_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_drag_vectors,
                          "Bob Drag",
                          "Direct drag on each bob body.",
                          {255, 159, 102, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_drag_vectors = !app.visuals.show_drag_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }
        if (draw_checkbox({right_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_link_drag_vectors,
                          "Link Drag",
                          "Distributed drag resultants on rods or taut ropes.",
                          {255, 214, 120, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_link_drag_vectors = !app.visuals.show_link_drag_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }

        check_row += 64.0f * ui;
        if (draw_checkbox({left_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_reaction_vectors,
                          "Reaction",
                          "Constraint or tension result acting on each bob.",
                          {140, 247, 182, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_reaction_vectors = !app.visuals.show_reaction_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }
        if (draw_checkbox({right_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_net_vectors,
                          "Net Force",
                          "Resultant force after all loads are combined.",
                          {255, 110, 191, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_net_vectors = !app.visuals.show_net_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }

        check_row += 64.0f * ui;
        if (draw_checkbox({left_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_joint_torque_vectors,
                          "Joint Torque",
                          "Pivot and elbow resistance torques, shown as circular load indicators in rigid mode.",
                          joint_torque_accent(),
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_joint_torque_vectors = !app.visuals.show_joint_torque_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }
        if (draw_checkbox({right_x, check_row, check_w, 58.0f * ui},
                          app.visuals.show_flow_field,
                          "Flow Field",
                          "Draw ambient-flow streamlines and use the inspector to compare wind and body speeds.",
                          {112, 224, 255, 255},
                          ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_flow_field = !app.visuals.show_flow_field;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }
    }

    Rectangle metrics = {panel.x + 18.0f * ui, panel_y(metrics_offset), panel.width - 36.0f * ui, metrics_height};
    if (draw_section_shell(app,
                           result,
                           PanelSection::METRICS,
                           metrics,
                           "Live Readout",
                           metrics_note,
                           {255, 110, 191, 255},
                           ui)) {

        const double energy = app.simulation.energy();
        const double dissipation_power = app.simulation.dissipation_power();
        const double reach = app.simulation.reach();
        const double angle1 = app.simulation.theta1() * APP_RAD_TO_DEG;
        const double angle2 = app.simulation.theta2() * APP_RAD_TO_DEG;
        const std::string upper_link = app.simulation.connector1_taut() ? "Taut" : "Slack";
        const std::string lower_link = app.simulation.connector2_taut() ? "Taut" : "Slack";
        const bool resistance_enabled = app.simulation.resistance_enabled();
        const bool flow_enabled = app.simulation.ambient_flow_enabled();
        const float metric_y = metrics.y + metrics_body_offset;

        draw_metric({metrics.x + 16.0f * ui, metric_y, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Total Reach", format_number(reach, 2) + " m", ui);
        draw_metric({metrics.x + metrics.width * 0.5f + 6.0f * ui, metric_y, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Energy", format_number(energy, 3), ui);
        draw_metric({metrics.x + 16.0f * ui, metric_y + 76.0f * ui, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Upper Angle", format_number(angle1, 1) + " deg", ui);
        draw_metric({metrics.x + metrics.width * 0.5f + 6.0f * ui, metric_y + 76.0f * ui, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Lower Angle", format_number(angle2, 1) + " deg", ui);
        draw_metric({metrics.x + 16.0f * ui, metric_y + 152.0f * ui, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Upper Link", upper_link, ui);
        draw_metric({metrics.x + metrics.width * 0.5f + 6.0f * ui, metric_y + 152.0f * ui, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Lower Link", lower_link, ui);
        draw_metric({metrics.x + 16.0f * ui, metric_y + 228.0f * ui, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Power", format_number(dissipation_power, 3) + " W", ui);
        draw_metric({metrics.x + metrics.width * 0.5f + 6.0f * ui, metric_y + 228.0f * ui, metrics.width * 0.5f - 22.0f * ui, 68.0f * ui}, "Flow", flow_enabled ? "Active" : "Still", ui);

        const char* metrics_note =
            flow_enabled
                ? "Moving fluid can add or remove pendulum energy depending on relative motion.\nUse the flow overlay and inspector to see where drag is being driven from."
                : (resistance_enabled
                    ? "Resistance is active, so mechanical energy should usually decay over time.\nLink drag is distributed along rods or taut rope segments."
                    : "Dynamic framing always scales the whole mechanism into view.\nRope mode is unilateral: tension only acts while the rope stays taut.");
        draw_text_block(metrics_note,
                        {metrics.x + 16.0f * ui, metric_y + 308.0f * ui, metrics.width - 32.0f * ui, 64.0f * ui},
                        14.5f * ui,
                        {160, 194, 208, 220},
                        2.0f * ui);
    }

    EndScissorMode();

    if (max_panel_scroll > 0.0f) {
        const float track_h = panel.height - 8.0f;
        const float thumb_h = std::max(20.0f, track_h * (panel.height / (panel.height + max_panel_scroll)));
        const float thumb_frac = (max_panel_scroll > 0.0f) ? app.ui.panel_scroll / max_panel_scroll : 0.0f;
        const float thumb_y = panel.y + 4.0f + (track_h - thumb_h) * thumb_frac;
        DrawRectangleRounded({panel.x + panel.width - 7.0f, panel.y + 4.0f, 4.0f, track_h}, 1.0f, 6, {30, 55, 68, 100});
        DrawRectangleRounded({panel.x + panel.width - 7.0f, thumb_y, 4.0f, thumb_h}, 1.0f, 6, {80, 130, 155, 190});
    }

    result.max_scroll = max_panel_scroll;
    return result;
}
