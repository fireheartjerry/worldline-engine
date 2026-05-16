#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"

struct UniverseAtlasSceneResult {
    bool back_requested = false;
    bool open_workspace = false;
    bool open_reference = false;
    bool refresh_catalog = false;
};

UniverseAtlasSceneResult draw_universe_atlas_scene(AppState& app,
                                                   Rectangle viewport);
