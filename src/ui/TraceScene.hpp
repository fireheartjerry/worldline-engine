#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"

struct TraceSceneResult {
    bool back_requested = false;
    bool open_workspace = false;
};

TraceSceneResult draw_trace_scene(AppState& app,
                                  Rectangle viewport);
