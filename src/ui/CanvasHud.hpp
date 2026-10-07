#pragma once
#include "CanvasOverlayLayout.hpp"
#include "UiPrimitives.hpp"
#include "../physics/Simulation.hpp"
#include "../renderer/SceneLayout.hpp"
#include <algorithm>
#include <cstdio>
#include <string>

// ── Overlay view state ────────────────────────────────────────────────────────
struct CanvasOverlayView {
    bool show_vectors          = false;
    bool rigid_mode            = true;
    const char* mode_label     = "";
    const char* preset_label   = "";
    std::string hint;
    Simulation::VisualDiagnostics diagnostics{};
    double dissipation_power   = 0.0;
};

// ── Diagnostic accent colours (from WL namespace, aliased for readability) ────
inline Color hud_velocity_accent()   { return WL::ACCENT_VELOCITY;  }
inline Color hud_gravity_accent()    { return WL::ACCENT_GRAVITY;   }
inline Color hud_drag_accent()       { return WL::ACCENT_DRAG;      }
inline Color hud_reaction_accent()   { return WL::ACCENT_REACTION;  }
inline Color hud_net_force_accent()  { return WL::ACCENT_NET;       }
inline Color hud_link_drag_accent()  { return WL::ACCENT_LINK_DRAG; }
inline Color hud_joint_torque_accent(){ return WL::ACCENT_TORQUE;   }

// ── Probe row ─────────────────────────────────────────────────────────────────
// Tag chip on the left (matches the stage marker), label, and a right-aligned
// value.  ProbeRowLayout::SINGLE puts label and value on one line, both
// vertically centred; STACKED puts the label on top and the value below-right
// so the two can never overlap in narrow rows; AUTO picks per row.
enum class ProbeRowLayout { AUTO, SINGLE, STACKED };

inline float probe_chip_width(const char* tag, float scale) {
    return std::max(26.0f * scale, measure_ui_text(tag, 11.5f * scale).x + 12.0f * scale);
}

// Would `label` plus a value as wide as `value_sample` fit on one line?
inline bool probe_row_fits_single(float row_width, const char* tag, const char* label,
                                  const char* value_sample, float scale) {
    const float used = 8.0f * scale + probe_chip_width(tag, scale) + 8.0f * scale
                     + measure_ui_text(label, 12.5f * scale).x + 10.0f * scale
                     + measure_ui_text(value_sample, 15.0f * scale).x + 10.0f * scale;
    return used <= row_width;
}

inline void draw_probe_row(Rectangle rect,
                           const char* tag,
                           Color accent,
                           const char* label,
                           const std::string& value,
                           float scale,
                           ProbeRowLayout layout = ProbeRowLayout::AUTO) {
    DrawRectangleRounded(rect, 0.06f, 6, {10, 20, 32, 218});
    DrawRectangleRoundedLines(rect, 0.06f, 6, 1.0f, with_alpha(accent, 35));
    DrawRectangle(static_cast<int>(rect.x + 1),
                  static_cast<int>(rect.y + 3),
                  2,
                  static_cast<int>(rect.height - 6),
                  with_alpha(accent, 160));

    // Tag chip, sized to its tag.
    const float tag_px = 11.5f * scale;
    const Vector2 tag_sz = measure_ui_text(tag, tag_px);
    const float chip_h = std::min(20.0f * scale, rect.height - 10.0f * scale);
    const Rectangle chip = {rect.x + 8.0f * scale, rect.y + (rect.height - chip_h) * 0.5f,
                            probe_chip_width(tag, scale), chip_h};
    DrawRectangleRounded(chip, 0.40f, 6, with_alpha(accent, 185));
    DrawRectangleRoundedLines({chip.x - 1, chip.y - 1, chip.width + 2, chip.height + 2},
                              0.45f, 6, 1.0f, with_alpha(accent, 55));
    draw_text(tag, {chip.x + (chip.width - tag_sz.x) * 0.5f, chip.y + (chip.height - tag_sz.y) * 0.5f},
              tag_px, {6, 14, 22, 255});

    const float lx = chip.x + chip.width + 8.0f * scale;
    const float right = rect.x + rect.width - 10.0f * scale;
    const float label_px = 12.5f * scale;
    const float value_px = 15.0f * scale;
    const float label_w = measure_ui_text(label, label_px).x;
    const float value_w = measure_ui_text(value, value_px).x;

    const bool single = layout == ProbeRowLayout::SINGLE
        || (layout == ProbeRowLayout::AUTO && lx + label_w + 10.0f * scale + value_w <= right);
    if (single) {
        if (lx + label_w + 10.0f * scale + value_w > right) {
            // Forced single line with an unusually long value: keep the value,
            // ellipsize the label.
            draw_text_fit(label, {lx, rect.y + (rect.height - label_px) * 0.5f},
                          std::max(0.0f, right - value_w - 10.0f * scale - lx), label_px, label_px, WL::TEXT_TERTIARY);
            draw_text(value, {right - value_w, rect.y + (rect.height - value_px) * 0.5f}, value_px, WL::TEXT_PRIMARY);
            return;
        }
        draw_text(label, {lx, rect.y + (rect.height - label_px) * 0.5f}, label_px, WL::TEXT_TERTIARY);
        draw_text(value, {right - value_w, rect.y + (rect.height - value_px) * 0.5f}, value_px, WL::TEXT_PRIMARY);
    } else {
        const float small_label = 11.0f * scale;
        draw_text_fit(label, {lx, rect.y + 3.0f * scale}, right - lx, small_label, small_label, WL::TEXT_TERTIARY);
        const float vy = rect.y + rect.height - 3.0f * scale - 13.5f * scale;
        const float size = fit_ui_text_size(value, right - lx, 13.5f * scale, 10.5f * scale);
        const float vw = measure_ui_text(value, size).x;
        draw_text(value, {std::max(lx, right - vw), vy}, size, WL::TEXT_PRIMARY);
    }
}

