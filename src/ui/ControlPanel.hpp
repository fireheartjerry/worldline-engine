#pragma once
#include "UiPrimitives.hpp"
#include "../app/AppRuntime.hpp"
#include <algorithm>
#include <cmath>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Control Panel  — Tuning Studio
//  Visual language: sharp-cornered glassy cards, neon accent stripes, glow
//  knobs, diamond toggles.  Every section has a unique accent colour that
//  matches the HUD diagnostic palette so the two panels stay coherent.
// ─────────────────────────────────────────────────────────────────────────────

struct ControlPanelResult {
    float max_scroll             = 0.0f;
    PanelCommand command         = PanelCommand::NONE;
    PanelSection toggled_section = PanelSection::COUNT;
};

inline float panel_ui_scale(Rectangle panel) {
    const float wf = panel.width  / 430.0f;
    const float hf = panel.height / 860.0f;
    return std::clamp(std::min(wf, hf) + 0.12f, 1.06f, 1.28f);
}

inline std::size_t panel_section_index(PanelSection section) {
    return static_cast<std::size_t>(section);
}

inline bool section_collapsed(const AppState& app, PanelSection section) {
    return app.ui.collapsed_sections[panel_section_index(section)];
}

inline float section_card_height(const AppState& app, PanelSection section,
                                 float expanded_height, float scale) {
    return section_collapsed(app, section) ? 52.0f * scale : expanded_height;
}

inline float section_note_height(const std::string& note, float card_width, float scale) {
    if (note.empty()) return 0.0f;
    return measure_wrapped_ui_text_height(note, card_width - 100.0f * scale, 13.5f * scale, 1.0f * scale);
}

inline float section_body_offset(const std::string& note, float card_width, float scale) {
    if (note.empty()) return 50.0f * scale;
    return 33.0f * scale + section_note_height(note, card_width, scale) + 12.0f * scale;
}

inline float checkbox_card_height(const std::string& note, float width, float scale) {
    const float nh = measure_wrapped_ui_text_height(note,
                                                    width - 50.0f * scale,
                                                    13.5f * scale, 2.0f * scale);
    return std::max(62.0f * scale, 32.0f * scale + nh + 12.0f * scale);
}

// ── Section header toggle (Show / Hide) ───────────────────────────────────────
inline bool draw_section_toggle(Rectangle rect, bool collapsed, Color accent, float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot     = CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawRectangleRounded(rect, 0.38f, 8,
                         hot ? with_alpha(accent, 70) : Color{12, 24, 36, 210});
    DrawRectangleRoundedLines(rect, 0.38f, 8, 1.0f,
                              hot ? with_alpha(accent, 160) : with_alpha(accent, 70));

    const char* lbl = collapsed ? "SHOW" : "HIDE";
    const float ts = 11.0f * scale;
    const Vector2 tsz = measure_ui_text(lbl, ts);
    draw_text(lbl,
              {rect.x + (rect.width - tsz.x) * 0.5f,
               rect.y + (rect.height - tsz.y) * 0.5f},
              ts, with_alpha(accent, 230));
    return pressed;
}

// ── Section shell: card + title + note + collapse toggle ─────────────────────
// Returns true when the section is expanded.
inline bool draw_section_shell(AppState& app,
                               ControlPanelResult& result,
                               PanelSection section,
                               Rectangle rect,
                               const char* title,
                               const std::string& note,
                               Color accent,
                               float scale) {
    const bool collapsed = section_collapsed(app, section);

    // Card with accent stripe
    draw_card_accented(rect, WL::GLASS_1, with_alpha(accent, 65), accent);

    // Title
    draw_text(title,
              {rect.x + 18.0f * scale, rect.y + 11.0f * scale},
              20.0f * scale,
              WL::TEXT_PRIMARY);

    // Toggle button
    if (draw_section_toggle(
            {rect.x + rect.width - 72.0f * scale, rect.y + 9.0f * scale,
             56.0f * scale, 22.0f * scale},
            collapsed, accent, scale)) {
        result.toggled_section = section;
    }

    // Subtitle note
    if (!collapsed && !note.empty()) {
        const float nh = section_note_height(note, rect.width, scale);
        draw_text_block(note,
                        {rect.x + 18.0f * scale, rect.y + 32.0f * scale,
                         rect.width - 100.0f * scale, nh + 2.0f * scale},
                        13.5f * scale,
                        WL::TEXT_TERTIARY,
                        1.0f * scale);
    }
    return !collapsed;
}

// ── Slider ────────────────────────────────────────────────────────────────────
// Track: thin dark groove.  Fill: accent.  Knob: bright circle with accent ring + glow.
inline bool draw_slider(AppState& app, FieldId id, Rectangle rect,
                        double& value, double min, double max,
                        Color accent, float scale) {
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
            value   = min + (max - min) * t;
            changed = true;
        } else {
            app.ui.active_slider = FieldId::NONE;
        }
    }

    const double wheel = GetMouseWheelMove();
    if (hot && std::abs(wheel) > 0.0 && app.ui.active_slider == FieldId::NONE) {
        value = std::clamp(value + wheel * (max - min) / 160.0, min, max);
        changed = true;
        app.ui.slider_wheel_used = true;
    }

    const float t    = static_cast<float>((value - min) / (max - min));
    const float cy   = rect.y + rect.height * 0.5f;
    const Rectangle track = {rect.x, cy - 3.0f * scale, rect.width, 5.5f * scale};
    const Rectangle fill  = {track.x, track.y, track.width * t, track.height};
    const Vector2 knob = {track.x + track.width * t, cy};

    // Groove
    DrawRectangleRounded(track, 1.0f, 8, { 20, 36, 52, 255});
    // Fill glow
    DrawRectangleRounded(fill,  1.0f, 8, with_alpha(accent, 200));

    // Knob glow halo
    DrawCircleV(knob, 10.5f * scale, with_alpha(accent, 28));
    // Knob body
    DrawCircleV(knob,  8.0f * scale, {238, 244, 248, 255});
    // Knob accent ring
    DrawCircleLinesV(knob, 8.0f * scale, with_alpha(accent, 220));

    return changed;
}

