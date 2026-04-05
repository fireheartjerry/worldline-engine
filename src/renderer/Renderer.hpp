#pragma once
#include "raylib.h"
#include "SceneLayout.hpp"
#include "Trail.hpp"
#include "VectorOverlay.hpp"
#include "../physics/Simulation.hpp"
#include "../ui/CanvasOverlayLayout.hpp"
#include <algorithm>
#include <cmath>

class Renderer {
public:
    Renderer(int w, int h) { resize_texture(w, h); }

    ~Renderer() {
        if (has_texture) {
            UnloadRenderTexture(trail_tex);
        }
    }

    void ensure_size(int w, int h) {
        if (w != width || h != height) {
            resize_texture(w, h);
        }
    }

    void reset_trail() {
        BeginTextureMode(trail_tex);
        ClearBackground({0, 0, 0, 0});
        EndTextureMode();
    }

    PendulumLayout make_layout(Rectangle viewport,
                               bool show_vectors,
                               bool rigid_mode,
                               Vec2 bob1_world,
                               Vec2 bob2_world,
                               double reach) const {
        const CanvasOverlayRects hud =
            make_canvas_overlay_layout(viewport, show_vectors, rigid_mode);
        const Rectangle stage = hud.stage_rect;
        const double clamped_reach = std::max(0.6, reach);
        const float pad = std::clamp(std::min(stage.width, stage.height) * 0.06f, 24.0f, 56.0f);
        const float usable_width = std::max(180.0f, stage.width - pad * 2.0f);
        const float usable_height = std::max(180.0f, stage.height - pad * 2.0f);
        const float max_radius = std::min(usable_width, usable_height) / static_cast<float>(clamped_reach * 2.0);
        (void)bob1_world;
        (void)bob2_world;

        PendulumLayout layout;
        layout.viewport = viewport;
        layout.stage_rect = stage;
        layout.pivot = {
            stage.x + stage.width * 0.5f,
            stage.y + stage.height * 0.5f
        };
        layout.scale = max_radius;
        return layout;
    }

    Vector2 to_screen(Vec2 world, const PendulumLayout& layout) const {
        return {
            layout.pivot.x + static_cast<float>(world.x) * layout.scale,
            layout.pivot.y + static_cast<float>(world.y) * layout.scale
        };
    }

    Vec2 to_world(Vector2 screen, const PendulumLayout& layout) const {
        return {
            (screen.x - layout.pivot.x) / layout.scale,
            (screen.y - layout.pivot.y) / layout.scale
        };
    }

    float bob_radius(double mass) const {
        const double clamped = std::clamp(mass, 0.1, 12.0);
        return 8.0f + static_cast<float>(std::sqrt(clamped) * 4.8);
    }

    template<std::size_t N>
    void advance_trail(const Trail<N>& trail,
                       int new_count,
                       const PendulumLayout& layout,
                       unsigned char fade_alpha) {
        if (!has_texture || new_count <= 0) {
            return;
        }

        BeginTextureMode(trail_tex);
        DrawRectangle(
            static_cast<int>(layout.viewport.x),
            static_cast<int>(layout.viewport.y),
            static_cast<int>(layout.viewport.width),
            static_cast<int>(layout.viewport.height),
            {10, 17, 24, fade_alpha}
        );

        BeginBlendMode(BLEND_ADDITIVE);

        const std::size_t count = trail.size();
        const std::size_t start = (count > static_cast<std::size_t>(new_count + 1))
            ? count - static_cast<std::size_t>(new_count) - 1u
            : 0u;

        for (std::size_t i = start; i + 1 < count; ++i) {
            const auto& a = trail.at(i);
            const auto& b = trail.at(i + 1);
            const Vector2 p0 = to_screen(a.pos, layout);
            const Vector2 p1 = to_screen(b.pos, layout);
            const float segment_pixels = std::sqrt((p1.x - p0.x) * (p1.x - p0.x)
                                                   + (p1.y - p0.y) * (p1.y - p0.y));
            if (segment_pixels < 0.6f) {
                continue;
            }

            const float speed_factor =
                std::min(std::abs(static_cast<float>(b.angular_velocity)) / 16.0f, 1.0f);
            const float glow_width = 6.0f + speed_factor * 4.0f;
            const float core_width = 1.8f + speed_factor * 1.8f;
            Color glow = omega_to_color(b.angular_velocity);
            Color halo = glow;
            halo.a = static_cast<unsigned char>(34 + speed_factor * 22.0f);
            DrawLineEx(p0, p1, glow_width, halo);

            glow.a = static_cast<unsigned char>(120 + speed_factor * 60.0f);
            DrawLineEx(p0, p1, core_width, glow);
            DrawCircleGradient(static_cast<int>(p1.x),
                               static_cast<int>(p1.y),
                               2.0f + speed_factor * 2.2f,
                               glow,
                               {0, 0, 0, 0});
        }

        EndBlendMode();
        EndTextureMode();
    }

