#include "ui/TraceScene.hpp"

#include "app/SeededUniverseRuntime.hpp"
#include "app/WorldlineCopy.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cstdio>

namespace {

std::string metric_text(double value, int precision = 2) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

std::string first_paragraph(const std::string& text) {
    const std::size_t split = text.find("\n\n");
    if (split == std::string::npos) {
        return text;
    }
    return text.substr(0, split);
}

void draw_preview_path(Rectangle rect, const std::vector<Vec2>& points, Color accent) {
    draw_card(rect, {8, 18, 30, 236}, with_alpha(accent, 90));
    if (points.size() < 2) {
        return;
    }

    double min_x = points.front().x;
    double max_x = points.front().x;
    double min_y = points.front().y;
    double max_y = points.front().y;
    for (const Vec2& point : points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }

    const double span_x = std::max(1.0e-6, max_x - min_x);
    const double span_y = std::max(1.0e-6, max_y - min_y);
    const float seg_count = static_cast<float>(std::max(std::size_t{1}, points.size() - 2));
    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const Vec2& a = points[index];
        const Vec2& b = points[index + 1];
        const Vector2 p0 = {
            rect.x + 12.0f + static_cast<float>((a.x - min_x) / span_x) * (rect.width - 24.0f),
            rect.y + 12.0f + static_cast<float>((a.y - min_y) / span_y) * (rect.height - 24.0f)
        };
        const Vector2 p1 = {
            rect.x + 12.0f + static_cast<float>((b.x - min_x) / span_x) * (rect.width - 24.0f),
            rect.y + 12.0f + static_cast<float>((b.y - min_y) / span_y) * (rect.height - 24.0f)
        };
        const float t = static_cast<float>(index) / seg_count;
        const auto seg_alpha = static_cast<unsigned char>(110.0f + 100.0f * t);
        DrawLineEx(p0, p1, 1.5f + 0.6f * t, with_alpha(accent, seg_alpha));
    }
}

void draw_matrix(Rectangle rect, const double matrix[2][2], Color accent, float scale) {
    draw_card(rect, {8, 18, 30, 236}, with_alpha(accent, 90));
    const float cell_w = (rect.width - 18.0f * scale) * 0.5f;
    const float cell_h = (rect.height - 22.0f * scale) * 0.5f;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 2; ++col) {
            const Rectangle cell = {
                rect.x + 8.0f * scale + col * (cell_w + 2.0f * scale),
                rect.y + 8.0f * scale + row * (cell_h + 2.0f * scale),
                cell_w,
                cell_h
            };
            const bool positive = matrix[row][col] >= 0.0;
            DrawRectangleRounded(cell, 0.08f, 6, positive ? Color{12, 44, 58, 228} : Color{44, 20, 56, 228});
            DrawRectangleRoundedLines(cell, 0.08f, 6, 1.0f, with_alpha(accent, 70));
            draw_text(metric_text(matrix[row][col], 2),
                      {cell.x + 10.0f * scale, cell.y + cell.height * 0.5f - 8.0f * scale},
                      16.0f * scale,
                      WL::TEXT_PRIMARY);
        }
    }
}

} // namespace

