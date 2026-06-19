#pragma once
// Vector overlay — low-level drawing primitives and vector math.
#include "raylib.h"
#include "SceneLayout.hpp"
#include "../physics/Simulation.hpp"
#include "../ui/CanvasOverlayLayout.hpp"
#include "../ui/UiPrimitives.hpp"
#include <algorithm>
#include <cmath>

namespace vector_overlay_detail {

inline Color velocity_color() { return {102, 232, 255, 255}; }
inline Color gravity_color() { return {112, 142, 255, 255}; }
inline Color drag_color() { return {255, 159, 102, 255}; }
inline Color reaction_color() { return {140, 247, 182, 255}; }
inline Color net_color() { return {255, 110, 191, 255}; }
inline Color link_drag_color() { return {255, 214, 120, 255}; }
inline Color joint_torque_color() { return {255, 116, 145, 255}; }
inline Vector2 add(Vector2 a, Vector2 b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vector2 scale(Vector2 v, float s) {
    return {v.x * s, v.y * s};
}

inline float length(Vector2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

inline Vector2 normalize(Vector2 v) {
    const float len = length(v);
    if (len < 1e-4f) {
        return {0.0f, 0.0f};
    }
    return {v.x / len, v.y / len};
}

inline Vector2 perpendicular(Vector2 v) {
    return {-v.y, v.x};
}

inline Vector2 clamp_delta(Vector2 delta, float max_length) {
    const float len = length(delta);
    if (len <= max_length || len < 1e-4f) {
        return delta;
    }
    return scale(delta, max_length / len);
}

inline Vector2 clamp_point_to_rect(Vector2 point,
                                   Rectangle rect,
                                   float margin) {
    return {
        std::clamp(point.x, rect.x + margin, rect.x + rect.width - margin),
        std::clamp(point.y, rect.y + margin, rect.y + rect.height - margin)
    };
}

inline Vector2 polar(float radius, float degrees) {
    constexpr float pi = 3.14159265358979323846f;
    const float radians = degrees * pi / 180.0f;
    return {
        std::cos(radians) * radius,
        std::sin(radians) * radius
    };
}

inline Vector2 to_screen(Vec2 world, const PendulumLayout& layout) {
    return {
        layout.pivot.x + static_cast<float>(world.x) * layout.scale,
        layout.pivot.y + static_cast<float>(world.y) * layout.scale
    };
}

inline Vector2 world_vector_to_screen(Vec2 world_vector,
                                      double world_scale,
                                      const PendulumLayout& layout) {
    return {
        static_cast<float>(world_vector.x * world_scale) * layout.scale,
        static_cast<float>(world_vector.y * world_scale) * layout.scale
    };
}

inline void draw_arrow(Vector2 origin,
                       Vector2 delta,
                       Color color,
                       float thickness) {
    const float len = length(delta);
    if (len < 7.0f) {
        return;
    }

    const Vector2 end = add(origin, delta);
    const Vector2 dir = normalize(delta);
    const Vector2 side = perpendicular(dir);
    const float head = std::clamp(len * 0.22f, 7.0f, 14.0f);
    const float head_width = head * 0.56f;
    const Vector2 shaft_end = add(end, scale(dir, -head * 0.75f));
    const Vector2 left = add(shaft_end, scale(side, head_width * 0.5f));
    const Vector2 right = add(shaft_end, scale(side, -head_width * 0.5f));

    Color halo = color;
    halo.a = 34;
    DrawLineEx(origin, shaft_end, thickness + 4.0f, halo);
    DrawTriangle(end, left, right, halo);

    Color core = color;
    core.a = 210;
    DrawLineEx(origin, shaft_end, thickness, core);
    DrawTriangle(end, left, right, core);
    Color dot = core;
    dot.a = 180;
    DrawCircleV(origin, 2.8f, dot);
}

inline void draw_label_chip(Vector2 anchor,
                            const char* label,
                            Color color,
                            const PendulumLayout& layout) {
    if (label == nullptr || label[0] == '\0') {
        return;
    }

    const float scale = canvas_overlay_scale(layout.viewport);
    const float text_height = 13.0f * scale;
    const Vector2 text_size = measure_ui_text(label, text_height);
    Rectangle chip = {
        anchor.x - text_size.x * 0.5f - 7.0f * scale,
        anchor.y - text_size.y * 0.5f - 4.0f * scale,
        text_size.x + 14.0f * scale,
        text_size.y + 8.0f * scale
    };

    chip.x = std::clamp(chip.x,
                        layout.stage_rect.x + 8.0f * scale,
                        layout.stage_rect.x + layout.stage_rect.width - chip.width - 8.0f * scale);
    chip.y = std::clamp(chip.y,
                        layout.stage_rect.y + 8.0f * scale,
                        layout.stage_rect.y + layout.stage_rect.height - chip.height - 8.0f * scale);

    DrawRectangleRounded(chip, 0.45f, 10, {7, 15, 22, 218});
    Color rim = color;
    rim.a = 170;
    DrawRectangleRoundedLines(chip, 0.45f, 10, 1.1f, rim);
        DrawTextEx(ui_font(),
                   label,
                   {chip.x + (chip.width - text_size.x) * 0.5f,
                    chip.y + (chip.height - text_size.y) * 0.5f},
                   text_height,
                   ui_text_spacing(text_height),
                   {240, 245, 248, 245});
}

inline void draw_link_basis(Vector2 origin,
                            Vec2 axis_hint,
                            const PendulumLayout& layout) {
    const float scale = canvas_overlay_scale(layout.viewport);
    Vector2 axis = normalize({
        static_cast<float>(axis_hint.x),
        static_cast<float>(axis_hint.y)
    });
    if (length(axis) < 1e-4f) {
        return;
    }

    const Vector2 normal = perpendicular(axis);
    const Vector2 safe_origin = clamp_point_to_rect(origin, layout.stage_rect, 16.0f * scale);

    DrawLineEx(add(safe_origin, vector_overlay_detail::scale(axis, -11.0f * scale)),
               add(safe_origin, vector_overlay_detail::scale(axis, 11.0f * scale)),
               1.2f,
               {120, 204, 240, 138});
    DrawLineEx(add(safe_origin, vector_overlay_detail::scale(normal, -9.0f * scale)),
               add(safe_origin, vector_overlay_detail::scale(normal, 9.0f * scale)),
               1.2f,
               {255, 212, 132, 138});
    DrawCircleV(safe_origin, 2.2f * scale, {242, 245, 247, 185});
}

inline void draw_arc_arrow(Vector2 center,
                           float radius,
                           float start_deg,
                           float sweep_deg,
                           Color color,
                           float thickness) {
    if (std::abs(sweep_deg) < 8.0f) {
        return;
    }

    const int segments =
        std::max(8, static_cast<int>(std::ceil(std::abs(sweep_deg) / 12.0f)));
    const float direction = (sweep_deg >= 0.0f) ? 1.0f : -1.0f;

    auto draw_pass = [&](Color pass_color, float pass_thickness) {
        Vector2 previous = add(center, polar(radius, start_deg));
        for (int i = 1; i <= segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float angle = start_deg + sweep_deg * t;
            const Vector2 current = add(center, polar(radius, angle));
            DrawLineEx(previous, current, pass_thickness, pass_color);
            previous = current;
        }

        const float end_deg = start_deg + sweep_deg;
        const Vector2 end = add(center, polar(radius, end_deg));
        constexpr float pi = 3.14159265358979323846f;
        const float radians = end_deg * pi / 180.0f;
        Vector2 tangent = {
            -std::sin(radians) * direction,
            std::cos(radians) * direction
        };
        tangent = normalize(tangent);
        const Vector2 side = perpendicular(tangent);
        const float head = 11.0f;
        const float head_width = 6.5f;
        const Vector2 base = add(end, scale(tangent, -head));
        const Vector2 left = add(base, scale(side, head_width));
        const Vector2 right = add(base, scale(side, -head_width));
        DrawTriangle(end, left, right, pass_color);
    };

    Color halo = color;
    halo.a = 32;
    draw_pass(halo, thickness + 3.5f);

    Color core = color;
    core.a = 205;
    draw_pass(core, thickness);
}

inline void draw_body_vector(Vector2 origin,
                             Vec2 vector,
                             double scale_world,
                             const PendulumLayout& layout,
                             Color color,
                             float thickness,
                             const char* label) {
    const Vector2 delta = clamp_delta(
        world_vector_to_screen(vector, scale_world, layout),
        160.0f);
    if (length(delta) < 7.0f) {
        return;
    }
    draw_arrow(origin, delta, color, thickness);
    const Vector2 direction = normalize(delta);
    const Vector2 label_anchor = clamp_point_to_rect(
        add(add(origin, delta),
            add(scale(direction, 10.0f),
                scale(perpendicular(direction), 9.0f))),
        layout.viewport,
        22.0f);
    draw_label_chip(label_anchor, label, color, layout);
}

} // namespace vector_overlay_detail
