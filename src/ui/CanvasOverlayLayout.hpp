#pragma once
#include "raylib.h"
#include <algorithm>

// Layout geometry is kept identical to the original so all positioning math
// continues to work.  Only the scale constants are tightened to leave more
// room for the sharper, denser alien-tech panels.

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

inline float canvas_inspector_height(float scale, bool stacked, bool rigid_mode) {
    const float body_block_height  = 270.0f * scale;
    const float summary_row_height =  34.0f * scale;
    const float summary_gap        =   8.0f * scale;
    const float body_gap           = (stacked ? 10.0f : 12.0f) * scale;
    const float header_height      =  62.0f * scale;
    const float summary_rows       = rigid_mode ? 2.0f : 1.0f;
    return header_height
         + (stacked ? body_block_height * 2.0f + body_gap : body_block_height)
         + summary_gap
         + summary_rows * (summary_row_height + 8.0f * scale)
         + summary_row_height
         + 20.0f * scale;
}

inline CanvasOverlayRects make_canvas_overlay_layout(Rectangle viewport,
                                                     bool show_vectors,
                                                     bool rigid_mode) {
    CanvasOverlayRects rects;
    rects.hud_scale = canvas_overlay_scale(viewport);

    const float s      = rects.hud_scale;
    const float margin = 16.0f * s;
    const float gap    = 20.0f * s;

    // ── Title bar ────────────────────────────────────────────────────────────
    const float title_width = std::min(viewport.width - margin * 2.0f,
                                       std::clamp(viewport.width * 0.28f,
                                                  336.0f * s,
                                                  452.0f * s));
    const float title_height = 76.0f * s;
    rects.title_bar = {
        viewport.x + margin,
        viewport.y + margin,
        title_width,
        title_height
    };

    // ── Simulation dock ───────────────────────────────────────────────────────
    const float dock_width  = std::clamp(viewport.width * 0.20f, 232.0f * s, 316.0f * s);
    const float dock_height = 200.0f * s;
    rects.sim_dock = {
        viewport.x + viewport.width - dock_width - margin,
        viewport.y + margin,
        dock_width,
        dock_height
    };

    // ── Hint bar ──────────────────────────────────────────────────────────────
    const float hint_height = 64.0f * s;
    rects.hint_bar = {
        viewport.x + margin,
        viewport.y + viewport.height - hint_height - margin,
        viewport.width - margin * 2.0f,
        hint_height
    };

    // ── Force inspector (visible only when vectors are on) ───────────────────
    if (show_vectors) {
        rects.show_inspector   = true;
        rects.show_legend      = true;
        rects.inspector_stacked = viewport.width < 1320.0f;

        const float insp_width = rects.inspector_stacked
            ? std::min(viewport.width - margin * 2.0f,
                       std::clamp(viewport.width * 0.34f, 336.0f * s, 416.0f * s))
            : std::min(viewport.width - margin * 2.0f,
                       std::clamp(viewport.width * 0.33f, 426.0f * s, 556.0f * s));
        const float insp_height = canvas_inspector_height(s, rects.inspector_stacked, rigid_mode);
        rects.inspector = {
            viewport.x + margin,
            std::max(rects.title_bar.y + rects.title_bar.height + gap,
                     rects.hint_bar.y  - insp_height - gap),
            insp_width,
            insp_height
        };

        const float legend_width  = 208.0f * s;
        const float legend_height = 194.0f * s;
        rects.legend = {
            viewport.x + viewport.width - legend_width - margin,
            rects.sim_dock.y + rects.sim_dock.height + 14.0f * s,
            legend_width,
            legend_height
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
        top    = viewport.y + margin + title_height + 14.0f * s;
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