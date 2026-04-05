#pragma once
#include "raylib.h"
#include "SceneLayout.hpp"
#include "../physics/FlowField.hpp"
#include "../physics/Simulation.hpp"
#include "../ui/CanvasOverlayLayout.hpp"
#include "../ui/UiPrimitives.hpp"
#include <algorithm>
#include <cmath>

struct VectorOverlayConfig {
    bool enabled = true;
    bool show_velocity = true;
    bool show_gravity = true;
    bool show_drag = true;
    bool show_reaction = true;
    bool show_net = false;
    bool show_link_drag = true;
    bool show_joint_torque = true;
    bool show_flow_field = false;
    double velocity_scale = 0.24;
    double force_scale = 0.038;
    double flow_scale = 0.22;
    double flow_density = 1.0;
};

namespace vector_overlay_detail {

inline Color velocity_color() { return {102, 232, 255, 255}; }
inline Color gravity_color() { return {112, 142, 255, 255}; }
inline Color drag_color() { return {255, 159, 102, 255}; }
inline Color reaction_color() { return {140, 247, 182, 255}; }
inline Color net_color() { return {255, 110, 191, 255}; }
inline Color link_drag_color() { return {255, 214, 120, 255}; }
inline Color joint_torque_color() { return {255, 116, 145, 255}; }
inline Color flow_field_color() { return {112, 224, 255, 255}; }

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

inline void draw_flow_field(const Simulation& simulation,
                            const PendulumLayout& layout,
                            const VectorOverlayConfig& overlay) {
    if (!overlay.show_flow_field || !simulation.ambient_flow_enabled()) {
        return;
    }

    const float hud_scale = canvas_overlay_scale(layout.viewport);
    const float density = std::clamp(static_cast<float>(overlay.flow_density), 0.5f, 2.0f);
    const float aspect = layout.stage_rect.width / std::max(layout.stage_rect.height, 1.0f);
    const int rows = std::clamp(static_cast<int>(std::round(5.0f * density)), 4, 10);
    const int cols = std::clamp(static_cast<int>(std::round(rows * aspect * 1.15f)), 5, 14);
    const float margin = 28.0f * hud_scale;
    const double streamline_step = simulation.reach() * 0.07;
    const int streamline_segments = 5;

    auto in_bounds = [&](Vector2 point) {
        return point.x >= layout.stage_rect.x + 2.0f
            && point.x <= layout.stage_rect.x + layout.stage_rect.width - 2.0f
            && point.y >= layout.stage_rect.y + 2.0f
            && point.y <= layout.stage_rect.y + layout.stage_rect.height - 2.0f;
    };

    for (int row = 0; row < rows; ++row) {
        const float row_t = (rows == 1)
            ? 0.5f
            : static_cast<float>(row) / static_cast<float>(rows - 1);
        for (int col = 0; col < cols; ++col) {
            const float col_t = (cols == 1)
                ? 0.5f
                : static_cast<float>(col) / static_cast<float>(cols - 1);
            const Vector2 seed_screen = {
                layout.stage_rect.x + margin + (layout.stage_rect.width - margin * 2.0f) * col_t,
                layout.stage_rect.y + margin + (layout.stage_rect.height - margin * 2.0f) * row_t
            };
            Vec2 seed_world = {
                (seed_screen.x - layout.pivot.x) / layout.scale,
                (seed_screen.y - layout.pivot.y) / layout.scale
            };
            const Vec2 seed_flow = simulation.flow_velocity(seed_world);
            const double speed = seed_flow.length();
            if (speed < 1e-4) {
                continue;
            }

            const unsigned char alpha = static_cast<unsigned char>(
                std::clamp(46.0 + speed * 32.0, 46.0, 120.0));
            Color trail = flow_field_color();
            trail.a = alpha;

            Vec2 world_cursor = seed_world;
            Vector2 previous = seed_screen;
            Vector2 last = seed_screen;
            for (int segment = 0; segment < streamline_segments; ++segment) {
                Vec2 velocity = simulation.flow_velocity(world_cursor);
                const double local_speed = velocity.length();
                if (local_speed < 1e-4) {
                    break;
                }

                Vec2 direction = velocity / local_speed;
                const Vec2 midpoint = world_cursor + direction * (streamline_step * 0.5);
                const Vec2 midpoint_velocity = simulation.flow_velocity(midpoint);
                if (midpoint_velocity.length() > 1e-4) {
                    direction = midpoint_velocity / midpoint_velocity.length();
                }

                world_cursor += direction * streamline_step;
                const Vector2 current = to_screen(world_cursor, layout);
                if (!in_bounds(current)) {
                    break;
                }
                DrawLineEx(previous, current, 1.1f, trail);
                last = current;
                previous = current;
            }

            Color arrow_color = flow_field_color();
            arrow_color.a = static_cast<unsigned char>(
                std::clamp(80.0 + speed * 35.0, 80.0, 165.0));
            const Vector2 arrow_delta = clamp_delta(
                world_vector_to_screen(seed_flow, overlay.flow_scale, layout),
                46.0f);
            draw_arrow(seed_screen, arrow_delta, arrow_color, 1.4f);

            const Vector2 trail_delta = {last.x - seed_screen.x, last.y - seed_screen.y};
            if (length(trail_delta) > 14.0f) {
                DrawCircleV(last, 1.5f, arrow_color);
            }
        }
    }
}

} // namespace vector_overlay_detail