    void draw_scene(const Simulation& simulation,
                    const PendulumLayout& layout,
                    const VectorOverlayConfig& overlay,
                    const Simulation::VisualDiagnostics* diagnostics) const {
        const PendulumParams& params = simulation.params;
        const Vec2 b1_world = simulation.bob1_pos();
        const Vec2 b2_world = simulation.bob2_pos();
        const Vector2 b1 = to_screen(b1_world, layout);
        const Vector2 b2 = to_screen(b2_world, layout);
        const float r1 = bob_radius(params.m1);
        const float r2 = bob_radius(params.m2);

        draw_canvas_background(layout);

        BeginScissorMode(
            static_cast<int>(layout.viewport.x),
            static_cast<int>(layout.viewport.y),
            static_cast<int>(layout.viewport.width),
            static_cast<int>(layout.viewport.height)
        );

        const Rectangle src = {0.0f, 0.0f, static_cast<float>(width), -static_cast<float>(height)};
        DrawTextureRec(trail_tex.texture, src, {0.0f, 0.0f}, ColorAlpha(WHITE, 0.96f));

        draw_grid(layout, simulation.reach());
        draw_connectors(simulation, layout, b1, b2);
        draw_vector_overlay(simulation, layout, overlay, diagnostics);
        draw_bobs(layout.pivot, b1, b2, r1, r2, simulation.omega2());
        draw_vector_legend(layout,
                           overlay,
                           simulation.rigid_connectors(),
                           simulation.ambient_flow_enabled());

        EndScissorMode();

        DrawRectangleRoundedLines(layout.viewport, 0.028f, 20, 2.0f, {70, 104, 120, 180});
    }

private:
    int width = 0;
    int height = 0;
    bool has_texture = false;
    RenderTexture2D trail_tex{};

    void resize_texture(int w, int h) {
        if (has_texture) {
            UnloadRenderTexture(trail_tex);
        }
        width = w;
        height = h;
        trail_tex = LoadRenderTexture(width, height);
        has_texture = true;
        reset_trail();
    }

    Color omega_to_color(double omega) const {
        const float speed = std::min(std::abs(static_cast<float>(omega)) / 18.0f, 1.0f);
        const float hue = 205.0f - speed * 175.0f;
        return ColorFromHSV(hue, 0.75f + speed * 0.2f, 1.0f);
    }

    void draw_canvas_background(const PendulumLayout& layout) const {
        DrawRectangleRounded(layout.viewport, 0.028f, 20, {7, 14, 21, 230});
        DrawCircleGradient(
            static_cast<int>(layout.pivot.x),
            static_cast<int>(layout.pivot.y),
            std::min(layout.stage_rect.width, layout.stage_rect.height) * 0.44f,
            {24, 65, 82, 70},
            {0, 0, 0, 0}
        );
    }

    void draw_grid(const PendulumLayout& layout, double reach) const {
        const float max_radius = layout.scale * static_cast<float>(reach);
        const Color ring = {64, 92, 108, 65};
        const Color axis = {82, 120, 138, 55};

        DrawLineV(
            {layout.stage_rect.x, layout.pivot.y},
            {layout.stage_rect.x + layout.stage_rect.width, layout.pivot.y},
            axis
        );
        DrawLineV(
            {layout.pivot.x, layout.stage_rect.y},
            {layout.pivot.x, layout.stage_rect.y + layout.stage_rect.height},
            axis
        );

        for (int i = 1; i <= 4; ++i) {
            const float radius = max_radius * (static_cast<float>(i) / 4.0f);
            DrawCircleLinesV(layout.pivot, radius, ring);
        }
    }

