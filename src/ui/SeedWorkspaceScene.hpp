#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"

#include <algorithm>

struct SeedWorkspaceSceneResult {
    bool back_requested = false;
    bool open_atlas = false;
    bool open_reference = false;
    bool open_trace = false;
    bool refresh_catalog = false;
};

// Live readout produced by the field backend (filled in main, consumed here).
// POD only — keeps the scene decoupled from the FieldRenderer header.
struct FieldReadout {
    bool valid = false;
    float exotic_index = 0.0f;
    float vorticity = 0.0f;
    float flux = 0.0f;
    float coherence = 0.0f;
    float handedness = 0.0f;
    int particles = 0;
    Color cool{64, 228, 240, 255};
    Color hot{255, 148, 48, 255};
    Color accent{120, 255, 220, 255};
};

// Shared layout so the live field backend (drawn in main) and the workspace
// chrome (drawn here) agree on exactly where the stage lives.
struct SeedWorkspaceLayout {
    float scale = 1.0f;
    Rectangle header{};
    Rectangle stage{};
    Rectangle inspector{};
    Rectangle timeline{};
};

inline SeedWorkspaceLayout seed_workspace_layout(Rectangle viewport) {
    SeedWorkspaceLayout L;
    L.scale = std::clamp(std::min(viewport.width / 1540.0f, viewport.height / 940.0f), 0.84f, 1.12f);
    const float margin = 18.0f * L.scale;
    const float gap = 16.0f * L.scale;
    const float inspector_w = std::clamp(viewport.width * 0.24f, 300.0f * L.scale, 360.0f * L.scale);
    const float timeline_h = 132.0f * L.scale;
    L.header = {viewport.x + margin, viewport.y + margin, viewport.width - margin * 2.0f, 116.0f * L.scale};
    L.stage = {
        viewport.x + margin,
        L.header.y + L.header.height + gap,
        viewport.width - margin * 2.0f - inspector_w - gap,
        viewport.height - L.header.height - timeline_h - margin * 3.0f - gap * 2.0f
    };
    L.inspector = {L.stage.x + L.stage.width + gap, L.stage.y, inspector_w, L.stage.height + timeline_h + gap};
    L.timeline = {L.stage.x, L.stage.y + L.stage.height + gap, L.stage.width, timeline_h};
    return L;
}

SeedWorkspaceSceneResult draw_seed_workspace_scene(AppState& app,
                                                   Rectangle viewport,
                                                   const FieldReadout& field_readout);
