#pragma once
#include "ControlPanel.hpp"

struct SettingsModalResult {
    ControlPanelResult panel{};
    bool close_requested = false;
};

inline Rectangle make_settings_modal_rect(Rectangle viewport) {
    const float available_width = std::max(320.0f, viewport.width - 18.0f);
    const float max_width = std::min(780.0f, available_width);
    const float min_width = std::min(580.0f, max_width);
    const float width = std::clamp(viewport.width * 0.38f, min_width, max_width);
    const float available_height = std::max(360.0f, viewport.height - 20.0f);
    const float max_height = available_height;
    const float min_height = std::min(700.0f, max_height);
    const float height = std::clamp(viewport.height - 44.0f, min_height, max_height);
    return {
        viewport.x + (viewport.width - width) * 0.5f,
        viewport.y + (viewport.height - height) * 0.5f,
        width,
        height
    };
}

inline SettingsModalResult draw_settings_modal(AppState& app,
                                               Rectangle viewport) {
    SettingsModalResult result;
    DrawRectangleRec(viewport, {4, 7, 12, 176});

    const Rectangle modal = make_settings_modal_rect(viewport);
    const Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, modal)) {
        result.close_requested = true;
    }
    if (app.ui.active_field == FieldId::NONE && IsKeyPressed(KEY_ESCAPE)) {
        result.close_requested = true;
    }

    result.panel = draw_control_panel(app, modal);

    const float scale = panel_ui_scale(modal);
    if (draw_button({modal.x + modal.width - 106.0f * scale, modal.y + 14.0f * scale, 86.0f * scale, 28.0f * scale},
                    "Close",
                    {33, 50, 72, 235},
                    {46, 67, 94, 255},
                    {228, 235, 246, 255},
                    true,
                    scale)) {
        result.close_requested = true;
    }

    return result;
}