    void draw_connectors(const Simulation& simulation,
                         const PendulumLayout& layout,
                         Vector2 b1,
                         Vector2 b2) const {
        if (simulation.rigid_connectors()) {
            draw_rigid_connector(layout.pivot, b1, simulation.params.connector1_mass);
            draw_rigid_connector(b1, b2, simulation.params.connector2_mass);

            if (simulation.params.connector1_mass > 1e-5) {
                draw_connector_mass(
                    to_screen(simulation.connector1_com(), layout),
                    simulation.params.connector1_mass
                );
            }
            if (simulation.params.connector2_mass > 1e-5) {
                draw_connector_mass(
                    to_screen(simulation.connector2_com(), layout),
                    simulation.params.connector2_mass
                );
            }
            return;
        }

        draw_rope(layout.pivot, b1, simulation.connector1_taut());
        draw_rope(b1, b2, simulation.connector2_taut());
    }

    void draw_rigid_connector(Vector2 from, Vector2 to, double connector_mass) const {
        const float heft = 5.4f + static_cast<float>(std::sqrt(std::max(0.0, connector_mass)) * 2.2);
        const Color glow = connector_mass > 1e-5 ? Color{132, 115, 84, 72} : Color{79, 124, 168, 70};
        const Color core = connector_mass > 1e-5 ? Color{220, 195, 152, 215} : Color{187, 213, 232, 210};

        DrawLineEx(from, to, heft + 2.8f, glow);
        DrawLineEx(from, to, heft, core);
        DrawLineEx(from, to, 1.4f, {245, 247, 250, 120});
    }

    void draw_rope(Vector2 from, Vector2 to, bool taut) const {
        if (taut) {
            DrawLineEx(from, to, 3.0f, {212, 225, 235, 185});
            DrawLineEx(from, to, 1.0f, {250, 250, 250, 90});
            return;
        }

        const Vector2 delta = {to.x - from.x, to.y - from.y};
        constexpr int segments = 10;
        for (int i = 0; i < segments; ++i) {
            if (i % 2 == 1) {
                continue;
            }
            const float t0 = static_cast<float>(i) / segments;
            const float t1 = static_cast<float>(i + 1) / segments;
            const Vector2 p0 = {from.x + delta.x * t0, from.y + delta.y * t0};
            const Vector2 p1 = {from.x + delta.x * t1, from.y + delta.y * t1};
            DrawLineEx(p0, p1, 2.0f, {188, 200, 208, 90});
        }
    }

    void draw_connector_mass(Vector2 centre, double mass) const {
        const float radius = 4.0f + static_cast<float>(std::sqrt(std::max(0.0, mass)) * 2.0);
        draw_glow_circle(centre, radius, {255, 208, 122, 255});
    }

    void draw_bobs(Vector2 pivot,
                   Vector2 b1,
                   Vector2 b2,
                   float radius1,
                   float radius2,
                   double omega2) const {
        draw_glow_circle(pivot, 5.0f, {223, 238, 250, 255});
        draw_glow_circle(b1, radius1, {110, 215, 255, 255});

        Color bob2 = omega_to_color(omega2);
        bob2.a = 255;
        draw_glow_circle(b2, radius2, bob2);
    }

    void draw_glow_circle(Vector2 centre, float radius, Color color) const {
        const Color transparent = {0, 0, 0, 0};

        BeginBlendMode(BLEND_ADDITIVE);
        Color halo = color;
        halo.a = 32;
        DrawCircleGradient(
            static_cast<int>(centre.x),
            static_cast<int>(centre.y),
            radius * 4.2f,
            halo,
            transparent
        );
        Color rim = color;
        rim.a = 70;
        DrawCircleGradient(
            static_cast<int>(centre.x),
            static_cast<int>(centre.y),
            radius * 2.0f,
            rim,
            transparent
        );
        EndBlendMode();

        Color core = color;
        core.a = 220;
        DrawCircleGradient(
            static_cast<int>(centre.x),
            static_cast<int>(centre.y),
            radius,
            WHITE,
            core
        );
    }
};
