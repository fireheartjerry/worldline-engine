#pragma once
#include "CanvasOverlayLayout.hpp"
#include "UiPrimitives.hpp"
#include "../physics/FlowField.hpp"
#include "../physics/Simulation.hpp"
#include "../renderer/SceneLayout.hpp"
#include <algorithm>
#include <cstdio>
#include <string>

struct CanvasOverlayView {
    bool show_vectors = false;
    bool rigid_mode = true;
    bool ambient_flow_enabled = false;
    const char* mode_label = "";
    const char* preset_label = "";
    std::string hint;
    Simulation::VisualDiagnostics diagnostics{};
    FlowFieldParams flow_field{};
    double dissipation_power = 0.0;
};

inline Color hud_velocity_accent() { return {102, 232, 255, 255}; }
inline Color hud_gravity_accent() { return {112, 142, 255, 255}; }
inline Color hud_drag_accent() { return {255, 159, 102, 255}; }
inline Color hud_reaction_accent() { return {140, 247, 182, 255}; }
inline Color hud_net_force_accent() { return {255, 110, 191, 255}; }
inline Color hud_link_drag_accent() { return {255, 214, 120, 255}; }
inline Color hud_joint_torque_accent() { return {255, 116, 145, 255}; }
inline Color hud_flow_accent() { return {112, 224, 255, 255}; }

inline void draw_probe_row(Rectangle rect,
                           const char* tag,
                           Color accent,
                           const char* label,
                           const std::string& value,
                           float scale) {
    DrawRectangleRounded(rect, 0.18f, 10, {9, 17, 25, 220});
    DrawRectangleRoundedLines(rect, 0.18f, 10, 1.0f, {42, 72, 84, 120});

    Rectangle chip = {rect.x + 10.0f * scale, rect.y + 8.0f * scale, 34.0f * scale, 18.0f * scale};
    DrawRectangleRounded(chip, 0.45f, 8, with_alpha(accent, 205));
    const float tag_size_px = 13.0f * scale;
    const Vector2 tag_size = measure_ui_text(tag, tag_size_px);
    draw_text(tag,
              {chip.x + (chip.width - tag_size.x) * 0.5f,
               chip.y + (chip.height - tag_size.y) * 0.5f},
              tag_size_px,
              {8, 18, 24, 245});

    draw_text(label, {rect.x + 54.0f * scale, rect.y + 6.0f * scale}, 13.0f * scale, {155, 186, 201, 220});
    const float value_text_size = fit_ui_text_size(value,
                                                   rect.width - 68.0f * scale,
                                                   18.0f * scale,
                                                   13.0f * scale);
    const Vector2 value_size = measure_ui_text(value, value_text_size);
    draw_text(value,
              {rect.x + rect.width - value_size.x - 12.0f * scale, rect.y + 18.0f * scale},
              value_text_size,
              {236, 243, 247, 255});
}

inline void draw_force_block(Rectangle rect,
                             const char* title,
                             Color accent,
                             const Simulation::BodyDiagnostics& body,
                             Vec2 ambient_flow,
                             bool ambient_flow_enabled,
                             float scale) {
    draw_card(rect, {8, 17, 25, 206}, with_alpha(accent, 90));
    draw_text(title, {rect.x + 12.0f * scale, rect.y + 10.0f * scale}, 18.0f * scale, {236, 243, 247, 255});
    draw_text(body.constrained ? "Constrained" : "Free",
              {rect.x + rect.width - 100.0f * scale, rect.y + 12.0f * scale},
              13.0f * scale,
              {150, 185, 202, 215});

    auto magnitude_text = [](Vec2 value, int precision, const char* unit) {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.*f %s", precision, value.length(), unit);
        return std::string(buffer);
    };

    const float row_height = 42.0f * scale;
    const float row_gap = 4.0f * scale;
    float y = rect.y + 40.0f * scale;
    draw_probe_row({rect.x + 10.0f * scale, y, rect.width - 20.0f * scale, row_height},
                   "v", hud_velocity_accent(), "Speed",
                   magnitude_text(body.velocity, 2, "m/s"),
                   scale);
    y += row_height + row_gap;
    draw_probe_row({rect.x + 10.0f * scale, y, rect.width - 20.0f * scale, row_height},
                   "Uf", hud_flow_accent(), "Ambient Flow",
                   ambient_flow_enabled ? magnitude_text(ambient_flow, 2, "m/s")
                                        : std::string("0.00 m/s"),
                   scale);
    y += row_height + row_gap;
    draw_probe_row({rect.x + 10.0f * scale, y, rect.width - 20.0f * scale, row_height},
                   "Db", hud_drag_accent(), "Bob Drag",
                   magnitude_text(body.drag_force, 2, "N"),
                   scale);
    y += row_height + row_gap;
    draw_probe_row({rect.x + 10.0f * scale, y, rect.width - 20.0f * scale, row_height},
                   "R", hud_reaction_accent(), "Constraint",
                   magnitude_text(body.reaction_force, 2, "N"),
                   scale);
    y += row_height + row_gap;
    draw_probe_row({rect.x + 10.0f * scale, y, rect.width - 20.0f * scale, row_height},
                   "F", hud_net_force_accent(), "Net Force",
                   magnitude_text(body.net_force, 2, "N"),
                   scale);
}

inline std::string format_scalar(double value, int precision, const char* unit) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f %s", precision, value, unit);
    return std::string(buffer);
}

