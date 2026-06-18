#pragma once
// Seeded Universe — shared drawing widgets (headers, tooltips, snapshot strip).
#include "SeededCommon.hpp"

namespace SeededUniverseUi {

// ── Shared Drawing Primitives ────────────────────────────────────────────────

inline void draw_separator_h(float x, float y, float w, Color color) {
    DrawLineEx({x, y}, {x + w, y}, 1.0f, color);
}

inline void draw_separator_v(float x, float y, float h, Color color) {
    DrawLineEx({x, y}, {x, y + h}, 1.0f, color);
}

inline void draw_corner_brackets(Rectangle rect, float arm, float thickness, Color color) {
    // Top-left
    DrawLineEx({rect.x, rect.y}, {rect.x + arm, rect.y}, thickness, color);
    DrawLineEx({rect.x, rect.y}, {rect.x, rect.y + arm}, thickness, color);
    // Top-right
    DrawLineEx({rect.x + rect.width, rect.y}, {rect.x + rect.width - arm, rect.y}, thickness, color);
    DrawLineEx({rect.x + rect.width, rect.y}, {rect.x + rect.width, rect.y + arm}, thickness, color);
    // Bottom-left
    DrawLineEx({rect.x, rect.y + rect.height}, {rect.x + arm, rect.y + rect.height}, thickness, color);
    DrawLineEx({rect.x, rect.y + rect.height}, {rect.x, rect.y + rect.height - arm}, thickness, color);
    // Bottom-right
    DrawLineEx({rect.x + rect.width, rect.y + rect.height}, {rect.x + rect.width - arm, rect.y + rect.height}, thickness, color);
    DrawLineEx({rect.x + rect.width, rect.y + rect.height}, {rect.x + rect.width, rect.y + rect.height - arm}, thickness, color);
}

inline bool draw_info_button(Rectangle rect, float scale, Color accent) {
    const bool hot = CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRectangleRounded(rect, 0.35f, 8, hot ? Color{16, 34, 58, 244} : Color{8, 18, 32, 230});
    DrawRectangleRoundedLines(rect, 0.35f, 8, 1.0f, with_alpha(accent, hot ? 180 : 100));
    if (hot) {
        DrawRectangleRounded({rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4},
                             0.35f, 8, with_alpha(accent, 20));
    }
    draw_text("i",
              {rect.x + rect.width * 0.5f - 3.0f * scale, rect.y + 4.0f * scale},
              14.0f * scale,
              hot ? WL::TEXT_PRIMARY : with_alpha(accent, 210));
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

inline PanelHeaderResult draw_panel_header(Rectangle rect,
                                           const char* eyebrow,
                                           const char* title,
                                           const char* subtitle,
                                           float scale,
                                           Color accent) {
    PanelHeaderResult result;
    draw_text(eyebrow, {rect.x + 14.0f * scale, rect.y + 10.0f * scale}, 12.0f * scale, with_alpha(accent, 180));
    draw_text(title, {rect.x + 14.0f * scale, rect.y + 24.0f * scale}, 24.0f * scale, WL::TEXT_PRIMARY);
    draw_text_block(subtitle,
                    {rect.x + 14.0f * scale, rect.y + 52.0f * scale, rect.width - 60.0f * scale, 38.0f * scale},
                    13.0f * scale,
                    WL::TEXT_SECONDARY,
                    3.0f * scale);
    result.info_clicked = draw_info_button(
        {rect.x + rect.width - 30.0f * scale, rect.y + 10.0f * scale, 20.0f * scale, 20.0f * scale},
        scale,
        accent);
    return result;
}

inline void draw_scrollbar(Rectangle viewport, float scroll, float max_scroll) {
    if (max_scroll <= 0.0f) return;
    const float track_w = 5.0f;
    const float track_x = viewport.x + viewport.width - track_w - 3.0f;
    const float track_y = viewport.y + 3.0f;
    const float track_h = viewport.height - 6.0f;
    const float thumb_h = std::max(28.0f, track_h * (viewport.height / (viewport.height + max_scroll)));
    const float thumb_y = track_y + (track_h - thumb_h) * (scroll / max_scroll);
    DrawRectangleRounded({track_x, track_y, track_w, track_h}, 0.5f, 8, {255, 255, 255, 8});
    DrawRectangleRounded({track_x, thumb_y, track_w, thumb_h}, 0.5f, 8, {64, 208, 224, 100});
}

// ── Hover Tooltip ────────────────────────────────────────────────────────────

inline void draw_tooltip(const std::string& text, Vector2 pos, float scale) {
    const float font_size = 12.0f * scale;
    const Vector2 text_size = measure_ui_text(text, font_size);
    const float pad_x = 8.0f * scale;
    const float pad_y = 5.0f * scale;
    const Rectangle bg = {
        pos.x - pad_x,
        pos.y - text_size.y - pad_y * 2.0f - 4.0f * scale,
        text_size.x + pad_x * 2.0f,
        text_size.y + pad_y * 2.0f
    };
    DrawRectangleRounded(bg, 0.12f, 8, {4, 12, 22, 245});
    DrawRectangleRoundedLines(bg, 0.12f, 8, 1.0f, with_alpha(WL::CYAN_CORE, 130));
    draw_text(text, {bg.x + pad_x, bg.y + pad_y}, font_size, WL::TEXT_PRIMARY);
}

// ── Snapshot Strip ───────────────────────────────────────────────────────────

inline void draw_snapshot_strip(Rectangle rect,
                                const CellularCheckpoint& checkpoint,
                                float intensity,
                                float scale) {
    DrawRectangleRounded(rect, 0.06f, 6, {6, 14, 24, 204});
    DrawRectangleRoundedLines(rect, 0.06f, 6, 1.0f, with_alpha(WL::CYAN_DIM, 50));
    const std::size_t bucket_count = std::max<std::size_t>(16u, static_cast<std::size_t>(rect.width / (3.0f * scale)));
    const std::vector<float> buckets = bucketize_cells(checkpoint.cells, bucket_count);
    const float slot_w = rect.width / static_cast<float>(buckets.size());
    for (std::size_t index = 0; index < buckets.size(); ++index) {
        const float value = buckets[index];
        const float height = std::max(2.0f * scale, (rect.height - 6.0f * scale) * value);
        const Rectangle bar = {
            rect.x + index * slot_w + 0.5f,
            rect.y + rect.height - 3.0f * scale - height,
            std::max(1.0f, slot_w - 1.0f),
            height
        };
        const Color fill = {
            static_cast<unsigned char>(28 + 60 * (1.0f - value) + 40 * intensity),
            static_cast<unsigned char>(70 + 160 * value),
            static_cast<unsigned char>(88 + 120 * value),
            static_cast<unsigned char>(60 + 170 * intensity)
        };
        DrawRectangleRec(bar, fill);
    }
}


} // namespace SeededUniverseUi