// ── Force block ───────────────────────────────────────────────────────────────
inline void draw_force_block(Rectangle rect,
                             const char* title,
                             Color accent,
                             const Simulation::BodyDiagnostics& body,
                             float scale,
                             ProbeRowLayout layout = ProbeRowLayout::AUTO) {
    draw_card_accented(rect, WL::GLASS_1, with_alpha(accent, 70), accent);

    // Constrained / Free badge — top right, sized to its label.
    const bool constrained = body.constrained;
    const char* state = constrained ? "BOUND" : "FREE";
    const float badge_w = measure_ui_text(state, 11.5f * scale).x + 22.0f * scale;
    const Color badge_fill = constrained ? with_alpha(WL::XENON_DIM, 180) : with_alpha(WL::PLASMA_DIM, 160);
    const Color badge_text = constrained ? WL::XENON_CORE : WL::PLASMA_GREEN;
    draw_badge({rect.x + rect.width - badge_w - 10.0f * scale, rect.y + 9.0f * scale, badge_w, 19.0f * scale},
               state, badge_fill, badge_text, scale * 0.85f);

    draw_text_fit(title, {rect.x + 14.0f * scale, rect.y + 10.0f * scale},
                  rect.width - badge_w - 34.0f * scale, 17.0f * scale, 13.0f * scale, WL::TEXT_PRIMARY);

    auto magnitude_text = [](Vec2 v, int prec, const char* unit) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f %s", prec, v.length(), unit);
        return std::string(buf);
    };

    const float rh   = canvas_probe_row_height(scale);
    const float rgap = canvas_probe_row_gap(scale);
    float y = rect.y + 38.0f * scale;

    auto row = [&](const char* tag, Color ac, const char* lbl, const std::string& val) {
        draw_probe_row({rect.x + 8.0f * scale, y, rect.width - 16.0f * scale, rh},
                       tag, ac, lbl, val, scale, layout);
        y += rh + rgap;
    };

    row("v",  hud_velocity_accent(), "Speed",      magnitude_text(body.velocity, 2, "m/s"));
    row("Db", hud_drag_accent(),     "Bob Drag",   magnitude_text(body.drag_force, 2, "N"));
    row("R",  hud_reaction_accent(), "Constraint", magnitude_text(body.reaction_force, 2, "N"));
    row("F",  hud_net_force_accent(), "Net Force", magnitude_text(body.net_force, 2, "N"));
}

// ── Scalar formatter ──────────────────────────────────────────────────────────
inline std::string format_scalar(double value, int precision, const char* unit) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f %s", precision, value, unit);
    return std::string(buf);
}

