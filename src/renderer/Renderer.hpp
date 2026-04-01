#pragma once
#include "raylib.h"
#include "Trail.hpp"
#include "../physics/Universe.hpp"   // RenderState
#include "../physics/Simulation.hpp" // Simulation (Phase 1/2 draw_frame overload)
#include <algorithm>
#include <cmath>
#include <string>

// Owns the raylib RenderTexture used for the persistent glowing trail.
// All double→float conversions happen here — physics headers stay raylib-free.
class Renderer {
public:
    // Pixels per metre — scales physics units to screen pixels.
    static constexpr float SCALE = 210.0f;

    Renderer(int w, int h) : width(w), height(h) {
        trail_tex = LoadRenderTexture(w, h);
        BeginTextureMode(trail_tex);
        ClearBackground({8, 8, 12, 255});
        EndTextureMode();
    }

    ~Renderer() { UnloadRenderTexture(trail_tex); }

    // Render one visual frame.
    // Call after all physics substeps for this frame have been pushed to trail.
    // new_count = number of trail entries added this frame.
    template<std::size_t N>
    void draw_frame(const Simulation& sim, const Trail<N>& trail, int new_count) {
        const Vector2 pivot = {width * 0.5f, height * 0.35f};

        const Vec2    b1w = sim.bob1_pos();
        const Vec2    b2w = sim.bob2_pos();
        const Vector2 b1s = to_screen(b1w, pivot);
        const Vector2 b2s = to_screen(b2w, pivot);

        // 1. Accumulate new trail segments into the persistent texture.
        update_trail_texture(trail, new_count, pivot);

        // 2. Composite the final frame.
        BeginDrawing();
        ClearBackground({8, 8, 12, 255});

        // Trail texture — OpenGL render textures are bottom-up, flip the rect.
        const Rectangle src = {0.f, 0.f, (float)width, -(float)height};
        DrawTextureRec(trail_tex.texture, src, {0.f, 0.f}, WHITE);

        // Rods
        draw_rod({width * 0.5f, height * 0.35f}, b1s);
        draw_rod(b1s, b2s);

        // Pivot
        draw_glow_circle({width * 0.5f, height * 0.35f}, 3.5f, {220, 225, 255, 255});

        // Bob 1 — cool blue-white
        draw_glow_circle(b1s, 7.5f, {155, 200, 255, 255});

        // Bob 2 — hue tracks angular velocity
        Color bob2_col = omega_to_color(sim.state.omega2);
        bob2_col.a     = 255;
        draw_glow_circle(b2s, 9.5f, bob2_col);

        EndDrawing();
    }

    // ---- Phase 3: RenderState overload ----
    // Accepts the universal rendering contract produced by any Universe subclass.
    template<std::size_t N>
    void draw_frame(const RenderState& rs, const Trail<N>& trail, int new_count,
                    const std::string& description = "") {
        const Vector2 pivot = {width * 0.5f, height * 0.35f};
        const Vector2 b1s   = to_screen(rs.bob1, pivot);
        const Vector2 b2s   = to_screen(rs.bob2, pivot);

        update_trail_texture(trail, new_count, pivot);

        BeginDrawing();
        ClearBackground({8, 8, 12, 255});

        const Rectangle src = {0.f, 0.f, (float)width, -(float)height};
        DrawTextureRec(trail_tex.texture, src, {0.f, 0.f}, WHITE);

        draw_rod({width * 0.5f, height * 0.35f}, b1s);
        draw_rod(b1s, b2s);
        draw_glow_circle({width * 0.5f, height * 0.35f}, 3.5f, {220, 225, 255, 255});
        draw_glow_circle(b1s, 7.5f, {155, 200, 255, 255});

        Color bob2_col = omega_to_color(rs.angular_velocity);
        bob2_col.a     = 255;
        draw_glow_circle(b2s, 9.5f, bob2_col);

        if (!description.empty()) {
            DrawText(description.c_str(), 14, 14, 14, {160, 160, 180, 180});
        }

        EndDrawing();
    }

    // Clears the persistent trail texture (call when switching universes).
    void reset_trail() {
        BeginTextureMode(trail_tex);
        ClearBackground({8, 8, 12, 255});
        EndTextureMode();
    }

private:
    int             width;
    int             height;
    RenderTexture2D trail_tex;

    // World coords (pivot = origin, +y down) → screen pixels.
    Vector2 to_screen(Vec2 p, Vector2 pivot) const {
        return {pivot.x + (float)p.x * SCALE,
                pivot.y + (float)p.y * SCALE};
    }

    // Map angular velocity to a hue-shifted colour.
    // |omega|=0   → 240° electric blue
    // |omega|=15+ → 0°  red  (cycling through cyan, green, yellow en route)
    Color omega_to_color(double omega) const {
        const float speed = std::min(std::abs((float)omega) / 15.f, 1.f);
        const float hue   = 240.f * (1.f - speed);
        return ColorFromHSV(hue, 1.f, 1.f);
    }

    // Fade old trail content towards the background, then additively draw
    // the new segments produced this frame.
    template<std::size_t N>
    void update_trail_texture(const Trail<N>& trail, int new_count,
                               Vector2 pivot) {
        BeginTextureMode(trail_tex);

        // Fade towards background colour — alpha 7 ≈ 2.7% per frame.
        // At 60 FPS bright regions persist ~0.5 s; additive accumulation on
        // frequently-visited paths creates longer-lived glowing attractors.
        DrawRectangle(0, 0, width, height, {8, 8, 12, 7});

        BeginBlendMode(BLEND_ADDITIVE);

        const std::size_t n     = trail.size();
        const std::size_t start =
            (n > (std::size_t)(new_count + 1))
                ? n - (std::size_t)new_count - 1u
                : 0u;

        for (std::size_t i = start; i + 1 < n; ++i) {
            const auto&   e0 = trail.at(i);
            const auto&   e1 = trail.at(i + 1);
            const Vector2 p0 = to_screen(e0.pos, pivot);
            const Vector2 p1 = to_screen(e1.pos, pivot);

            Color c = omega_to_color(e1.angular_velocity);

            // Wide, dim outer glow
            Color outer = c; outer.a = 35;
            DrawLineEx(p0, p1, 5.f, outer);

            // Narrow, bright core
            c.a = 150;
            DrawLineEx(p0, p1, 1.5f, c);
        }

        EndBlendMode();
        EndTextureMode();
    }

    // Thin glowing rod between two screen points.
    void draw_rod(Vector2 from, Vector2 to) const {
        DrawLineEx(from, to, 3.5f, {80, 100, 160, 35});
        DrawLineEx(from, to, 1.5f, {170, 185, 220, 130});
    }

    // Concentric gradient circles producing a soft radial glow.
    void draw_glow_circle(Vector2 centre, float radius, Color color) const {
        const Color zero = {0, 0, 0, 0};

        // Additive halo — two expanding rings
        BeginBlendMode(BLEND_ADDITIVE);
        Color h1 = color; h1.a = 25;
        DrawCircleGradient((int)centre.x, (int)centre.y, radius * 5.f, h1, zero);
        Color h2 = color; h2.a = 55;
        DrawCircleGradient((int)centre.x, (int)centre.y, radius * 2.5f, h2, zero);
        EndBlendMode();

        // Solid core with standard alpha blend
        Color inner = color; inner.a = 200;
        DrawCircleGradient((int)centre.x, (int)centre.y, radius, WHITE, inner);
    }
};
