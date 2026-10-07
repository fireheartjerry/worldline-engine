#pragma once
#include "raylib.h"
#include <algorithm>

// Reference Lab HUD geometry.  Everything that draws on the Reference Lab
// canvas (title bar, simulation dock, hint bar, force inspector, vector
// legend) and the renderer's stage framing derive their rectangles from
// make_canvas_overlay_layout(), so the panels and the pendulum never collide.
//
// The "< Back" key is drawn by the app shell at (20, 20) * scale inside the
// canvas (draw_back_to_menu_button); the title bar starts below it.

struct CanvasOverlayRects {
    Rectangle title_bar{};
    Rectangle sim_dock{};
    Rectangle hint_bar{};
    Rectangle inspector{};
    Rectangle legend{};
    Rectangle stage_rect{};
    float hud_scale    = 1.0f;
    bool show_inspector = false;
    bool show_legend    = false;
    bool inspector_stacked = false;
};

inline float canvas_overlay_scale(Rectangle viewport) {
    const float wf = viewport.width  / 1320.0f;
    const float hf = viewport.height / 820.0f;
    return std::clamp(std::min(wf, hf), 0.92f, 1.16f);
}

// Force-inspector block metrics, shared by the layout and the HUD renderer.
inline float canvas_probe_row_height(float scale)  { return 36.0f * scale; }
inline float canvas_probe_row_gap(float scale)     { return 4.0f * scale; }
inline float canvas_inspector_header(float scale)  { return 60.0f * scale; }
inline float canvas_summary_row_height(float scale) { return 34.0f * scale; }

// One bob's block: title row + four probe rows (speed, drag, constraint, net).
inline float canvas_body_block_height(float scale) {
    return 38.0f * scale + 4.0f * (canvas_probe_row_height(scale) + canvas_probe_row_gap(scale)) + 6.0f * scale;
}

inline float canvas_inspector_height(float scale, bool stacked, bool rigid_mode) {
    const float body_block_height  = canvas_body_block_height(scale);
    const float summary_row_height = canvas_summary_row_height(scale);
    const float summary_gap        = 8.0f * scale;
    const float body_gap           = 10.0f * scale;
    const float row_pitch          = summary_row_height + 8.0f * scale;
    // Link drag pair (+ joint torque pair in rigid mode); stacked layouts put
    // each pair on two rows.  Power exchange is always one full-width row.
    const float pair_rows = (rigid_mode ? 2.0f : 1.0f) * (stacked ? 2.0f : 1.0f);
    return canvas_inspector_header(scale)
         + (stacked ? body_block_height * 2.0f + body_gap : body_block_height)
         + summary_gap
         + pair_rows * row_pitch
         + summary_row_height
         + 16.0f * scale;
}

inline CanvasOverlayRects make_canvas_overlay_layout(Rectangle viewport,
                                                     bool show_vectors,
                                                     bool rigid_mode) {
    CanvasOverlayRects rects;
    rects.hud_scale = canvas_overlay_scale(viewport);

    const float s      = rects.hud_scale;
    const float margin = 16.0f * s;
    const float gap    = 18.0f * s;

    // ── Title bar (below the shell's back key) ───────────────────────────────
    const float back_bottom  = 20.0f * s + 36.0f * s;
    const float title_width  = std::min(viewport.width - margin * 2.0f,
                                        std::clamp(viewport.width * 0.28f, 336.0f * s, 452.0f * s));
    const float title_height = 72.0f * s;
    rects.title_bar = {
        viewport.x + margin,
        viewport.y + back_bottom + 10.0f * s,
        title_width,
        title_height
    };

    // ── Simulation dock ───────────────────────────────────────────────────────
    const float dock_width  = std::clamp(viewport.width * 0.20f, 252.0f * s, 316.0f * s);
    const float dock_height = 214.0f * s;
    rects.sim_dock = {
        viewport.x + viewport.width - dock_width - margin,
        viewport.y + margin,
        dock_width,
        dock_height
    };

    // ── Hint bar: one line of guidance along the bottom ──────────────────────
    const float hint_height = 42.0f * s;
    rects.hint_bar = {
        viewport.x + margin,
        viewport.y + viewport.height - hint_height - margin,
        viewport.width - margin * 2.0f,
        hint_height
    };

    // ── Force inspector + legend (visible only when vectors are on) ──────────
    if (show_vectors) {
        rects.show_inspector = true;
        rects.show_legend    = true;

        const float top_limit    = rects.title_bar.y + rects.title_bar.height + gap;
        const float bottom_limit = rects.hint_bar.y - gap;
        const float legend_width = std::clamp(viewport.width * 0.17f, 214.0f * s, 260.0f * s);

        // Side-by-side blocks are preferred; stack them only when the window is
        // too narrow for that *and* tall enough for the stacked column.
        const float wide_width = std::min(viewport.width - margin * 2.0f,
                                          std::clamp(viewport.width * 0.33f, 400.0f * s, 556.0f * s));
        const float stage_if_wide = viewport.width - margin * 2.0f - wide_width - legend_width - gap * 2.0f;
        const bool  stacked_fits  = canvas_inspector_height(s, true, rigid_mode) <= bottom_limit - top_limit;
        rects.inspector_stacked = stage_if_wide < viewport.width * 0.30f && stacked_fits;

        const float insp_width = rects.inspector_stacked
            ? std::min(viewport.width - margin * 2.0f,
                       std::clamp(viewport.width * 0.30f, 300.0f * s, 400.0f * s))
            : wide_width;
        const float insp_height = canvas_inspector_height(s, rects.inspector_stacked, rigid_mode);
        rects.inspector = {
            viewport.x + margin,
            std::max(top_limit, bottom_limit - insp_height),
            insp_width,
            insp_height
        };

        rects.legend = {
            viewport.x + viewport.width - legend_width - margin,
            rects.sim_dock.y + rects.sim_dock.height + 14.0f * s,
            legend_width,
            194.0f * s
        };
    }

    // ── Stage rect (area available for the actual pendulum rendering) ─────────
    float left   = viewport.x + margin;
    float right  = viewport.x + viewport.width  - margin;
    float top    = viewport.y + margin;
    float bottom = viewport.y + viewport.height - margin;

    top    = std::max(top,    rects.title_bar.y + rects.title_bar.height + gap);
    top    = std::max(top,    rects.sim_dock.y  + rects.sim_dock.height  + 18.0f * s);
    bottom = std::min(bottom, rects.hint_bar.y  - 16.0f * s);

    if (show_vectors) {
        left  = std::max(left,  rects.inspector.x + rects.inspector.width + gap);
        right = std::min(right, rects.legend.x    - 18.0f * s);
    }

    if (right - left < viewport.width * 0.30f) {
        left  = viewport.x + margin + (show_vectors ? viewport.width * 0.08f : 0.0f);
        right = viewport.x + viewport.width - margin - (show_vectors ? viewport.width * 0.05f : 0.0f);
    }
    if (bottom - top < 220.0f * s) {
        // Degenerate (very short) windows: let the stage reach up beside the
        // dock rather than collapse.
        top    = rects.title_bar.y + title_height + 14.0f * s;
        bottom = rects.hint_bar.y - 12.0f * s;
    }

    rects.stage_rect = {
        left,
        top,
        std::max(180.0f, right - left),
        std::max(180.0f, bottom - top)
    };
    return rects;
}
