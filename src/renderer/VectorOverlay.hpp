#pragma once
// Vector overlay — config + the overlay/legend renderers.
// Drawing primitives live in VectorOverlayPrimitives.hpp.
#include "VectorOverlayPrimitives.hpp"

struct VectorOverlayConfig {
    bool enabled = true;
    bool show_velocity = true;
    bool show_gravity = true;
    bool show_drag = true;
    bool show_reaction = true;
    bool show_net = false;
    bool show_link_drag = true;
    bool show_joint_torque = true;
    double velocity_scale = 0.24;
    double force_scale = 0.038;
};

inline void draw_vector_overlay(const Simulation& simulation,
                                const PendulumLayout& layout,
                                const VectorOverlayConfig& overlay,
                                const Simulation::VisualDiagnostics* diagnostics) {
    if (!overlay.enabled) {
        return;
    }

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
                               bool rigid_mode) {
    if (!overlay.enabled) {
        return;
    }

    struct LegendItem {
        const char* label;
        Color color;
    };

    LegendItem items[8];
    int count = 0;
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
