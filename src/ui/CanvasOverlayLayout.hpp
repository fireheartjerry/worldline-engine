#pragma once
#include "raylib.h"
#include <algorithm>

struct CanvasOverlayRects {
    Rectangle title_bar{};
    Rectangle sim_dock{};
    Rectangle hint_bar{};
    Rectangle inspector{};
    Rectangle legend{};
    Rectangle stage_rect{};
    float hud_scale = 1.0f;
    bool show_inspector = false;
    bool show_legend = false;
    bool inspector_stacked = false;
};

inline float canvas_overlay_scale(Rectangle viewport) {
    const float width_factor = viewport.width / 1320.0f;
    const float height_factor = viewport.height / 820.0f;
    return std::clamp(std::min(width_factor, height_factor), 0.94f, 1.18f);
}

inline float canvas_inspector_height(float scale,
                                     bool stacked,
                                     bool rigid_mode) {
    const float body_block_height = 270.0f * scale;
    const float summary_row_height = 34.0f * scale;
    const float summary_gap = 8.0f * scale;
    const float body_gap = (stacked ? 10.0f : 12.0f) * scale;
    const float header_height = 62.0f * scale;
    const float summary_rows = rigid_mode ? 2.0f : 1.0f;
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

    const float scale = rects.hud_scale;
    const float margin = 18.0f * scale;
    const float gap = 22.0f * scale;
    const float title_width = std::min(viewport.width - margin * 2.0f,
                                       std::clamp(viewport.width * 0.28f,
                                                  340.0f * scale,
                                                  460.0f * scale));
    const float title_height = 78.0f * scale;
    rects.title_bar = {
        viewport.x + margin,
        viewport.y + margin,
        title_width,
        title_height
    };

    const float dock_width = std::clamp(viewport.width * 0.20f,
                                        236.0f * scale,
                                        320.0f * scale);
    const float dock_height = 202.0f * scale;
    rects.sim_dock = {
        viewport.x + viewport.width - dock_width - margin,
        viewport.y + margin,
        dock_width,
        dock_height
    };

    const float hint_height = 68.0f * scale;
    rects.hint_bar = {
        viewport.x + margin,
        viewport.y + viewport.height - hint_height - margin,
        viewport.width - margin * 2.0f,
        hint_height
    };

    if (show_vectors) {
        rects.show_inspector = true;
        rects.show_legend = true;
        rects.inspector_stacked = viewport.width < 1320.0f;

        const float inspector_width = rects.inspector_stacked
            ? std::min(viewport.width - margin * 2.0f,
                       std::clamp(viewport.width * 0.34f,
                                  340.0f * scale,
                                  420.0f * scale))
            : std::min(viewport.width - margin * 2.0f,
                       std::clamp(viewport.width * 0.33f,
                                  430.0f * scale,
                                  560.0f * scale));
        const float inspector_height =
            canvas_inspector_height(scale, rects.inspector_stacked, rigid_mode);
        rects.inspector = {
            viewport.x + margin,
            std::max(rects.title_bar.y + rects.title_bar.height + gap,
                     rects.hint_bar.y - inspector_height - gap),
            inspector_width,
            inspector_height
        };

        const float legend_width = 210.0f * scale;
        const float legend_height = 196.0f * scale;
        rects.legend = {
            viewport.x + viewport.width - legend_width - margin,
            rects.sim_dock.y + rects.sim_dock.height + 14.0f * scale,
            legend_width,
            legend_height
        };
    }

    float left = viewport.x + margin;
    float right = viewport.x + viewport.width - margin;
    float top = viewport.y + margin;
    float bottom = viewport.y + viewport.height - margin;

    top = std::max(top, rects.title_bar.y + rects.title_bar.height + gap);
    top = std::max(top, rects.sim_dock.y + rects.sim_dock.height + 18.0f * scale);
    bottom = std::min(bottom, rects.hint_bar.y - 16.0f * scale);

    if (show_vectors) {
        left = std::max(left, rects.inspector.x + rects.inspector.width + gap);
        right = std::min(right, rects.legend.x - 18.0f * scale);
    }

    if (right - left < viewport.width * 0.30f) {
        left = viewport.x + margin + (show_vectors ? viewport.width * 0.08f : 0.0f);
        right = viewport.x + viewport.width - margin - (show_vectors ? viewport.width * 0.05f : 0.0f);
    }
    if (bottom - top < 220.0f * scale) {
        top = viewport.y + margin + title_height + 14.0f * scale;
        bottom = rects.hint_bar.y - 12.0f * scale;
    }

    rects.stage_rect = {
        left,
        top,
        std::max(180.0f, right - left),
        std::max(180.0f, bottom - top)
    };
    return rects;
}
