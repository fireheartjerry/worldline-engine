#pragma once
// 2D navigation for the Cosmos sandbox stage: an animated zoom + pan camera that
// also flows continuously through the scale ladder. Scrolling in past a tier
// drops to the next-smaller tier; scrolling out past it rises to the next-larger
// one — so you can travel from quarks to galaxies with one gesture. Direct jumps
// (scale rail, number keys, brackets) animate to any tier instantly.

#include "ui/CosmosExplorerInternal.hpp"
#include "cosmos/ScaleLadder.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace cosmos_ui {

// Comfortable zoom range within a single tier; pushing past either end crosses
// into the neighbouring tier. The ~7x span per tier gives a powers-of-ten feel.
constexpr double kZoomMin = 0.50;
constexpr double kZoomMax = 3.60;

inline float stage_base_scale(Rectangle stage) {
    return std::min(stage.width, stage.height) * 0.5f / static_cast<float>(kWorldHalf);
}

inline Vector2 cosmos_world_to_screen(Vec2 w, Rectangle stage, const CosmosCamera& cam) {
    const float cx = stage.x + stage.width * 0.5f;
    const float cy = stage.y + stage.height * 0.5f;
    const float s = stage_base_scale(stage) * static_cast<float>(cam.zoom);
    return {cx + static_cast<float>(w.x - cam.pan.x) * s,
            cy + static_cast<float>(w.y - cam.pan.y) * s};
}

// Recenter the camera and (re)populate a tier — used by every direct jump.
inline void cosmos_jump_to_tier(CosmosState& cosmos, int index) {
    index = std::clamp(index, 0, static_cast<int>(kScaleCount) - 1);
    cosmos.set_scale(static_cast<Scale>(index));
    cosmos.spawn();
    cosmos.camera.target_zoom = 1.0;
    cosmos.camera.zoom = 1.0;
    cosmos.camera.target_pan = {};
    cosmos.camera.pan = {};
    cosmos.camera.flash = 1.0f;
}

// Advance the camera one frame from input. `interactive` is false while a modal
// is open so navigation never fights an overlay.
inline void update_cosmos_camera(CosmosState& cosmos, Rectangle stage,
                                 float dt, bool interactive) {
    CosmosCamera& cam = cosmos.camera;
    const float sc0 = stage_base_scale(stage);
    const Vector2 ctr = {stage.x + stage.width * 0.5f, stage.y + stage.height * 0.5f};

    // First time the stage is shown, drop in a live universe so there is always
    // something to navigate.
    if (!cam.primed && !cosmos.has_sim) {
        cosmos.spawn();
        cam.primed = true;
    }

    if (interactive) {
        const bool over = CheckCollisionPointRec(GetMousePosition(), stage);

        // Wheel zoom, anchored at the cursor (the world point under the mouse
        // stays put as you zoom).
        const float wheel = GetMouseWheelMove();
        if (over && wheel != 0.0f) {
            const Vector2 m = GetMousePosition();
            const double z0 = cam.target_zoom;
            const Vec2 before = {cam.target_pan.x + (m.x - ctr.x) / (sc0 * z0),
                                 cam.target_pan.y + (m.y - ctr.y) / (sc0 * z0)};
            cam.target_zoom *= std::exp(wheel * 0.20);
            const double z1 = cam.target_zoom;
            const Vec2 after = {cam.target_pan.x + (m.x - ctr.x) / (sc0 * z1),
                                cam.target_pan.y + (m.y - ctr.y) / (sc0 * z1)};
            cam.target_pan.x += before.x - after.x;
            cam.target_pan.y += before.y - after.y;
        }

        // Drag to pan (left or middle button over the stage).
        const bool drag_btn = IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
                              IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
        if (over && drag_btn && (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) || cam.dragging ||
                                 std::abs(GetMouseDelta().x) + std::abs(GetMouseDelta().y) > 0.0f)) {
            const Vector2 d = GetMouseDelta();
            cam.target_pan.x -= d.x / (sc0 * cam.target_zoom);
            cam.target_pan.y -= d.y / (sc0 * cam.target_zoom);
            cam.dragging = true;
        }
        if (!drag_btn) {
            cam.dragging = false;
        }

        // Keyboard: WASD/arrows pan, +/- zoom, [ ] step tiers, 1-9 jump, 0 reset.
        const double pan_step = (5.0 * dt) / std::max(0.35, cam.target_zoom);
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  cam.target_pan.x -= pan_step * kWorldHalf;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) cam.target_pan.x += pan_step * kWorldHalf;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    cam.target_pan.y -= pan_step * kWorldHalf;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  cam.target_pan.y += pan_step * kWorldHalf;
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD))      cam.target_zoom *= std::exp(2.2 * dt);
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) cam.target_zoom *= std::exp(-2.2 * dt);

        const int idx = static_cast<int>(scale_index(cosmos.scale));
        if (IsKeyPressed(KEY_LEFT_BRACKET))  cosmos_jump_to_tier(cosmos, idx - 1);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) cosmos_jump_to_tier(cosmos, idx + 1);
        if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_HOME)) {
            cam.target_zoom = 1.0;
            cam.target_pan = {};
        }
        for (int k = 0; k < 9; ++k) {
            if (IsKeyPressed(KEY_ONE + k)) {
                cosmos_jump_to_tier(cosmos, k);
            }
        }
    }

    // Tier flow: crossing a zoom bound steps to the neighbouring tier and lands
    // at the opposite end of the new tier's range, so the gesture feels continuous.
    const int idx = static_cast<int>(scale_index(cosmos.scale));
    if (cam.target_zoom > kZoomMax && idx > 0) {
        cosmos.set_scale(static_cast<Scale>(idx - 1));
        cosmos.spawn();
        cam.target_pan = {}; cam.pan = {};
        cam.target_zoom = kZoomMin * 1.05; cam.zoom = kZoomMin * 1.05;
        cam.flash = 1.0f;
    } else if (cam.target_zoom < kZoomMin && idx < static_cast<int>(kScaleCount) - 1) {
        cosmos.set_scale(static_cast<Scale>(idx + 1));
        cosmos.spawn();
        cam.target_pan = {}; cam.pan = {};
        cam.target_zoom = kZoomMax * 0.95; cam.zoom = kZoomMax * 0.95;
        cam.flash = 1.0f;
    }
    // Clamp at the ends of the ladder (can't go smaller than quarks or bigger
    // than the cosmic web).
    if (idx == 0) cam.target_zoom = std::min(cam.target_zoom, kZoomMax);
    if (idx == static_cast<int>(kScaleCount) - 1) cam.target_zoom = std::max(cam.target_zoom, kZoomMin);
    cam.target_zoom = std::clamp(cam.target_zoom, kZoomMin * 0.9, kZoomMax * 1.1);

    // Critically damped-ish smoothing toward the targets.
    const double k = 1.0 - std::exp(-13.0 * std::max(0.0001f, dt));
    cam.zoom  += (cam.target_zoom - cam.zoom) * k;
    cam.pan.x += (cam.target_pan.x - cam.pan.x) * k;
    cam.pan.y += (cam.target_pan.y - cam.pan.y) * k;
    cam.flash = std::max(0.0f, cam.flash - static_cast<float>(dt) * 2.2f);
}