inline void draw_vector_overlay(const Simulation& simulation,
                                const PendulumLayout& layout,
                                const VectorOverlayConfig& overlay,
                                const Simulation::VisualDiagnostics* diagnostics) {
    if (!overlay.enabled) {
        return;
    }

    vector_overlay_detail::draw_flow_field(simulation, layout, overlay);
    if (diagnostics == nullptr) {
        return;
    }
    const Vector2 offset_velocity = {-18.0f, -18.0f};
    const Vector2 offset_gravity = {18.0f, -18.0f};
    const Vector2 offset_drag = {-18.0f, 18.0f};
    const Vector2 offset_reaction = {18.0f, 18.0f};
    const Vector2 offset_net = {0.0f, 0.0f};

    auto draw_body_pack = [&](const Simulation::BodyDiagnostics& body) {
        const Vector2 anchor = vector_overlay_detail::to_screen(body.position, layout);
        if (overlay.show_velocity) {
            vector_overlay_detail::draw_body_vector(
                vector_overlay_detail::add(anchor, offset_velocity),
                body.velocity,
                overlay.velocity_scale,
                layout,
                vector_overlay_detail::velocity_color(),
                2.2f,
                "v");
        }
        if (overlay.show_gravity) {
            vector_overlay_detail::draw_body_vector(
                vector_overlay_detail::add(anchor, offset_gravity),
                body.gravity_force,
                overlay.force_scale,
                layout,
                vector_overlay_detail::gravity_color(),
                2.1f,
                "W");
        }
        if (overlay.show_drag) {
            vector_overlay_detail::draw_body_vector(
                vector_overlay_detail::add(anchor, offset_drag),
                body.drag_force,
                overlay.force_scale,
                layout,
                vector_overlay_detail::drag_color(),
                2.1f,
                "Db");
        }
        if (overlay.show_reaction && body.constrained) {
            vector_overlay_detail::draw_body_vector(
                vector_overlay_detail::add(anchor, offset_reaction),
                body.reaction_force,
                overlay.force_scale,
                layout,
                vector_overlay_detail::reaction_color(),
                2.1f,
                "R");
        }
        if (overlay.show_net) {
            vector_overlay_detail::draw_body_vector(
                vector_overlay_detail::add(anchor, offset_net),
                body.net_force,
                overlay.force_scale,
                layout,
                vector_overlay_detail::net_color(),
                2.5f,
                "F");
        }
    };

    draw_body_pack(diagnostics->bob1);
    draw_body_pack(diagnostics->bob2);

    if (overlay.show_link_drag) {
        auto draw_link_drag = [&](const Simulation::LinkDiagnostics& link) {
            if (!link.active) {
                return;
            }
            const Vector2 origin = vector_overlay_detail::to_screen(link.position, layout);
            vector_overlay_detail::draw_link_basis(origin, link.direction, layout);
            vector_overlay_detail::draw_body_vector(origin,
                                                    link.drag_force,
                                                    overlay.force_scale,
                                                    layout,
                                                    vector_overlay_detail::link_drag_color(),
                                                    2.0f,
                                                    "Dl");
        };
        draw_link_drag(diagnostics->connector1);
        draw_link_drag(diagnostics->connector2);
    }

    if (overlay.show_joint_torque && simulation.rigid_connectors()) {
        auto draw_joint_torque = [&](Vector2 center,
                                     double torque,
                                     bool active,
                                     float base_radius,
                                     const char* label) {
            if (!active || std::abs(torque) < 1e-5) {
                return;
            }

            const float radius = std::clamp(
                base_radius + static_cast<float>(std::abs(torque)
                                                 * overlay.force_scale
                                                 * layout.scale * 0.52),
                base_radius,
                base_radius + 30.0f);
            const float start = (torque >= 0.0) ? -140.0f : 140.0f;
            const float sweep = (torque >= 0.0) ? 118.0f : -118.0f;
            vector_overlay_detail::draw_arc_arrow(center,
                                                  radius,
                                                  start,
                                                  sweep,
                                                  vector_overlay_detail::joint_torque_color(),
                                                  2.0f);
            const Vector2 label_anchor = vector_overlay_detail::clamp_point_to_rect(
                vector_overlay_detail::add(center,
                                           vector_overlay_detail::polar(radius + 18.0f,
                                                                        start + sweep * 0.55f)),
                layout.viewport,
                18.0f);
            vector_overlay_detail::draw_label_chip(label_anchor,
                                                   label,
                                                   vector_overlay_detail::joint_torque_color(),
                                                   layout);
        };

        draw_joint_torque(layout.pivot,
                          diagnostics->pivot_torque,
                          diagnostics->pivot_active,
                          22.0f,
                          "Tp");
        draw_joint_torque(vector_overlay_detail::to_screen(diagnostics->bob1.position, layout),
                          diagnostics->elbow_torque,
                          diagnostics->elbow_active,
                          18.0f,
                          "Te");
    }
}