// ── Force inspector panel ─────────────────────────────────────────────────────
inline void draw_force_inspector(const CanvasOverlayView& view,
                                 const CanvasOverlayRects& hud) {
    if (!hud.show_inspector) return;

    const float s = hud.hud_scale;
    const bool  stacked = hud.inspector_stacked;
    const float body_block_height  = canvas_body_block_height(s);
    const float summary_row_height = canvas_summary_row_height(s);
    const float summary_gap        = 8.0f * s;
    const float body_gap           = 10.0f * s;
    const float header_height      = canvas_inspector_header(s);
    const Rectangle card = hud.inspector;

    draw_card(card, WL::GLASS_2, with_alpha(WL::CYAN_DIM, 130));
    draw_corner_brackets(card, with_alpha(WL::CYAN_CORE, 170), 13.0f * s, 1.6f, 5.0f * s);

    // Slow scan sweep — a single faint line travelling down the pane.
    const float sweep = 0.5f + 0.5f * std::sin(ui_time() * 0.7f);
    const float sweep_y = card.y + header_height + sweep * (card.height - header_height - 8.0f * s);
    DrawLineEx({card.x + 10.0f * s, sweep_y}, {card.x + card.width - 10.0f * s, sweep_y},
               1.0f, with_alpha(WL::CYAN_CORE, 26));

    draw_text("Force Inspector", {card.x + 14.0f * s, card.y + 12.0f * s}, 20.0f * s, WL::CYAN_CORE);
    draw_text_fit("Tags match the labels on the stage arrows.", {card.x + 14.0f * s, card.y + 36.0f * s},
                  card.width - 40.0f * s, 12.5f * s, 10.5f * s, WL::TEXT_TERTIARY);
    const float dot = 0.5f + 0.5f * std::sin(ui_time() * 3.0f);
    DrawCircleV({card.x + card.width - 18.0f * s, card.y + 20.0f * s}, 3.4f * s,
                with_alpha(WL::PLASMA_GREEN, static_cast<unsigned char>(140 + 110 * dot)));
    DrawLineEx({card.x + 10, card.y + header_height - 4},
               {card.x + card.width - 10, card.y + header_height - 4},
               1.0f, with_alpha(WL::CYAN_DIM, 60));

    const float block_gap   = 12.0f * s;
    const float block_width = stacked
        ? card.width - 28.0f * s
        : (card.width - 28.0f * s - block_gap) * 0.5f;
    const float block_y = card.y + header_height;
    const float col2_x  = card.x + 14.0f * s + block_width + block_gap;

    // One row layout for the whole inspector, decided from the longest labels
    // and a worst-case value width, so rows neither mix styles nor flip as the
    // numbers change length.
    const bool single = probe_row_fits_single(block_width - 16.0f * s, "Db", "Constraint", "000.00 m/s", s)
                     && probe_row_fits_single(block_width, "Dl2", "Elbow Torque", "000.00 Nm", s);
    const ProbeRowLayout mode = single ? ProbeRowLayout::SINGLE : ProbeRowLayout::STACKED;

    draw_force_block({card.x + 14.0f * s, block_y, block_width, body_block_height},
                     "Upper Bob", hud_velocity_accent(), view.diagnostics.bob1, s, mode);
    draw_force_block({stacked ? card.x + 14.0f * s : col2_x,
                      stacked ? block_y + body_block_height + body_gap : block_y,
                      block_width, body_block_height},
                     "Lower Bob", hud_drag_accent(), view.diagnostics.bob2, s, mode);

    float y = block_y + (stacked ? body_block_height * 2.0f + body_gap : body_block_height) + summary_gap;
    const float pitch = summary_row_height + 8.0f * s;
    auto pair = [&](const char* tag_a, const char* label_a, const std::string& value_a,
                    const char* tag_b, const char* label_b, const std::string& value_b, Color accent) {
        draw_probe_row({card.x + 14.0f * s, y, block_width, summary_row_height}, tag_a, accent, label_a, value_a, s, mode);
        draw_probe_row({stacked ? card.x + 14.0f * s : col2_x, stacked ? y + pitch : y,
                        block_width, summary_row_height}, tag_b, accent, label_b, value_b, s, mode);
        y += stacked ? pitch * 2.0f : pitch;
    };

    pair("Dl1", "Upper Link", format_scalar(view.diagnostics.connector1.drag_force.length(), 2, "N"),
         "Dl2", "Lower Link", format_scalar(view.diagnostics.connector2.drag_force.length(), 2, "N"),
         hud_link_drag_accent());
    if (view.rigid_mode) {
        pair("Tp", "Pivot Torque", format_scalar(view.diagnostics.pivot_torque, 2, "Nm"),
             "Te", "Elbow Torque", format_scalar(view.diagnostics.elbow_torque, 2, "Nm"),
             hud_joint_torque_accent());
    }

    draw_probe_row({card.x + 14.0f * s, y, card.width - 28.0f * s, summary_row_height},
                   "P", hud_net_force_accent(), "Power Exchange",
                   format_scalar(view.dissipation_power, 3, "W"), s);
}

