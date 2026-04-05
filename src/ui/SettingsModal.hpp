#pragma once
#include "ControlPanel.hpp"

// ── Settings modal ────────────────────────────────────────────────────────────
// A full-height sheet that slides over the canvas.  The overlay backdrop uses
// a very dark, slightly tinted vignette rather than a plain black so the stage
// is still faintly visible behind it (depth effect).

struct SettingsModalResult {
    ControlPanelResult panel{};
    bool close_requested = false;
};

inline Rectangle make_settings_modal_rect(Rectangle viewport) {
    const float available_w = std::max(320.0f, viewport.width  - 18.0f);
    const float max_w       = std::min(780.0f, available_w);
    const float min_w       = std::min(580.0f, max_w);
    const float width       = std::clamp(viewport.width * 0.38f, min_w, max_w);

    const float available_h = std::max(360.0f, viewport.height - 20.0f);
    const float min_h       = std::min(700.0f, available_h);
    const float height      = std::clamp(viewport.height - 44.0f, min_h, available_h);

    return {
        viewport.x + (viewport.width  - width)  * 0.5f,
        viewport.y + (viewport.height - height)  * 0.5f,
        width,
        height
    };
}

inline SettingsModalResult draw_settings_modal(AppState& app, Rectangle viewport) {
    SettingsModalResult result;

    // ── Backdrop ──────────────────────────────────────────────────────────────
    // Tinted void — very dark cyan tint so it doesn't feel like a flat black
    DrawRectangleRec(viewport, { 2, 8, 14, 184});

    // Subtle radial darkening at edges to create depth
    DrawCircleGradient(
        static_cast<int>(viewport.x + viewport.width  * 0.5f),
        static_cast<int>(viewport.y + viewport.height * 0.5f),
        std::max(viewport.width, viewport.height) * 0.72f,
        {0, 0, 0, 0},
        { 2, 6, 10, 120});

    const Rectangle modal = make_settings_modal_rect(viewport);
    const Vector2 mouse   = GetMousePosition();

    // Close on outside click or Escape
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, modal)) {
        result.close_requested = true;
    }
    if (app.ui.active_field == FieldId::NONE && IsKeyPressed(KEY_ESCAPE)) {
        result.close_requested = true;
    }

    // ── Modal border glow ─────────────────────────────────────────────────────
    // Draw a slightly enlarged glow rect behind the modal for a luminous edge.
    DrawRectangleRounded(
        {modal.x - 2, modal.y - 2, modal.width + 4, modal.height + 4},
        0.04f, 8,
        with_alpha(WL::CYAN_DIM, 55));

    // ── Control panel inside modal ────────────────────────────────────────────
    result.panel = draw_control_panel(app, modal);

    // ── Close button ──────────────────────────────────────────────────────────
    const float scale = panel_ui_scale(modal);
    if (draw_button(
            {modal.x + modal.width - 96.0f * scale,
             modal.y + 12.0f * scale,
             78.0f * scale,
             26.0f * scale},
            "CLOSE",
            { 22, 40, 64, 230},
            { 32, 58, 90, 255},
            WL::TEXT_SECONDARY,
            true, scale)) {
        result.close_requested = true;
    }

    return result;
}