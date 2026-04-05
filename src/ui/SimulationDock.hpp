#pragma once
#include "CanvasOverlayLayout.hpp"
#include "UiPrimitives.hpp"
#include "../app/AppRuntime.hpp"

struct SimulationDockResult {
    PanelCommand command = PanelCommand::NONE;
    bool open_settings   = false;
};

// ── Run mode helpers ──────────────────────────────────────────────────────────
inline const char* run_mode_label(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return "LIVE";
    case RunMode::PAUSED:  return "HOLD";
    case RunMode::STOPPED: return "EDIT";
    }
    return "EDIT";
}

inline Color run_mode_badge_fill(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return with_alpha(WL::PLASMA_DIM,  210);
    case RunMode::PAUSED:  return with_alpha(WL::XENON_DIM,   210);
    case RunMode::STOPPED: return { 18, 46, 76, 210};
    }
    return { 18, 46, 76, 210};
}

inline Color run_mode_badge_text(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return WL::PLASMA_GREEN;
    case RunMode::PAUSED:  return WL::XENON_CORE;
    case RunMode::STOPPED: return WL::ACCENT_GRAVITY;
    }
    return WL::TEXT_SECONDARY;
}

// ── Simulation dock ───────────────────────────────────────────────────────────
inline SimulationDockResult draw_simulation_dock(const AppState& app,
                                                 Rectangle viewport) {
    SimulationDockResult result;

    const CanvasOverlayRects hud =
        make_canvas_overlay_layout(viewport,
                                   app.visuals.show_vectors,
                                   app.simulation.rigid_connectors());
    const Rectangle dock = hud.sim_dock;
    const float s    = hud.hud_scale;
    const float gap  = 7.0f * s;
    const float ix   = dock.x + 13.0f * s;   // inner left x
    const float iw   = dock.width - 26.0f * s; // inner width

    // ── Card ─────────────────────────────────────────────────────────────────
    draw_card(dock, WL::GLASS_1, with_alpha(WL::CYAN_DIM, 95));

    // Top accent line
    DrawLineEx({dock.x + 4, dock.y + 1}, {dock.x + dock.width - 4, dock.y + 1},
               1.5f, with_alpha(WL::CYAN_CORE, 60));

    // ── Header ────────────────────────────────────────────────────────────────
    draw_text("SIMULATION",
              {ix, dock.y + 11.0f * s},
              12.0f * s,
              with_alpha(WL::CYAN_CORE, 180));

    // ── Status badges ─────────────────────────────────────────────────────────
    draw_badge({ix, dock.y + 30.0f * s, 74.0f * s, 22.0f * s},
               run_mode_label(app.mode),
               run_mode_badge_fill(app.mode),
               run_mode_badge_text(app.mode),
               s);
    draw_badge({dock.x + dock.width - 126.0f * s, dock.y + 30.0f * s, 112.0f * s, 22.0f * s},
               connector_mode_label(app.mode == RunMode::STOPPED ? app.draft : app.applied),
               { 22, 42, 62, 200},
               WL::TEXT_SECONDARY,
               s);

    // ── Separator ─────────────────────────────────────────────────────────────
    DrawLineEx({ix, dock.y + 60.0f * s}, {ix + iw, dock.y + 60.0f * s},
               1.0f, with_alpha(WL::GLASS_BORDER, 100));

    // ── Control buttons ───────────────────────────────────────────────────────
    const float bw   = (iw - gap) * 0.5f;
    const float row1 = dock.y + 68.0f * s;
    const float row2 = row1 + 34.0f * s + gap;
    const float row3 = row2 + 34.0f * s + gap * 0.6f;

    // Launch / Restart — primary green
    if (draw_button({ix, row1, bw, 32.0f * s},
                    app.mode == RunMode::STOPPED ? "Launch" : "Restart",
                    with_alpha(WL::PLASMA_DIM,  230),
                    with_alpha(WL::PLASMA_GREEN, 80),
                    WL::PLASMA_GREEN,
                    true, s)) {
        result.command = PanelCommand::LAUNCH;
    }

    // Pause / Resume — neutral blue
    if (draw_button({ix + bw + gap, row1, bw, 32.0f * s},
                    app.mode == RunMode::PAUSED ? "Resume" : "Pause",
                    { 26, 48, 74, 232},
                    { 36, 64, 96, 255},
                    WL::TEXT_PRIMARY,
                    app.mode != RunMode::STOPPED, s)) {
        result.command = PanelCommand::TOGGLE_PAUSE;
    }

    // Stop — xenon red
    if (draw_button({ix, row2, bw, 32.0f * s},
                    "Stop",
                    with_alpha(WL::XENON_DIM,  224),
                    with_alpha(WL::XENON_CORE,  80),
                    WL::XENON_CORE,
                    app.mode != RunMode::STOPPED, s)) {
        result.command = PanelCommand::STOP;
    }

    // Clear Trail — dim
    if (draw_button({ix + bw + gap, row2, bw, 32.0f * s},
                    "Clear Trail",
                    { 14, 30, 50, 228},
                    { 22, 48, 76, 255},
                    WL::TEXT_SECONDARY,
                    true, s)) {
        result.command = PanelCommand::CLEAR_TRAIL;
    }

    // ── Open Tuning Studio — violet CTA ──────────────────────────────────────
    if (draw_button({ix, row3, iw, 32.0f * s},
                    "Open Tuning Studio",
                    with_alpha(WL::VIOLET_DIM,  220),
                    with_alpha(WL::VIOLET_CORE,  80),
                    WL::VIOLET_CORE,
                    true, s)) {
        result.open_settings = true;
    }

    return result;
}