// On-stage scale + zoom readout, a reference scale bar, a boundary hint, and the
// controls legend. Tasteful glass, matching the rest of the HUD.
inline void draw_cosmos_scale_hud(const CosmosState& cosmos, Rectangle stage, float scale) {
    const ScaleTier& tier = tier_for(cosmos.scale);
    const CosmosCamera& cam = cosmos.camera;

    // Tier-transition flash: a brief accent ring around the stage.
    if (cam.flash > 0.001f) {
        DrawRectangleRoundedLines(stage, 0.02f, 12, 2.4f,
                                  with_alpha(WL::CYAN_CORE, static_cast<unsigned char>(180 * cam.flash)));
    }

    // Effective length scale = tier characteristic length divided by zoom (zoom
    // in -> finer scale). Formatted in scientific notation.
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.1e m", tier.length_m / std::max(0.05, cam.zoom));
    const std::string scale_txt = buf;

    // Top-left readout chip.
    const Rectangle chip = {stage.x + 12.0f * scale, stage.y + 34.0f * scale,
                            210.0f * scale, 40.0f * scale};
    draw_glass_panel(chip, {8, 18, 30, 205}, with_alpha(WL::CYAN_DIM, 120), 0.18f, 2);
    draw_text("VIEW SCALE", {chip.x + 10.0f * scale, chip.y + 6.0f * scale},
              10.0f * scale, with_alpha(WL::CYAN_CORE, 175));
    draw_text(scale_txt, {chip.x + 10.0f * scale, chip.y + 19.0f * scale},
              16.0f * scale, WL::TEXT_PRIMARY);
    char zb[24];
    std::snprintf(zb, sizeof(zb), "%.2fx", cam.zoom);
    const Vector2 zsz = measure_ui_text(zb, 13.0f * scale);
    draw_text(zb, {chip.x + chip.width - zsz.x - 10.0f * scale, chip.y + 21.0f * scale},
              13.0f * scale, with_alpha(WL::PLASMA_GREEN, 220));

    // Vertical zoom gauge on the right edge: fill shows position within the tier
    // range; chevrons hint that pushing past either end crosses tiers.
    const float gx = stage.x + stage.width - 16.0f * scale;
    const float gy0 = stage.y + 44.0f * scale;
    const float gy1 = stage.y + stage.height - 44.0f * scale;
    DrawLineEx({gx, gy0}, {gx, gy1}, 2.0f, with_alpha(WL::GLASS_BORDER, 150));
    const float frac = static_cast<float>(
        std::clamp((cam.zoom - kZoomMin) / (kZoomMax - kZoomMin), 0.0, 1.0));
    const float knob_y = gy1 - frac * (gy1 - gy0); // zoomed-in (high) at the top
    DrawCircleV({gx, knob_y}, 4.5f * scale, WL::CYAN_CORE);
    DrawCircleV({gx, knob_y}, 7.0f * scale, with_alpha(WL::CYAN_CORE, 70));
    const int idx = static_cast<int>(scale_index(cosmos.scale));
    if (idx > 0)
        draw_text("zoom in v", {gx - 64.0f * scale, gy0 - 14.0f * scale}, 9.5f * scale,
                  with_alpha(WL::TEXT_TERTIARY, 180));
    if (idx < static_cast<int>(kScaleCount) - 1)
        draw_text("zoom out ^", {gx - 70.0f * scale, gy1 + 4.0f * scale}, 9.5f * scale,
                  with_alpha(WL::TEXT_TERTIARY, 180));

    // Controls legend along the bottom of the stage.
    draw_text("scroll: zoom (flows through tiers)   |   drag / WASD: pan   |   "
              "[ ]: step tier   |   1-9: jump   |   0: reset",
              {stage.x + 12.0f * scale, stage.y + stage.height - 18.0f * scale},
              10.5f * scale, with_alpha(WL::TEXT_TERTIARY, 200));
}

} // namespace cosmos_ui
