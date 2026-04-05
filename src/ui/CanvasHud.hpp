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
// Each row has a glowing diamond tag chip on the left, label in dim text,
// and right-aligned value in bright text.
inline void draw_probe_row(Rectangle rect,
                           const char* tag,
                           Color accent,
                           const char* label,
                           const std::string& value,
                           float scale) {
    // Row background — slightly lighter than card, with accent left stripe
    DrawRectangleRounded(rect, 0.06f, 6, {10, 20, 32, 218});
    DrawRectangleRoundedLines(rect, 0.06f, 6, 1.0f, with_alpha(accent, 35));
    // Left stripe
    DrawRectangle(static_cast<int>(rect.x + 1),
                  static_cast<int>(rect.y + 3),
                  2,
                  static_cast<int>(rect.height - 6),
                  with_alpha(accent, 160));

    // Tag chip
    const Rectangle chip = {rect.x + 8.0f * scale, rect.y + 7.0f * scale,
                             38.0f * scale, 20.0f * scale};
    DrawRectangleRounded(chip, 0.40f, 6, with_alpha(accent, 185));
    // Chip glow halo
    DrawRectangleRoundedLines({chip.x - 1, chip.y - 1, chip.width + 2, chip.height + 2},
                              0.45f, 6, 1.0f, with_alpha(accent, 55));

    const float tag_px = 11.5f * scale;
    const Vector2 tag_sz = measure_ui_text(tag, tag_px);
    draw_text(tag,
              {chip.x + (chip.width - tag_sz.x) * 0.5f,
               chip.y + (chip.height - tag_sz.y) * 0.5f},
              tag_px,
              {6, 14, 22, 255});

    // Label
    draw_text(label,
              {rect.x + 54.0f * scale, rect.y + 6.0f * scale},
              12.5f * scale,
              WL::TEXT_TERTIARY);

    // Value — right-aligned, bright
    const float val_px = fit_ui_text_size(value, rect.width - 70.0f * scale, 15.0f * scale, 11.5f * scale);
    const Vector2 val_sz = measure_ui_text(value, val_px);
    draw_text(value,
              {rect.x + rect.width - val_sz.x - 10.0f * scale,
               rect.y + rect.height * 0.5f - val_sz.y * 0.5f},
              val_px,
              WL::TEXT_PRIMARY);
}

