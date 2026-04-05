#pragma once
#include "raylib.h"

struct PendulumLayout {
    Rectangle viewport{};
    Rectangle stage_rect{};
    Vector2 pivot{};
    float scale = 1.0f;
};