inline void draw_vector_legend(const PendulumLayout& layout,
                               const VectorOverlayConfig& overlay,
                               bool rigid_mode,
                               bool flow_active) {
    if (!overlay.enabled) {
        return;
    }

    struct LegendItem {
        const char* label;
        Color color;
    };

    LegendItem items[8];
    int count = 0;
    if (overlay.show_flow_field && flow_active) items[count++] = {"Ambient Flow", vector_overlay_detail::flow_field_color()};
    if (overlay.show_velocity) items[count++] = {"Velocity", vector_overlay_detail::velocity_color()};
    if (overlay.show_gravity) items[count++] = {"Gravity", vector_overlay_detail::gravity_color()};
    if (overlay.show_drag) items[count++] = {"Bob Drag", vector_overlay_detail::drag_color()};
    if (overlay.show_reaction) items[count++] = {rigid_mode ? "Reaction" : "Tension/Reaction", vector_overlay_detail::reaction_color()};
    if (overlay.show_net) items[count++] = {"Net Force", vector_overlay_detail::net_color()};
    if (overlay.show_link_drag) items[count++] = {"Link Drag", vector_overlay_detail::link_drag_color()};
    if (overlay.show_joint_torque && rigid_mode) items[count++] = {"Joint Torque", vector_overlay_detail::joint_torque_color()};
    if (count == 0) {
        return;
    }

    const bool show_basis_note = overlay.show_link_drag;
    const CanvasOverlayRects hud =
        make_canvas_overlay_layout(layout.viewport, true, rigid_mode);
    if (!hud.show_legend) {
        return;
    }

    const float scale = hud.hud_scale;
    Rectangle card = hud.legend;
    card.height = 40.0f * scale + count * 22.0f * scale + (show_basis_note ? 26.0f * scale : 0.0f);

    DrawRectangleRounded(card, 0.12f, 14, {7, 16, 24, 198});
    DrawRectangleRoundedLines(card, 0.12f, 14, 1.3f, {60, 95, 112, 116});
    DrawTextEx(ui_font(),
               "Vector Field",
               {card.x + 14.0f * scale, card.y + 10.0f * scale},
               18.0f * scale,
               ui_text_spacing(18.0f * scale),
               {235, 242, 247, 255});

    float y = card.y + 34.0f * scale;
    for (int i = 0; i < count; ++i) {
        const Vector2 from = {card.x + 16.0f * scale, y + 8.0f * scale};
        vector_overlay_detail::draw_arrow(from, {18.0f * scale, 0.0f}, items[i].color, 2.0f * scale);
        DrawTextEx(ui_font(),
                   items[i].label,
                   {card.x + 44.0f * scale, y - 1.0f * scale},
                   15.0f * scale,
                   ui_text_spacing(15.0f * scale),
                   {192, 214, 225, 230});
        y += 22.0f * scale;
    }

    if (show_basis_note) {
        DrawLineEx({card.x + 16.0f * scale, y + 8.0f * scale},
                   {card.x + 34.0f * scale, y + 8.0f * scale},
                   1.2f,
                   {120, 204, 240, 138});
        DrawLineEx({card.x + 25.0f * scale, y - 1.0f * scale},
                   {card.x + 25.0f * scale, y + 17.0f * scale},
                   1.2f,
                   {255, 212, 132, 138});
        DrawTextEx(ui_font(),
                   "Basis glyph = axial / normal",
                   {card.x + 44.0f * scale, y - 1.0f * scale},
                   15.0f * scale,
                   ui_text_spacing(15.0f * scale),
                   {192, 214, 225, 210});
    }
}