// ── Number input box ──────────────────────────────────────────────────────────
inline bool draw_number_box(AppState& app, Rectangle rect, FieldId id,
                            double current_value, int precision,
                            const char* unit, Color accent, float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot    = CheckCollisionPointRec(mouse, rect);
    const bool active = app.ui.active_field == id;

    if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !active) {
        commit_active_field(app);
        activate_field(app, id);
    }

    const Color fill   = active ? Color{10, 22, 36, 255} : Color{ 8, 16, 28, 240};
    const Color border = active ? accent : with_alpha(accent, 70);
    DrawRectangleRounded(rect, 0.10f, 6, fill);
    DrawRectangleRoundedLines(rect, 0.10f, 6, 1.2f, border);

    std::string text = active ? app.ui.buffer : format_number(current_value, precision);
    if (active && (static_cast<int>(GetTime() * 2.0) % 2 == 0)) text += "_";

    const Vector2 unit_sz = measure_ui_text(unit, 13.5f * scale);
    const float val_px = fit_ui_text_size(text,
                                          rect.width - unit_sz.x - 26.0f * scale,
                                          17.0f * scale, 12.0f * scale);
    draw_text(text, {rect.x + 9.0f * scale, rect.y + 8.0f * scale}, val_px, WL::TEXT_PRIMARY);
    draw_text(unit, {rect.x + rect.width - unit_sz.x - 8.0f * scale,
                     rect.y + 10.0f * scale},
              13.5f * scale, WL::TEXT_TERTIARY);
    return hot;
}

// ── Visual number box (mirrors draw_number_box for VisualFieldSpec) ───────────
inline bool draw_visual_number_box(AppState& app, Rectangle rect,
                                   const VisualFieldSpec& spec, Color accent, float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot    = CheckCollisionPointRec(mouse, rect);
    const bool active = app.ui.active_field == spec.id;

    if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !active) {
        commit_active_field(app);
        activate_field(app, spec.id);
    }

    const Color fill   = active ? Color{10, 22, 36, 255} : Color{ 8, 16, 28, 240};
    const Color border = active ? accent : with_alpha(accent, 70);
    DrawRectangleRounded(rect, 0.10f, 6, fill);
    DrawRectangleRoundedLines(rect, 0.10f, 6, 1.2f, border);

    std::string text = active ? app.ui.buffer : format_number(app.visuals.*(spec.member), spec.precision);
    if (active && (static_cast<int>(GetTime() * 2.0) % 2 == 0)) text += "_";

    const Vector2 unit_sz = measure_ui_text(spec.unit, 13.5f * scale);
    const float val_px = fit_ui_text_size(text,
                                          rect.width - unit_sz.x - 26.0f * scale,
                                          17.0f * scale, 12.0f * scale);
    draw_text(text, {rect.x + 9.0f * scale, rect.y + 8.0f * scale}, val_px, WL::TEXT_PRIMARY);
    draw_text(spec.unit, {rect.x + rect.width - unit_sz.x - 8.0f * scale,
                          rect.y + 10.0f * scale},
              13.5f * scale, WL::TEXT_TERTIARY);
    return hot;
}

// ── Parameter row: label + slider + number box ────────────────────────────────
inline bool draw_parameter_row(AppState& app, Rectangle row,
                               const FieldSpec& spec, Color accent, float scale) {
    double& value = app.draft.*(spec.member);
    draw_text(spec.label, {row.x, row.y}, 16.5f * scale, WL::TEXT_SECONDARY);

    const float reset_width = 60.0f * scale;
    const float box_width = 108.0f * scale;
    const float gap = 8.0f * scale;
    Rectangle slider = {row.x, row.y + 20.0f * scale,
                        row.width - box_width - reset_width - gap * 2.0f, 18.0f * scale};
    Rectangle box    = {row.x + row.width - box_width - reset_width - gap,
                        row.y + 1.0f * scale, box_width, 34.0f * scale};
    Rectangle reset  = {row.x + row.width - reset_width,
                        row.y + 1.0f * scale, reset_width, 34.0f * scale};

    const bool changed = draw_slider(app, spec.id, slider, value, spec.min, spec.max, accent, scale);
    draw_number_box(app, box, spec.id, value, spec.precision, spec.unit, accent, scale);
    if (draw_button(reset,
                    "Reset",
                    field_matches_default(app, spec) ? Color{12, 24, 36, 190} : with_alpha(accent, 48),
                    field_matches_default(app, spec) ? Color{26, 48, 70, 210} : with_alpha(accent, 82),
                    field_matches_default(app, spec) ? WL::TEXT_TERTIARY : WL::TEXT_PRIMARY,
                    !field_matches_default(app, spec),
                    scale)) {
        reset_field_to_default(app, spec);
    }
    return changed;
}