// ── Force block ───────────────────────────────────────────────────────────────
inline void draw_force_block(Rectangle rect,
                             const char* title,
                             Color accent,
                             const Simulation::BodyDiagnostics& body,
                             float scale) {
    // Card with coloured left stripe
    draw_card_accented(rect, WL::GLASS_1, with_alpha(accent, 70), accent);

    // Title row
    draw_text(title,
              {rect.x + 14.0f * scale, rect.y + 10.0f * scale},
              17.0f * scale,
              WL::TEXT_PRIMARY);

    // Constrained / Free badge — top right
    const bool constrained = body.constrained;
    const Color badge_fill = constrained ? with_alpha(WL::XENON_DIM, 180) : with_alpha(WL::PLASMA_DIM, 160);
    const Color badge_text = constrained ? WL::XENON_CORE : WL::PLASMA_GREEN;
    draw_badge({rect.x + rect.width - 88.0f * scale, rect.y + 9.0f * scale,
                78.0f * scale, 18.0f * scale},
               constrained ? "BOUND" : "FREE",
               badge_fill, badge_text, scale);

    auto magnitude_text = [](Vec2 v, int prec, const char* unit) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f %s", prec, v.length(), unit);
        return std::string(buf);
    };

    const float rh  = 38.0f * scale;
    const float rgap = 3.5f * scale;
    float y = rect.y + 36.0f * scale;

    auto row = [&](const char* tag, Color ac, const char* lbl, const std::string& val) {
        draw_probe_row({rect.x + 8.0f * scale, y, rect.width - 16.0f * scale, rh},
                       tag, ac, lbl, val, scale);
        y += rh + rgap;
    };

    row("v",  hud_velocity_accent(), "Speed",
        magnitude_text(body.velocity, 2, "m/s"));
    row("Db", hud_drag_accent(), "Bob Drag",
        magnitude_text(body.drag_force, 2, "N"));
    row("R",  hud_reaction_accent(), "Constraint",
        magnitude_text(body.reaction_force, 2, "N"));
    row("F",  hud_net_force_accent(), "Net Force",
        magnitude_text(body.net_force, 2, "N"));
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
    const float body_block_height  = 270.0f * s;
    const float summary_row_height =  34.0f * s;
    const float summary_gap        =   8.0f * s;
    const float body_gap           = (stacked ? 10.0f : 12.0f) * s;
    const float header_height      =  62.0f * s;
    const Rectangle card = hud.inspector;

    // Main inspector card — cyan-bordered
    draw_card(card, WL::GLASS_2, with_alpha(WL::CYAN_DIM, 110));

    // Header
    draw_text("Force Inspector",
              {card.x + 14.0f * s, card.y + 12.0f * s},
              21.0f * s,
              WL::CYAN_CORE);
    draw_text("Tag labels match stage markers.",
              {card.x + 14.0f * s, card.y + 36.0f * s},
              13.0f * s,
              WL::TEXT_TERTIARY);
    // Thin header separator
    DrawLineEx({card.x + 10, card.y + header_height - 4},
               {card.x + card.width - 10, card.y + header_height - 4},
               1.0f, with_alpha(WL::CYAN_DIM, 60));

    const float block_gap   = 12.0f * s;
    const float block_width = stacked
        ? card.width - 28.0f * s
        : (card.width - 28.0f * s - block_gap) * 0.5f;
    const float block_y = card.y + header_height;

    draw_force_block({card.x + 14.0f * s, block_y, block_width, body_block_height},
                     "Upper Bob", hud_velocity_accent(),
                     view.diagnostics.bob1,
                     s);

    draw_force_block({stacked ? card.x + 14.0f * s
                              : card.x + 14.0f * s + block_width + block_gap,
                      stacked ? block_y + body_block_height + body_gap : block_y,
                      block_width, body_block_height},
                     "Lower Bob", hud_drag_accent(),
                     view.diagnostics.bob2,
                     s);

    float y = block_y + (stacked ? body_block_height * 2.0f + body_gap : body_block_height) + summary_gap;
    const float sum_width = stacked ? card.width - 28.0f * s : block_width;

    draw_probe_row({card.x + 14.0f * s, y, sum_width, summary_row_height},
                   "Dl1", hud_link_drag_accent(), "Upper Link",
                   format_scalar(view.diagnostics.connector1.drag_force.length(), 2, "N"), s);
    draw_probe_row({stacked ? card.x + 14.0f * s
                            : card.x + 14.0f * s + block_width + block_gap,
                    stacked ? y + summary_row_height + 8.0f * s : y,
                    sum_width, summary_row_height},
                   "Dl2", hud_link_drag_accent(), "Lower Link",
                   format_scalar(view.diagnostics.connector2.drag_force.length(), 2, "N"), s);
    y += stacked ? (summary_row_height + 8.0f * s) * 2.0f : summary_row_height + 8.0f * s;

    if (view.rigid_mode) {
        draw_probe_row({card.x + 14.0f * s, y, sum_width, summary_row_height},
                       "Tp", hud_joint_torque_accent(), "Pivot Torque",
                       format_scalar(view.diagnostics.pivot_torque, 2, "Nm"), s);
        draw_probe_row({stacked ? card.x + 14.0f * s
                                : card.x + 14.0f * s + block_width + block_gap,
                        stacked ? y + summary_row_height + 8.0f * s : y,
                        sum_width, summary_row_height},
                       "Te", hud_joint_torque_accent(), "Elbow Torque",
                       format_scalar(view.diagnostics.elbow_torque, 2, "Nm"), s);
        y += stacked ? (summary_row_height + 8.0f * s) * 2.0f : summary_row_height + 8.0f * s;
    }

    // Power exchange — full-width, hot-pink accent
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
    draw_card(tb, WL::GLASS_1, with_alpha(WL::CYAN_DIM, 100));

    // Brand wordmark — two-tone
    draw_text("WORLDLINE",
              {tb.x + 14.0f * s, tb.y + 10.0f * s},
              14.0f * s,
              WL::CYAN_CORE);
    draw_text("Universe Simulator",
              {tb.x + 14.0f * s, tb.y + 28.0f * s},
              22.0f * s,
              WL::TEXT_PRIMARY);
    draw_text("Build the launch state, then watch it unfold.",
              {tb.x + 14.0f * s, tb.y + 53.0f * s},
              13.0f * s,
              WL::TEXT_TERTIARY);

    if (view.show_vectors) {
        draw_badge({tb.x + tb.width - 130.0f * s, tb.y + 12.0f * s, 90.0f * s, 22.0f * s},
                   "FIELD ON",
                   with_alpha(WL::VIOLET_DIM, 220),
                   WL::VIOLET_CORE,
                   s);
        draw_badge({tb.x + tb.width - 36.0f * s, tb.y + 12.0f * s, 26.0f * s, 22.0f * s},
                   view.preset_label,
                   with_alpha(WL::CYAN_DIM, 180),
                   WL::TEXT_PRIMARY,
                   s);
        draw_force_inspector(view, hud);
    }

    // ── Hint bar ──────────────────────────────────────────────────────────────
    const Rectangle hb = hud.hint_bar;
    draw_card(hb, WL::GLASS_1, with_alpha(WL::GLASS_BORDER, 130));

    // Small prompt chevron
    draw_text(">",
              {hb.x + 10.0f * s, hb.y + hb.height * 0.5f - 9.0f * s},
              17.0f * s,
              with_alpha(WL::CYAN_CORE, 140));
    draw_text_block(view.hint,
                    {hb.x + 26.0f * s, hb.y + 7.0f * s, hb.width - 36.0f * s, hb.height - 10.0f * s},
                    16.0f * s,
                    WL::TEXT_SECONDARY,
                    3.0f * s);
}
