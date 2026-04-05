#pragma once
#include "CanvasOverlayLayout.hpp"
#include "UiPrimitives.hpp"
#include "../app/AppRuntime.hpp"

struct SimulationDockResult {
    PanelCommand command = PanelCommand::NONE;
    bool open_settings = false;
};

inline const char* run_mode_label(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return "LIVE";
    case RunMode::PAUSED: return "PAUSED";
    case RunMode::STOPPED: return "EDIT";
    }
    return "EDIT";
}

inline Color run_mode_fill(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return {16, 82, 70, 220};
    case RunMode::PAUSED: return {92, 78, 20, 220};
    case RunMode::STOPPED: return {17, 68, 98, 220};
    }
    return {17, 68, 98, 220};
}

inline SimulationDockResult draw_simulation_dock(const AppState& app,
                                                 Rectangle viewport) {
    SimulationDockResult result;
    const CanvasOverlayRects hud =
        make_canvas_overlay_layout(viewport, app.visuals.show_vectors, app.simulation.rigid_connectors());
    const Rectangle dock = hud.sim_dock;
    const float scale = hud.hud_scale;
    const float gap = 8.0f * scale;
    const float inner_x = dock.x + 14.0f * scale;
    const float inner_w = dock.width - 28.0f * scale;

    draw_card(dock, {7, 16, 24, 194}, {55, 96, 115, 110});
    draw_text("Simulation", {inner_x, dock.y + 12.0f * scale}, 19.0f * scale, {237, 244, 248, 255});

    draw_badge({inner_x, dock.y + 40.0f * scale, 80.0f * scale, 24.0f * scale},
               run_mode_label(app.mode),
               run_mode_fill(app.mode),
               {236, 244, 246, 255},
               scale);
    draw_badge({dock.x + dock.width - 132.0f * scale, dock.y + 40.0f * scale, 118.0f * scale, 24.0f * scale},
               connector_mode_label(app.mode == RunMode::STOPPED ? app.draft : app.applied),
               {34, 51, 67, 210},
               {223, 235, 243, 255},
               scale);

    const float button_w = (inner_w - gap) * 0.5f;
    const float row1_y = dock.y + 74.0f * scale;
    const float row2_y = row1_y + 38.0f * scale;
    const float tune_y = row2_y + 40.0f * scale;

    if (draw_button({inner_x, row1_y, button_w, 32.0f * scale},
                    app.mode == RunMode::STOPPED ? "Launch" : "Restart",
                    {16, 88, 92, 235}, {24, 118, 122, 255}, {228, 249, 248, 255},
                    true,
                    scale)) {
        result.command = PanelCommand::LAUNCH;
    }
    if (draw_button({inner_x + button_w + gap, row1_y, button_w, 32.0f * scale},
                    app.mode == RunMode::PAUSED ? "Resume" : "Pause",
                    {33, 50, 72, 235}, {46, 67, 94, 255}, {228, 235, 246, 255},
                    app.mode != RunMode::STOPPED,
                    scale)) {
        result.command = PanelCommand::TOGGLE_PAUSE;
    }
    if (draw_button({inner_x, row2_y, button_w, 32.0f * scale},
                    "Stop",
                    {92, 38, 28, 230}, {118, 49, 36, 255}, {255, 233, 225, 255},
                    app.mode != RunMode::STOPPED,
                    scale)) {
        result.command = PanelCommand::STOP;
    }
    if (draw_button({inner_x + button_w + gap, row2_y, button_w, 32.0f * scale},
                    "Clear Trail",
                    {19, 37, 53, 230}, {28, 55, 76, 255}, {228, 235, 246, 255},
                    true,
                    scale)) {
        result.command = PanelCommand::CLEAR_TRAIL;
    }
    if (draw_button({inner_x, tune_y, inner_w, 34.0f * scale},
                    "Open Tuning Studio",
                    {58, 45, 92, 226}, {76, 58, 118, 255}, {239, 233, 250, 255},
                    true,
                    scale)) {
        result.open_settings = true;
    }

    return result;
}