// ── Visual parameter row ───────────────────────────────────────────────────────
inline bool draw_visual_row(AppState& app, Rectangle row,
                            const VisualFieldSpec& spec, Color accent, float scale) {
    double& value = app.visuals.*(spec.member);
    draw_text(spec.label, {row.x, row.y}, 16.5f * scale, WL::TEXT_SECONDARY);

    const float reset_width = 60.0f * scale;
    const float box_width = 108.0f * scale;
    const float gap = 8.0f * scale;
    Rectangle slider = {row.x, row.y + 20.0f * scale,
                        row.width - box_width - reset_width - gap * 2.0f, 18.0f * scale};
    Rectangle box    = {row.x + row.width - box_width - reset_width - gap,
                        row.y + 1.0f * scale, box_width, 34.0f * scale};
    Rectangle reset  = {row.x + row.width - reset_width,
                        row.y + 1.0f * scale, reset_width, 34.0f * scale};

    const bool changed = draw_slider(app, spec.id, slider, value, spec.min, spec.max, accent, scale);
    draw_visual_number_box(app, box, spec, accent, scale);
    if (draw_button(reset,
                    "Reset",
                    visual_field_matches_default(app, spec) ? Color{12, 24, 36, 190} : with_alpha(accent, 48),
                    visual_field_matches_default(app, spec) ? Color{26, 48, 70, 210} : with_alpha(accent, 82),
                    visual_field_matches_default(app, spec) ? WL::TEXT_TERTIARY : WL::TEXT_PRIMARY,
                    !visual_field_matches_default(app, spec),
                    scale)) {
        reset_visual_field_to_default(app, spec);
        app.visuals.preset = OverlayPreset::CUSTOM;
    }
    return changed;
}

// ── Preset button ──────────────────────────────────────────────────────────────
inline bool draw_preset_button(Rectangle rect, OverlayPreset current,
                               OverlayPreset preset, float scale) {
    const bool active = current == preset;
    return draw_button(rect,
                       overlay_preset_label(preset),
                       active ? with_alpha(WL::CYAN_DIM,  220) : Color{14, 30, 50, 228},
                       active ? with_alpha(WL::CYAN_CORE,  80) : Color{22, 48, 76, 255},
                       active ? WL::CYAN_CORE : WL::TEXT_SECONDARY,
                       true, scale);
}

// ── Jump button ────────────────────────────────────────────────────────────────
inline bool draw_jump_button(Rectangle rect, const char* label, Color accent, float scale) {
    return draw_button(rect, label,
                       with_alpha(accent, 55), with_alpha(accent, 95),
                       WL::TEXT_PRIMARY,
                       true, scale);
}

// ── Panel scroll handling ──────────────────────────────────────────────────────
inline void handle_control_panel_wheel_scroll(AppState& app, Rectangle panel,
                                              float max_panel_scroll) {
    if (!app.ui.slider_wheel_used && app.ui.active_field == FieldId::NONE) {
        if (CheckCollisionPointRec(GetMousePosition(), panel)) {
            const float wheel = GetMouseWheelMove();
            if (std::abs(wheel) > 0.001f) {
                app.ui.panel_scroll = std::clamp(
                    app.ui.panel_scroll - wheel * 40.0f,
                    0.0f, max_panel_scroll);
            }
        }
    }
}