TraceSceneResult draw_trace_scene(AppState& app,
                                  Rectangle viewport) {
    TraceSceneResult result;
    SeededUniverseUiState& seeded = app.ui.seeded;
    if (!seeded.result.ready && seeded.result.error.empty()) {
        regenerate_seeded_universe(seeded);
    }

    const float scale = std::clamp(std::min(viewport.width / 1540.0f, viewport.height / 940.0f), 0.84f, 1.14f);
    const float margin = 18.0f * scale;
    if (draw_back_to_menu_button(viewport, scale) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
    }

    const Rectangle header = {viewport.x + margin, viewport.y + margin, viewport.width - margin * 2.0f, 92.0f * scale};
    const Rectangle left = {viewport.x + margin, header.y + header.height + 16.0f * scale, viewport.width * 0.40f, viewport.height - header.height - margin * 3.0f};
    const Rectangle right = {left.x + left.width + 16.0f * scale, left.y, viewport.width - left.width - margin * 2.0f - 16.0f * scale, left.height};

    draw_card(header, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 110));
    draw_text("Trace",
              {header.x + 16.0f * scale, header.y + 16.0f * scale},
              30.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text("Generator output, law preview, and glossary terms without crowding the main workspace.",
              {header.x + 16.0f * scale, header.y + 50.0f * scale},
              14.0f * scale,
              WL::TEXT_SECONDARY);
    if (draw_button({header.x + header.width - 164.0f * scale, header.y + 22.0f * scale, 148.0f * scale, 36.0f * scale},
                    "Back To Workspace",
                    {10, 84, 98, 240},
                    {18, 126, 140, 255},
                    WL::CYAN_CORE,
                    seeded.result.ready,
                    scale)) {
        result.open_workspace = true;
    }

    draw_card(left, {6, 14, 24, 230}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Generator Summary",
              {left.x + 16.0f * scale, left.y + 16.0f * scale},
              20.0f * scale,
              WL::TEXT_PRIMARY);
    if (!seeded.result.ready) {
        draw_text("No generated universe loaded.",
                  {left.x + 16.0f * scale, left.y + 44.0f * scale},
                  14.0f * scale,
                  WL::TEXT_SECONDARY);
        return result;
    }

    draw_text_block(first_paragraph(seeded.result.descriptor),
                    {left.x + 16.0f * scale, left.y + 44.0f * scale, left.width - 32.0f * scale, 110.0f * scale},
                    13.0f * scale,
                    WL::TEXT_SECONDARY,
                    3.0f * scale);

    draw_metric({left.x + 16.0f * scale, left.y + 166.0f * scale, left.width * 0.5f - 22.0f * scale, 56.0f * scale},
                "Gain",
                metric_text(seeded.result.law_preview.linear_gain, 2),
                scale);
    draw_metric({left.x + left.width * 0.5f + 6.0f * scale, left.y + 166.0f * scale, left.width * 0.5f - 22.0f * scale, 56.0f * scale},
                "Accel Ceiling",
                metric_text(seeded.result.law_preview.accel_ceiling, 2),
                scale);
    draw_metric({left.x + 16.0f * scale, left.y + 232.0f * scale, left.width * 0.5f - 22.0f * scale, 56.0f * scale},
                "Radius Mean",
                metric_text(seeded.result.law_preview.radius_mean, 2),
                scale);
    draw_metric({left.x + left.width * 0.5f + 6.0f * scale, left.y + 232.0f * scale, left.width * 0.5f - 22.0f * scale, 56.0f * scale},
                "Handedness",
                metric_text(seeded.result.law_preview.handedness, 2),
                scale);

    draw_text("Glossary",
              {left.x + 16.0f * scale, left.y + 312.0f * scale},
              16.0f * scale,
              with_alpha(WL::CYAN_CORE, 180));
    float glossary_y = left.y + 336.0f * scale;
    for (const GlossaryEntry& entry : Copy::glossary_entries()) {
        draw_text(entry.term,
                  {left.x + 16.0f * scale, glossary_y},
                  13.0f * scale,
                  WL::TEXT_PRIMARY);
        draw_text_block(entry.short_definition,
                        {left.x + 16.0f * scale, glossary_y + 16.0f * scale, left.width - 32.0f * scale, 32.0f * scale},
                        11.0f * scale,
                        WL::TEXT_TERTIARY,
                        2.0f * scale);
        glossary_y += 44.0f * scale;
        if (glossary_y > left.y + left.height - 40.0f * scale) {
            break;
        }
    }

    draw_card(right, {6, 14, 24, 230}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Law Preview",
              {right.x + 16.0f * scale, right.y + 16.0f * scale},
              20.0f * scale,
              WL::TEXT_PRIMARY);

    const Rectangle preview_rect = {right.x + 16.0f * scale, right.y + 50.0f * scale, right.width - 32.0f * scale, 220.0f * scale};
    draw_preview_path(preview_rect, seeded.result.law_preview.phase_path, WL::CYAN_CORE);

    draw_text("Primary tensors",
              {right.x + 16.0f * scale, preview_rect.y + preview_rect.height + 18.0f * scale},
              16.0f * scale,
              with_alpha(WL::CYAN_CORE, 180));
    const float matrix_y = preview_rect.y + preview_rect.height + 42.0f * scale;
    const float matrix_gap = 10.0f * scale;
    const float matrix_w = (right.width - 32.0f * scale - matrix_gap) * 0.5f;
    draw_matrix({right.x + 16.0f * scale, matrix_y, matrix_w, 120.0f * scale}, seeded.result.meta_spec.g, WL::CYAN_CORE, scale);
    draw_matrix({right.x + 16.0f * scale + matrix_w + matrix_gap, matrix_y, matrix_w, 120.0f * scale}, seeded.result.meta_spec.V, WL::VIOLET_CORE, scale);
    draw_matrix({right.x + 16.0f * scale, matrix_y + 132.0f * scale, matrix_w, 120.0f * scale}, seeded.result.meta_spec.S, WL::PLASMA_GREEN, scale);
    draw_matrix({right.x + 16.0f * scale + matrix_w + matrix_gap, matrix_y + 132.0f * scale, matrix_w, 120.0f * scale}, seeded.result.meta_spec.W, WL::XENON_CORE, scale);

    return result;
}