// ── Canvas overlay (title bar + hint bar + inspector) ─────────────────────────
inline void draw_canvas_overlay(const CanvasOverlayView& view,
                                const PendulumLayout& layout) {
    const CanvasOverlayRects hud =
        make_canvas_overlay_layout(layout.viewport, view.show_vectors, view.rigid_mode);
    const float s = hud.hud_scale;

    // ── Title bar ─────────────────────────────────────────────────────────────
    const Rectangle tb = hud.title_bar;
    draw_card(tb, WL::GLASS_1, with_alpha(WL::CYAN_DIM, 120));
    draw_corner_brackets(tb, with_alpha(WL::CYAN_CORE, 160), 11.0f * s, 1.5f, 4.0f * s);

    const float pad = 14.0f * s;
    float badge_left = tb.x + tb.width - pad;
    if (view.show_vectors) {
        // Overlay state badges, right-aligned on the top row and sized to fit.
        const float bh = 20.0f * s;
        const float preset_w = measure_ui_text(view.preset_label, 11.5f * s).x + 20.0f * s;
        badge_left -= preset_w;
        draw_badge({badge_left, tb.y + 9.0f * s, preset_w, bh}, view.preset_label,
                   with_alpha(WL::CYAN_DIM, 180), WL::TEXT_PRIMARY, s * 0.85f);
        const float field_w = measure_ui_text("FIELD ON", 11.5f * s).x + 20.0f * s;
        badge_left -= field_w + 6.0f * s;
        draw_badge({badge_left, tb.y + 9.0f * s, field_w, bh}, "FIELD ON",
                   with_alpha(WL::VIOLET_DIM, 220), WL::VIOLET_CORE, s * 0.85f);
    } else {
        const float pulse = 0.5f + 0.5f * std::sin(ui_time() * 2.4f);
        DrawCircleV({tb.x + tb.width - 16.0f * s, tb.y + 16.0f * s}, 3.2f * s,
                    with_alpha(WL::CYAN_CORE, static_cast<unsigned char>(130 + 110 * pulse)));
    }

    draw_text_fit("WORLDLINE  /  REFERENCE SYSTEM", {tb.x + pad, tb.y + 11.0f * s},
                  badge_left - tb.x - pad * 2.0f, 12.5f * s, 9.5f * s, WL::CYAN_CORE);
    draw_text_fit("Newtonian Double Pendulum", {tb.x + pad, tb.y + 28.0f * s}, tb.width - pad * 2.0f,
                  21.0f * s, 15.0f * s, WL::TEXT_PRIMARY);
    draw_text_fit("Build the launch state, then watch it unfold.", {tb.x + pad, tb.y + 52.0f * s},
                  tb.width - pad * 2.0f, 12.5f * s, 10.5f * s, WL::TEXT_TERTIARY);

    if (view.show_vectors) {
        draw_force_inspector(view, hud);
    }

    // ── Hint bar: one centred line, shrunk/ellipsized to fit ──────────────────
    const Rectangle hb = hud.hint_bar;
    draw_card(hb, WL::GLASS_1, with_alpha(WL::GLASS_BORDER, 130));
    const float chevron = 16.0f * s;
    draw_text(">", {hb.x + 12.0f * s, hb.y + (hb.height - chevron) * 0.5f}, chevron, with_alpha(WL::CYAN_CORE, 150));
    draw_text_fit_in(view.hint, {hb.x + 32.0f * s, hb.y, hb.width - 44.0f * s, hb.height},
                     15.0f * s, 11.5f * s, WL::TEXT_SECONDARY);
}
