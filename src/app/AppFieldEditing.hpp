#pragma once
// AppRuntime — field defaults, activation, commit, and text input.
#include "AppOverlay.hpp"

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
    app.ui.buffer_select_all = false;
    app.ui.backspace_repeat_timer = 0.0f;
    app.ui.active_slider = FieldId::NONE;
    app.draft.*(spec.member) = default_field_value(spec);
    clamp_draft(app.draft);
    sync_preview(app);
}

inline void reset_visual_field_to_default(AppState& app, const VisualFieldSpec& spec) {
    app.ui.active_field = FieldId::NONE;
    app.ui.buffer.clear();
    app.ui.buffer_select_all = false;
    app.ui.backspace_repeat_timer = 0.0f;
    app.ui.active_slider = FieldId::NONE;
    app.visuals.*(spec.member) = default_visual_field_value(spec);
}

inline void activate_field(AppState& app, FieldId id) {
    if (const VisualFieldSpec* spec = find_visual_field_spec(id)) {
        app.ui.active_field = id;
        app.ui.buffer = format_number(app.visuals.*(spec->member), spec->precision);
        app.ui.buffer_select_all = true;
        app.ui.backspace_repeat_timer = 0.0f;
        return;
    }

    if (const FieldSpec* spec = find_field_spec(id)) {
        app.ui.active_field = id;
        app.ui.buffer = format_number(app.draft.*(spec->member), spec->precision);
        app.ui.buffer_select_all = true;
        app.ui.backspace_repeat_timer = 0.0f;
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
    app.ui.buffer_select_all = false;
    app.ui.backspace_repeat_timer = 0.0f;
}

inline void prepare_toggle_interaction(AppState& app) {
    commit_active_field(app);
    app.ui.active_slider = FieldId::NONE;
}

inline void cancel_active_field(AppState& app) {
    app.ui.active_field = FieldId::NONE;
    app.ui.buffer.clear();
    app.ui.buffer_select_all = false;
    app.ui.backspace_repeat_timer = 0.0f;
}

inline void handle_active_field_input(AppState& app) {
    if (app.ui.active_field == FieldId::NONE) {
        return;
    }

    const bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl_down && IsKeyPressed(KEY_A)) {
        app.ui.buffer_select_all = true;
    }

    int ch = GetCharPressed();
    while (ch > 0) {
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '.') {
            if (app.ui.buffer_select_all) {
                app.ui.buffer.clear();
                app.ui.buffer_select_all = false;
            }
            app.ui.buffer.push_back(static_cast<char>(ch));
        }
        ch = GetCharPressed();
    }

    if (ctrl_down && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (app.ui.buffer_select_all) {
            app.ui.buffer.clear();
            app.ui.buffer_select_all = false;
        }
        while (clipboard != nullptr && *clipboard != '\0') {
            const char value = *clipboard;
            if ((value >= '0' && value <= '9') || value == '-' || value == '.') {
                app.ui.buffer.push_back(value);
            }
            ++clipboard;
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (app.ui.buffer_select_all) {
            app.ui.buffer.clear();
            app.ui.buffer_select_all = false;
        } else if (!app.ui.buffer.empty()) {
            app.ui.buffer.pop_back();
        }
        app.ui.backspace_repeat_timer = 0.0f;
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        app.ui.backspace_repeat_timer += GetFrameTime();
        const float repeat_delay = 0.42f;
        const float repeat_step = 0.035f;
        while (app.ui.backspace_repeat_timer >= repeat_delay) {
            if (app.ui.buffer_select_all) {
                app.ui.buffer.clear();
                app.ui.buffer_select_all = false;
                app.ui.backspace_repeat_timer = 0.0f;
                break;
            }
            if (!app.ui.buffer.empty()) {
                app.ui.buffer.pop_back();
            }
            app.ui.backspace_repeat_timer -= repeat_step;
        }
    } else {
        app.ui.backspace_repeat_timer = 0.0f;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        commit_active_field(app);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        cancel_active_field(app);
    }
}

