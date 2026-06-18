#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"

struct GuidedFirstUniverseSceneResult {
    bool open_workspace = false;
    bool open_atlas = false;
    bool open_reference = false;
    bool open_cosmos = false;
};

GuidedFirstUniverseSceneResult draw_guided_first_universe_scene(AppState& app,
                                                               Rectangle viewport);
