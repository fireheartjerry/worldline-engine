#pragma once
// Control Panel — reusable widgets (section shells, sliders, number boxes,
// parameter rows, preset/jump buttons, scroll handling).
#include "UiPrimitives.hpp"
#include "../app/AppRuntime.hpp"
#include <algorithm>
#include <cmath>
#include <string>

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