inline void draw_force_inspector(const CanvasOverlayView& view,
                                 const CanvasOverlayRects& hud) {
    if (!hud.show_inspector) {
        return;
    }

    const float scale = hud.hud_scale;
    const bool stacked = hud.inspector_stacked;
    const float body_block_height = 270.0f * scale;
    const float summary_row_height = 34.0f * scale;
    const float summary_gap = 8.0f * scale;
    const float body_gap = (stacked ? 10.0f : 12.0f) * scale;
    const float header_height = 62.0f * scale;
    const Rectangle card = hud.inspector;
    draw_card(card, {7, 15, 22, 188}, {55, 96, 115, 110});
    draw_text("Force Inspector", {card.x + 14.0f * scale, card.y + 12.0f * scale}, 22.0f * scale, {236, 243, 247, 255});
    draw_text("Tags match the labels drawn on the stage.",
              {card.x + 14.0f * scale, card.y + 38.0f * scale},
              14.0f * scale,
              {150, 185, 202, 220});

    const float block_gap = 12.0f * scale;
    const float block_width = stacked
        ? card.width - 28.0f * scale
        : (card.width - 28.0f * scale - block_gap) * 0.5f;
    const float block_y = card.y + header_height;
    draw_force_block({card.x + 14.0f * scale, block_y, block_width, body_block_height},
                     "Upper Bob",
                     hud_velocity_accent(),
                     view.diagnostics.bob1,
                     flow_velocity_at(view.diagnostics.bob1.position, view.flow_field),
                     view.ambient_flow_enabled,
                     scale);
    draw_force_block({stacked ? card.x + 14.0f * scale : card.x + 14.0f * scale + block_width + block_gap,
                      stacked ? block_y + body_block_height + body_gap : block_y,
                      block_width,
                      body_block_height},
                     "Lower Bob",
                     hud_drag_accent(),
                     view.diagnostics.bob2,
                     flow_velocity_at(view.diagnostics.bob2.position, view.flow_field),
                     view.ambient_flow_enabled,
                     scale);

    float y = block_y + (stacked ? body_block_height * 2.0f + body_gap : body_block_height) + summary_gap;
    const float summary_width = stacked
        ? card.width - 28.0f * scale
        : block_width;
    draw_probe_row({card.x + 14.0f * scale, y, summary_width, summary_row_height},
                   "Dl1", hud_link_drag_accent(), "Upper Link",
                   format_scalar(view.diagnostics.connector1.drag_force.length(), 2, "N"),
                   scale);
    draw_probe_row({stacked ? card.x + 14.0f * scale : card.x + 14.0f * scale + block_width + block_gap,
                    stacked ? y + summary_row_height + 8.0f * scale : y,
                    summary_width,
                    summary_row_height},
                   "Dl2",
                   hud_link_drag_accent(),
                   "Lower Link",
                   format_scalar(view.diagnostics.connector2.drag_force.length(), 2, "N"),
                   scale);
    y += stacked ? (summary_row_height + 8.0f * scale) * 2.0f : summary_row_height + 8.0f * scale;

    if (view.rigid_mode) {
        draw_probe_row({card.x + 14.0f * scale, y, summary_width, summary_row_height},
                       "Tp", hud_joint_torque_accent(), "Pivot Torque",
                       format_scalar(view.diagnostics.pivot_torque, 2, "Nm"),
                       scale);
        draw_probe_row({stacked ? card.x + 14.0f * scale : card.x + 14.0f * scale + block_width + block_gap,
                        stacked ? y + summary_row_height + 8.0f * scale : y,
                        summary_width,
                        summary_row_height},
                       "Te",
                       hud_joint_torque_accent(),
                       "Elbow Torque",
                       format_scalar(view.diagnostics.elbow_torque, 2, "Nm"),
                       scale);
        y += stacked ? (summary_row_height + 8.0f * scale) * 2.0f : summary_row_height + 8.0f * scale;
    }

    draw_probe_row({card.x + 14.0f * scale, y, card.width - 28.0f * scale, summary_row_height},
                   "P", hud_net_force_accent(), "Power Exchange",
                   format_scalar(view.dissipation_power, 3, "W"),
                   scale);
}

inline void draw_canvas_overlay(const CanvasOverlayView& view,
                                const PendulumLayout& layout) {
    const CanvasOverlayRects hud =
        make_canvas_overlay_layout(layout.viewport, view.show_vectors, view.rigid_mode);
    const float scale = hud.hud_scale;
    const Rectangle title_bar = hud.title_bar;
    draw_card(title_bar, {7, 16, 24, 182}, {55, 96, 115, 110});
    draw_text("Double Pendulum Lab", {title_bar.x + 16.0f * scale, title_bar.y + 12.0f * scale}, 27.0f * scale, {237, 244, 248, 255});
    draw_text("Build the launch state visually, then watch it unfold.",
              {title_bar.x + 16.0f * scale, title_bar.y + 43.0f * scale},
              16.0f * scale,
              {139, 177, 195, 225});
    if (view.show_vectors) {
        draw_badge({title_bar.x + title_bar.width - 136.0f * scale, title_bar.y + 12.0f * scale, 94.0f * scale, 24.0f * scale},
                   "FIELD ON",
                   {53, 38, 89, 220},
                   {233, 226, 255, 255},
                   scale);
        draw_badge({title_bar.x + title_bar.width - 38.0f * scale, title_bar.y + 12.0f * scale, 28.0f * scale, 24.0f * scale},
                   view.preset_label,
                   {24, 68, 86, 210},
                   {231, 243, 248, 255},
                   scale);
        draw_force_inspector(view, hud);
    }

    const Rectangle hint_bar = hud.hint_bar;
    draw_card(hint_bar, {8, 15, 22, 175}, {44, 78, 92, 120});
    draw_text_block(view.hint,
                    {hint_bar.x + 14.0f * scale,
                     hint_bar.y + 8.0f * scale,
                     hint_bar.width - 28.0f * scale,
                     hint_bar.height - 10.0f * scale},
                    17.0f * scale,
                    {176, 207, 221, 215},
                    3.0f * scale);
}
