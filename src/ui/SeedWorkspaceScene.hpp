#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"

struct SeedWorkspaceSceneResult {
    bool back_requested = false;
    bool open_atlas = false;
    bool open_reference = false;
    bool open_trace = false;
    bool refresh_catalog = false;
};

SeedWorkspaceSceneResult draw_seed_workspace_scene(AppState& app,
                                                   Rectangle viewport);