// ── Main control panel ─────────────────────────────────────────────────────────
inline ControlPanelResult draw_control_panel(AppState& app, Rectangle panel) {
    app.ui.slider_wheel_used = false;
    ControlPanelResult result;
    const float ui = panel_ui_scale(panel);

    const bool pending_restart       = app.mode != RunMode::STOPPED && !drafts_equal(app.draft, app.applied);
    const bool show_connector_masses = app.draft.rigid_connectors && app.draft.connector_mass_enabled;

    // ── Section notes ──────────────────────────────────────────────────────────
    const std::string nav_note        = "Jump straight to the model area you want to tune.";
    const std::string actions_note    = "Launch, pause, stop, or wipe the trail.";
    const std::string connector_note  = "Choose rigid rods or ideal ropes, and optionally give the rods distributed mass.";
    const std::string geometry_note   = "Drag the stage handles or fine-tune the numbers.";
    const std::string launch_note     = "Initial angles and spin. Enter restarts from these values.";
    const std::string bob_drag_note   = "Per-bob viscous and air-drag coefficients.";
    const std::string link_drag_note  = "Axial terms act along the link. Normal terms resist cross-flow.";
    const std::string joints_note     = "Pivot and elbow damping or dry friction. Rigid mode only.";
    const std::string visuals_note    = "Presets switch between presentation and debugging overlays.";
    const std::string metrics_note    = "Live force-and-power readout for the current frame.";

    const float card_width           = panel.width - 36.0f * ui;
    const float nav_body_offset      = section_body_offset(nav_note,       card_width, ui);
    const float actions_body_offset  = section_body_offset(actions_note,   card_width, ui);
    const float connector_body_offset= section_body_offset(connector_note, card_width, ui);
    const float geometry_body_offset = section_body_offset(geometry_note,  card_width, ui);
    const float launch_body_offset   = section_body_offset(launch_note,    card_width, ui);
    const float bob_drag_body_offset = section_body_offset(bob_drag_note,  card_width, ui);
    const float link_drag_body_offset= section_body_offset(link_drag_note, card_width, ui);
    const float joints_body_offset   = section_body_offset(joints_note,    card_width, ui);
    const float visuals_body_offset  = section_body_offset(visuals_note,   card_width, ui);
    const float metrics_body_offset  = section_body_offset(metrics_note,   card_width, ui);

    const float jump_button_height = 32.0f * ui;
    const float jump_row_gap       = 9.0f  * ui;
    const float action_button_height = 34.0f * ui;
    const float action_row_gap       = 9.0f  * ui;
    const float connector_check_width = card_width - 28.0f * ui;

    const float rigid_checkbox_height = checkbox_card_height(
        "Rod constraints. Toggle off for massless ropes with slack/re-catch.",
        connector_check_width, ui);
    const float mass_checkbox_height = checkbox_card_height(
        "Treat each rod as a uniform rigid body instead of ideal massless links.",
        connector_check_width, ui);

    // ── Section heights ────────────────────────────────────────────────────────
    const float nav_height = section_card_height(app, PanelSection::NAV,
        nav_body_offset + jump_button_height * 3.0f + jump_row_gap * 2.0f + 16.0f * ui, ui);
    const float actions_height = section_card_height(app, PanelSection::CONTROLS,
        actions_body_offset + action_button_height * 2.0f + action_row_gap + 16.0f * ui, ui);

    float connector_expanded = connector_body_offset + rigid_checkbox_height + 12.0f * ui;
    if (app.draft.rigid_connectors)    connector_expanded += mass_checkbox_height + 10.0f * ui;
    if (show_connector_masses)         connector_expanded += static_cast<float>(CONNECTOR_FIELDS.size()) * 40.0f * ui + 10.0f * ui;
    connector_expanded += 12.0f * ui;
    const float connector_height = section_card_height(app, PanelSection::CONNECTOR, connector_expanded, ui);

    const float geometry_height  = section_card_height(app, PanelSection::GEOMETRY,
        geometry_body_offset  + static_cast<float>(GEOMETRY_FIELDS.size())          * 38.0f * ui + 18.0f * ui, ui);
    const float launch_height    = section_card_height(app, PanelSection::LAUNCH,
        launch_body_offset    + static_cast<float>(LAUNCH_FIELDS.size())             * 38.0f * ui + 18.0f * ui, ui);
    const float bob_drag_height  = section_card_height(app, PanelSection::BOB_DRAG,
        bob_drag_body_offset  + static_cast<float>(BOB_RESISTANCE_FIELDS.size())     * 38.0f * ui + 18.0f * ui, ui);
    const float connector_drag_height = section_card_height(app, PanelSection::LINK_DRAG,
        link_drag_body_offset + static_cast<float>(CONNECTOR_DRAG_FIELDS.size())     * 38.0f * ui + 20.0f * ui, ui);
    const float joints_height    = section_card_height(app, PanelSection::JOINTS,
        joints_body_offset    + static_cast<float>(JOINT_RESISTANCE_FIELDS.size())   * 38.0f * ui
            + (!app.draft.rigid_connectors ? 32.0f * ui : 16.0f * ui), ui);
    const float visuals_height   = section_card_height(app, PanelSection::VISUALS, 688.0f * ui, ui);
    const float metrics_height   = section_card_height(app, PanelSection::METRICS,
        metrics_body_offset + 350.0f * ui, ui);

    const float card_gap     = 12.0f * ui;
    const float badge_height = pending_restart ? 62.0f * ui : 44.0f * ui;

    // ── Content offsets ────────────────────────────────────────────────────────
    float cy = 16.0f * ui;
    const float title_offset    = cy; cy += 36.0f * ui;
    const float subtitle_offset = cy; cy += 26.0f * ui;
    const float badge_offset    = cy; cy += badge_height;
    const float nav_offset      = cy; cy += nav_height      + card_gap;
    const float actions_offset  = cy; cy += actions_height  + card_gap;
    const float connector_offset= cy; cy += connector_height + card_gap;
    const float geometry_offset = cy; cy += geometry_height  + card_gap;
    const float launch_offset   = cy; cy += launch_height    + card_gap;
    const float bob_drag_offset = cy; cy += bob_drag_height  + card_gap;
    const float connector_drag_offset = cy; cy += connector_drag_height + card_gap;
    const float joints_offset   = cy; cy += joints_height    + card_gap;
    const float visuals_offset  = cy; cy += visuals_height   + card_gap;
    const float metrics_offset  = cy; cy += metrics_height   + 16.0f * ui;

    const float max_panel_scroll = std::max(0.0f, cy - panel.height);
    app.ui.panel_scroll = std::clamp(app.ui.panel_scroll, 0.0f, max_panel_scroll);

    if (app.ui.active_field == FieldId::NONE) {
        if (IsKeyDown(KEY_DOWN)) app.ui.panel_scroll = std::clamp(app.ui.panel_scroll + 5.0f * ui, 0.0f, max_panel_scroll);
        if (IsKeyDown(KEY_UP))   app.ui.panel_scroll = std::clamp(app.ui.panel_scroll - 5.0f * ui, 0.0f, max_panel_scroll);
    }

    auto panel_y = [&](float offset) { return panel.y + offset - app.ui.panel_scroll; };
    auto jump_to = [&](float offset) {
        prepare_toggle_interaction(app);
        app.ui.panel_scroll = std::clamp(offset - 10.0f * ui, 0.0f, max_panel_scroll);
    };

    // ── Panel card (outer shell) ───────────────────────────────────────────────
    draw_card(panel, { 5, 10, 18, 224}, with_alpha(WL::GLASS_BORDER, 130));

    BeginScissorMode(static_cast<int>(panel.x), static_cast<int>(panel.y),
                     static_cast<int>(panel.width), static_cast<int>(panel.height));

    // ── Panel header ───────────────────────────────────────────────────────────
    draw_text("TUNING STUDIO",
              {panel.x + 18.0f * ui, panel_y(title_offset)},
              12.0f * ui, with_alpha(WL::CYAN_CORE, 190));
    draw_text("Pick a connector model, shape the launch, then run it.",
              {panel.x + 18.0f * ui, panel_y(title_offset) + 16.0f * ui},
              15.0f * ui, WL::TEXT_TERTIARY);

    // ── Status badges ──────────────────────────────────────────────────────────
    const float by = panel_y(badge_offset);
    if (app.mode == RunMode::RUNNING) {
        draw_badge({panel.x + 18.0f * ui, by, 90.0f * ui, 28.0f * ui},
                   "LIVE", with_alpha(WL::PLASMA_DIM, 210), WL::PLASMA_GREEN, ui);
    } else if (app.mode == RunMode::PAUSED) {
        draw_badge({panel.x + 18.0f * ui, by, 100.0f * ui, 28.0f * ui},
                   "HOLD", with_alpha(WL::XENON_DIM, 210), WL::XENON_CORE, ui);
    } else {
        draw_badge({panel.x + 18.0f * ui, by, 110.0f * ui, 28.0f * ui},
                   "EDIT", { 16, 44, 76, 210}, WL::ACCENT_GRAVITY, ui);
    }
    draw_badge({panel.x + 142.0f * ui, by, 155.0f * ui, 28.0f * ui},
               connector_mode_label(app.draft), {20, 40, 60, 200}, WL::TEXT_SECONDARY, ui);
    if (pending_restart) {
        draw_text("Settings staged for the next restart.",
                  {panel.x + 18.0f * ui, by + 34.0f * ui},
                  13.5f * ui, with_alpha(WL::XENON_CORE, 200));
    }

    // ── Quick Jump nav card ─────────────────────────────────────────────────────
    Rectangle nav_card = {panel.x + 18.0f * ui, panel_y(nav_offset), card_width, nav_height};
    if (draw_section_shell(app, result, PanelSection::NAV, nav_card,
                           "Quick Jump", nav_note, WL::ACCENT_FLOW, ui)) {
        const float jg  = 7.0f * ui;
        const float jw  = (nav_card.width - 32.0f * ui - jg) * 0.5f;
        const float jr1 = nav_card.y + nav_body_offset;
        const float jr2 = jr1 + jump_button_height + jump_row_gap;
        const float jr3 = jr2 + jump_button_height + jump_row_gap;

        if (draw_jump_button({nav_card.x + 16.0f * ui, jr1, jw, jump_button_height}, "Setup",   WL::ACCENT_FLOW,      ui)) jump_to(connector_offset);
        if (draw_jump_button({nav_card.x + 16.0f * ui + jw + jg, jr1, jw, jump_button_height},  "Launch", WL::ACCENT_LINK_DRAG, ui)) jump_to(launch_offset);
        if (draw_jump_button({nav_card.x + 16.0f * ui, jr2, jw, jump_button_height}, "Loads",   WL::ACCENT_DRAG,      ui)) jump_to(bob_drag_offset);
        if (draw_jump_button({nav_card.x + 16.0f * ui + jw + jg, jr2, jw, jump_button_height},  "Joints", WL::ACCENT_TORQUE,    ui)) jump_to(joints_offset);
        if (draw_jump_button({nav_card.x + 16.0f * ui, jr3, jw, jump_button_height}, "View",    WL::VIOLET_CORE,      ui)) jump_to(visuals_offset);
        if (draw_jump_button({nav_card.x + 16.0f * ui + jw + jg, jr3, jw, jump_button_height},  "Readout",WL::ACCENT_NET,       ui)) jump_to(metrics_offset);
    }

    // ── Controls card ──────────────────────────────────────────────────────────
    Rectangle actions = {panel.x + 18.0f * ui, panel_y(actions_offset), card_width, actions_height};
    if (draw_section_shell(app, result, PanelSection::CONTROLS, actions,
                           "Controls", actions_note, WL::PLASMA_GREEN, ui)) {
        const float bg  = 9.0f  * ui;
        const float bw2 = (actions.width - bg * 3.0f) * 0.5f;
        const float ar1 = actions.y + actions_body_offset;
        const float ar2 = ar1 + action_button_height + action_row_gap;

        if (draw_button({actions.x + bg, ar1, bw2, action_button_height},
                        app.mode == RunMode::STOPPED ? "Launch" : "Restart",
                        with_alpha(WL::PLASMA_DIM, 228), with_alpha(WL::PLASMA_GREEN, 80),
                        WL::PLASMA_GREEN, true, ui))
            result.command = PanelCommand::LAUNCH;

        if (draw_button({actions.x + bg * 2.0f + bw2, ar1, bw2, action_button_height},
                        app.mode == RunMode::PAUSED ? "Resume" : "Pause",
                        {24, 46, 72, 230}, {34, 62, 96, 255}, WL::TEXT_PRIMARY,
                        app.mode != RunMode::STOPPED, ui))
            result.command = PanelCommand::TOGGLE_PAUSE;

        if (draw_button({actions.x + bg, ar2, bw2, action_button_height},
                        "Stop",
                        with_alpha(WL::XENON_DIM, 224), with_alpha(WL::XENON_CORE, 80),
                        WL::XENON_CORE,
                        app.mode != RunMode::STOPPED, ui))
            result.command = PanelCommand::STOP;

        if (draw_button({actions.x + bg * 2.0f + bw2, ar2, bw2, action_button_height},
                        "Clear Trail",
                        {12, 28, 48, 226}, {20, 44, 72, 255}, WL::TEXT_SECONDARY,
                        true, ui))
            result.command = PanelCommand::CLEAR_TRAIL;
    }

    // ── Connector Behavior card ────────────────────────────────────────────────
    Rectangle connector_card = {panel.x + 18.0f * ui, panel_y(connector_offset), card_width, connector_height};
    if (draw_section_shell(app, result, PanelSection::CONNECTOR, connector_card,
                           "Connector Behaviour", connector_note, WL::ACCENT_LINK_DRAG, ui)) {
        float cy2 = connector_card.y + connector_body_offset;

        if (draw_checkbox({connector_card.x + 14.0f * ui, cy2,
                           connector_card.width - 28.0f * ui, rigid_checkbox_height},
                          app.draft.rigid_connectors,
                          "Rigid connectors",
                          "Rod constraints. Toggle off for massless ropes with slack/re-catch.",
                          WL::PLASMA_GREEN, ui)) {
            prepare_toggle_interaction(app);
            app.draft.rigid_connectors = !app.draft.rigid_connectors;
            if (!app.draft.rigid_connectors) app.draft.connector_mass_enabled = false;
            apply_draft_change(app);
        }

        if (app.draft.rigid_connectors) {
            cy2 += rigid_checkbox_height + 10.0f * ui;
            if (draw_checkbox({connector_card.x + 14.0f * ui, cy2,
                               connector_card.width - 28.0f * ui, mass_checkbox_height},
                              app.draft.connector_mass_enabled,
                              "Connector mass enabled",
                              "Treat each rod as a uniform rigid body instead of ideal massless links.",
                              WL::ACCENT_LINK_DRAG, ui)) {
                prepare_toggle_interaction(app);
                app.draft.connector_mass_enabled = !app.draft.connector_mass_enabled;
                if (app.draft.connector_mass_enabled &&
                    app.draft.connector1_mass < 1e-6 && app.draft.connector2_mass < 1e-6) {
                    app.draft.connector1_mass = 0.35;
                    app.draft.connector2_mass = 0.25;
                }
                apply_draft_change(app);
            }
        }

        if (show_connector_masses) {
            cy2 += mass_checkbox_height + 12.0f * ui;
            float row_y = cy2;
            for (const FieldSpec& spec : CONNECTOR_FIELDS) {
                if (draw_parameter_row(app, {connector_card.x + 16.0f * ui, row_y,
                                             connector_card.width - 32.0f * ui, 42.0f * ui},
                                       spec, WL::ACCENT_LINK_DRAG, ui))
                    apply_draft_change(app);
                row_y += 40.0f * ui;
            }
        }
    }

    // ── Stage Geometry card ────────────────────────────────────────────────────
    Rectangle geometry = {panel.x + 18.0f * ui, panel_y(geometry_offset), card_width, geometry_height};
    if (draw_section_shell(app, result, PanelSection::GEOMETRY, geometry,
                           "Stage Geometry", geometry_note, WL::ACCENT_FLOW, ui)) {
        float row_y = geometry.y + geometry_body_offset;
        for (const FieldSpec& spec : GEOMETRY_FIELDS) {
            if (draw_parameter_row(app, {geometry.x + 16.0f * ui, row_y,
                                         geometry.width - 32.0f * ui, 42.0f * ui},
                                   spec, WL::ACCENT_FLOW, ui))
                apply_draft_change(app);
            row_y += 38.0f * ui;
        }
    }

    // ── Launch State card ──────────────────────────────────────────────────────
    Rectangle launch = {panel.x + 18.0f * ui, panel_y(launch_offset), card_width, launch_height};
    if (draw_section_shell(app, result, PanelSection::LAUNCH, launch,
                           "Launch State", launch_note, WL::ACCENT_LINK_DRAG, ui)) {
        float row_y = launch.y + launch_body_offset;
        for (const FieldSpec& spec : LAUNCH_FIELDS) {
            if (draw_parameter_row(app, {launch.x + 16.0f * ui, row_y,
                                         launch.width - 32.0f * ui, 42.0f * ui},
                                   spec, WL::ACCENT_LINK_DRAG, ui))
                apply_draft_change(app);
            row_y += 38.0f * ui;
        }
    }

    // ── Bob Drag card ──────────────────────────────────────────────────────────
    Rectangle bob_drag = {panel.x + 18.0f * ui, panel_y(bob_drag_offset), card_width, bob_drag_height};
    if (draw_section_shell(app, result, PanelSection::BOB_DRAG, bob_drag,
                           "Bob Drag", bob_drag_note, WL::VIOLET_CORE, ui)) {
        float row_y = bob_drag.y + bob_drag_body_offset;
        for (const FieldSpec& spec : BOB_RESISTANCE_FIELDS) {
            if (draw_parameter_row(app, {bob_drag.x + 16.0f * ui, row_y,
                                         bob_drag.width - 32.0f * ui, 42.0f * ui},
                                   spec, WL::VIOLET_CORE, ui))
                apply_draft_change(app);
            row_y += 38.0f * ui;
        }
    }

    // ── Link Drag card ─────────────────────────────────────────────────────────
    Rectangle connector_drag = {panel.x + 18.0f * ui, panel_y(connector_drag_offset), card_width, connector_drag_height};
    if (draw_section_shell(app, result, PanelSection::LINK_DRAG, connector_drag,
                           "Link Drag", link_drag_note, WL::ACCENT_GRAVITY, ui)) {
        float row_y = connector_drag.y + link_drag_body_offset + 6.0f * ui;
        for (const FieldSpec& spec : CONNECTOR_DRAG_FIELDS) {
            if (draw_parameter_row(app, {connector_drag.x + 16.0f * ui, row_y,
                                         connector_drag.width - 32.0f * ui, 42.0f * ui},
                                   spec, WL::ACCENT_GRAVITY, ui))
                apply_draft_change(app);
            row_y += 38.0f * ui;
        }
    }

    // ── Joint Resistance card ──────────────────────────────────────────────────
    Rectangle joints = {panel.x + 18.0f * ui, panel_y(joints_offset), card_width, joints_height};
    if (draw_section_shell(app, result, PanelSection::JOINTS, joints,
                           "Joint Resistance", joints_note, WL::ACCENT_TORQUE, ui)) {
        float row_y = joints.y + joints_body_offset;
        for (const FieldSpec& spec : JOINT_RESISTANCE_FIELDS) {
            if (draw_parameter_row(app, {joints.x + 16.0f * ui, row_y,
                                         joints.width - 32.0f * ui, 42.0f * ui},
                                   spec, WL::ACCENT_TORQUE, ui))
                apply_draft_change(app);
            row_y += 38.0f * ui;
        }
        if (!app.draft.rigid_connectors) {
            draw_text("Torques stored but ignored in rope mode.",
                      {joints.x + 16.0f * ui, joints.y + joints.height - 20.0f * ui},
                      13.0f * ui, with_alpha(WL::XENON_CORE, 190));
        }
    }

    // ── Field Visuals card ─────────────────────────────────────────────────────
    Rectangle visuals = {panel.x + 18.0f * ui, panel_y(visuals_offset), card_width, visuals_height};
    if (draw_section_shell(app, result, PanelSection::VISUALS, visuals,
                           "Field Visuals", visuals_note, WL::VIOLET_CORE, ui)) {
        const float pg  = 10.0f * ui;
        const float pw  = (visuals.width - 32.0f * ui - pg) * 0.5f;
        float preset_y  = visuals.y + visuals_body_offset + 4.0f * ui;

        if (draw_preset_button({visuals.x + 16.0f * ui, preset_y, pw, 32.0f * ui},                    app.visuals.preset, OverlayPreset::CLEAN,      ui)) { prepare_toggle_interaction(app); apply_overlay_preset(app.visuals, OverlayPreset::CLEAN);      }
        if (draw_preset_button({visuals.x + 16.0f * ui + pw + pg, preset_y, pw, 32.0f * ui},           app.visuals.preset, OverlayPreset::FORCES,     ui)) { prepare_toggle_interaction(app); apply_overlay_preset(app.visuals, OverlayPreset::FORCES);     }
        preset_y += 38.0f * ui;
        if (draw_preset_button({visuals.x + 16.0f * ui, preset_y, pw, 32.0f * ui},                    app.visuals.preset, OverlayPreset::RESISTANCE,  ui)) { prepare_toggle_interaction(app); apply_overlay_preset(app.visuals, OverlayPreset::RESISTANCE);  }
        if (draw_preset_button({visuals.x + 16.0f * ui + pw + pg, preset_y, pw, 32.0f * ui},           app.visuals.preset, OverlayPreset::FULL,        ui)) { prepare_toggle_interaction(app); apply_overlay_preset(app.visuals, OverlayPreset::FULL);        }

        float row_y = preset_y + 46.0f * ui;

        // Master vector toggle
        if (draw_checkbox({visuals.x + 16.0f * ui, row_y, visuals.width - 32.0f * ui, 56.0f * ui},
                          app.visuals.show_vectors,
                          "Vector field overlay",
                          "Master switch for arrows, torque rings, inspector, and legend.",
                          WL::VIOLET_CORE, ui)) {
            prepare_toggle_interaction(app);
            app.visuals.show_vectors = !app.visuals.show_vectors;
            app.visuals.preset = OverlayPreset::CUSTOM;
        }
        row_y += 62.0f * ui;

        // Two-column checkbox grid
        const float cg = 8.0f * ui;
        const float cw = (visuals.width - 32.0f * ui - cg) * 0.5f;
        const float lx = visuals.x + 16.0f * ui;
        const float rx = lx + cw + cg;

        auto chk2 = [&](float x, bool& flag, const char* lbl, const char* note, Color ac) {
            if (draw_checkbox({x, row_y, cw, 56.0f * ui}, flag, lbl, note, ac, ui)) {
                prepare_toggle_interaction(app);
                flag = !flag;
                app.visuals.preset = OverlayPreset::CUSTOM;
            }
        };

        chk2(lx, app.visuals.show_velocity_vectors,    "Velocity",    "Motion direction and speed.", WL::ACCENT_VELOCITY);
        chk2(rx, app.visuals.show_gravity_vectors,     "Gravity",     "Weight on each bob.",         WL::ACCENT_GRAVITY);
        row_y += 62.0f * ui;
        chk2(lx, app.visuals.show_drag_vectors,        "Bob Drag",    "Direct drag on bodies.",      WL::ACCENT_DRAG);
        chk2(rx, app.visuals.show_link_drag_vectors,   "Link Drag",   "Distributed rod drag.",       WL::ACCENT_LINK_DRAG);
        row_y += 62.0f * ui;
        chk2(lx, app.visuals.show_reaction_vectors,    "Reaction",    "Constraint forces.",          WL::ACCENT_REACTION);
        chk2(rx, app.visuals.show_net_vectors,         "Net Force",   "All loads combined.",         WL::ACCENT_NET);
        row_y += 62.0f * ui;
        chk2(lx, app.visuals.show_joint_torque_vectors,"Joint Torque","Pivot and elbow torques.",    WL::ACCENT_TORQUE);
        row_y += 62.0f * ui;

        // Visual scalar rows
        for (const VisualFieldSpec& spec : VISUAL_FIELDS) {
            if (draw_visual_row(app, {visuals.x + 16.0f * ui, row_y,
                                      visuals.width - 32.0f * ui, 42.0f * ui},
                                spec, WL::VIOLET_CORE, ui))
                app.visuals.*(spec.member) = std::clamp(app.visuals.*(spec.member), spec.min, spec.max);
            row_y += 38.0f * ui;
        }
    }

    // ── Live Readout card ──────────────────────────────────────────────────────
    Rectangle metrics = {panel.x + 18.0f * ui, panel_y(metrics_offset), card_width, metrics_height};
    if (draw_section_shell(app, result, PanelSection::METRICS, metrics,
                           "Live Readout", metrics_note, WL::ACCENT_NET, ui)) {
        const double energy     = app.simulation.energy();
        const double diss_power = app.simulation.dissipation_power();
        const double reach      = app.simulation.reach();
        const double angle1     = app.simulation.theta1() * APP_RAD_TO_DEG;
        const double angle2     = app.simulation.theta2() * APP_RAD_TO_DEG;
        const bool res_enabled  = app.simulation.resistance_enabled();
        const float my = metrics.y + metrics_body_offset;
        const float mw = metrics.width * 0.5f - 22.0f * ui;

        draw_metric({metrics.x + 14.0f * ui, my,             mw, 60.0f * ui}, "Total Reach",  format_number(reach,  2) + " m",   ui);
        draw_metric({metrics.x + 14.0f * ui + mw + 12.0f*ui, my,             mw, 60.0f * ui}, "Energy",       format_number(energy, 3),              ui);
        draw_metric({metrics.x + 14.0f * ui, my + 66.0f*ui,  mw, 60.0f * ui}, "Upper Angle",  format_number(angle1, 1) + " deg",  ui);
        draw_metric({metrics.x + 14.0f * ui + mw + 12.0f*ui, my + 66.0f*ui,  mw, 60.0f * ui}, "Lower Angle",  format_number(angle2, 1) + " deg",  ui);
        draw_metric({metrics.x + 14.0f * ui, my + 132.0f*ui, mw, 60.0f * ui}, "Upper Link",   app.simulation.connector1_taut() ? "Taut" : "Slack", ui);
        draw_metric({metrics.x + 14.0f * ui + mw + 12.0f*ui, my + 132.0f*ui, mw, 60.0f * ui}, "Lower Link",   app.simulation.connector2_taut() ? "Taut" : "Slack", ui);
        draw_metric({metrics.x + 14.0f * ui, my + 198.0f*ui, mw, 60.0f * ui}, "Power",        format_number(diss_power, 3) + " W", ui);
        draw_metric({metrics.x + 14.0f * ui + mw + 12.0f*ui, my + 198.0f*ui, mw, 60.0f * ui}, "Resistance",  res_enabled ? "Active" : "Off",     ui);

        const char* mnote = res_enabled
            ? "Resistance active - mechanical energy should decay over time.\nLink drag is distributed along rods or taut segments."
            : "Dynamic framing scales the mechanism into view.\nRope mode: tension only acts while taut.";
        draw_text_block(mnote,
                        {metrics.x + 14.0f * ui, my + 270.0f * ui,
                         metrics.width - 28.0f * ui, 60.0f * ui},
                        13.5f * ui, WL::TEXT_TERTIARY, 2.0f * ui);
    }

    EndScissorMode();

    // ── Scrollbar ──────────────────────────────────────────────────────────────
    if (max_panel_scroll > 0.0f) {
        const float track_h = panel.height - 8.0f;
        const float thumb_h = std::max(20.0f, track_h * (panel.height / (panel.height + max_panel_scroll)));
        const float thumb_frac = max_panel_scroll > 0.0f ? app.ui.panel_scroll / max_panel_scroll : 0.0f;
        const float thumb_y = panel.y + 4.0f + (track_h - thumb_h) * thumb_frac;
        DrawRectangleRounded({panel.x + panel.width - 6.0f, panel.y + 4.0f, 3.5f, track_h},
                             1.0f, 4, {18, 42, 60, 90});
        DrawRectangleRounded({panel.x + panel.width - 6.0f, thumb_y, 3.5f, thumb_h},
                             1.0f, 4, with_alpha(WL::CYAN_DIM, 170));
    }

    result.max_scroll = max_panel_scroll;
    return result;
}
