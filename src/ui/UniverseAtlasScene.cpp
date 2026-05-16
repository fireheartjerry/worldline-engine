#include "ui/UniverseAtlasScene.hpp"

#include "app/UniverseProject.hpp"
#include "app/WorldlineCopy.hpp"
#include "app/WorldlineStorage.hpp"
#include "ui/TextInput.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cstdio>

namespace {

std::string metric_text(double value, int precision = 2) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

void draw_fingerprint(Rectangle rect, const std::vector<Vec2>& points, Color accent) {
    DrawRectangleRounded(rect, 0.08f, 6, {8, 18, 30, 236});
    DrawRectangleRoundedLines(rect, 0.08f, 6, 1.0f, with_alpha(accent, 70));
    if (points.size() < 2) {
        return;
    }

    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const Vec2& a = points[index];
        const Vec2& b = points[index + 1];
        const Vector2 p0 = {
            rect.x + 8.0f + static_cast<float>(a.x) * (rect.width - 16.0f),
            rect.y + 8.0f + static_cast<float>(a.y) * (rect.height - 16.0f)
        };
        const Vector2 p1 = {
            rect.x + 8.0f + static_cast<float>(b.x) * (rect.width - 16.0f),
            rect.y + 8.0f + static_cast<float>(b.y) * (rect.height - 16.0f)
        };
        DrawLineEx(p0, p1, 1.6f, with_alpha(accent, 190));
    }
}

std::string first_line(const std::string& text) {
    const std::size_t split = text.find('\n');
    if (split == std::string::npos) {
        return text;
    }
    return text.substr(0, split);
}

void sync_selection(UniverseAtlasSceneState& atlas, int visible_count) {
    if (visible_count <= 0) {
        atlas.primary_index = 0;
        atlas.compare_index = -1;
        return;
    }
    atlas.primary_index = std::clamp(atlas.primary_index, 0, visible_count - 1);
    if (atlas.compare_index >= visible_count) {
        atlas.compare_index = -1;
    }
}

} // namespace

UniverseAtlasSceneResult draw_universe_atlas_scene(AppState& app,
                                                   Rectangle viewport) {
    UniverseAtlasSceneResult result;
    UniverseAtlasSceneState& atlas = app.ui.atlas;

    const float scale = std::clamp(std::min(viewport.width / 1540.0f, viewport.height / 940.0f), 0.86f, 1.14f);
    const float margin = 22.0f * scale;
    const Rectangle header = {viewport.x + margin, viewport.y + margin, viewport.width - margin * 2.0f, 88.0f * scale};
    const Rectangle list_rect = {viewport.x + margin, header.y + header.height + 14.0f * scale, viewport.width * 0.42f, viewport.height - header.height - margin * 3.0f};
    const Rectangle detail_rect = {list_rect.x + list_rect.width + 16.0f * scale, list_rect.y, viewport.width - (list_rect.width + margin * 2.0f + 16.0f * scale), list_rect.height};

    if (draw_back_to_menu_button(viewport, scale) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
    }

    draw_card(header, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 120));
    draw_text("Universe Atlas", {header.x + 16.0f * scale, header.y + 14.0f * scale}, 28.0f * scale, WL::TEXT_PRIMARY);
    draw_text("Saved deterministic universes with searchable metrics, notes, and compare view.",
              {header.x + 16.0f * scale, header.y + 46.0f * scale},
              15.0f * scale,
              WL::TEXT_SECONDARY);

    const Rectangle search_rect = {header.x + header.width - 466.0f * scale, header.y + 20.0f * scale, 190.0f * scale, 36.0f * scale};
    if (draw_text_field(search_rect, atlas.query, "Search", atlas.query_active, WL::CYAN_CORE, scale)) {
        atlas.query_active = true;
        atlas.query_select_all = true;
        atlas.backspace_repeat_timer = 0.0f;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), search_rect)) {
        atlas.query_active = false;
        atlas.query_select_all = false;
    }
    handle_text_input(atlas.query, atlas.query_active, atlas.query_select_all, atlas.backspace_repeat_timer, 96u);

    if (draw_button({header.x + header.width - 264.0f * scale, header.y + 20.0f * scale, 164.0f * scale, 36.0f * scale},
                    Copy::kReferenceLabel,
                    {24, 30, 48, 228},
                    {36, 46, 70, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale)) {
        result.open_reference = true;
    }
    if (draw_button({header.x + header.width - 88.0f * scale, header.y + 20.0f * scale, 72.0f * scale, 36.0f * scale},
                    "Refresh",
                    {22, 36, 62, 232},
                    {34, 56, 92, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale)) {
        result.refresh_catalog = true;
    }

    const std::vector<std::size_t> visible = Storage::query_catalog(app.catalog, {atlas.query});
    sync_selection(atlas, static_cast<int>(visible.size()));
    if (!visible.empty() && IsKeyPressed(KEY_ENTER)) {
        UniverseProject& project = app.catalog.projects[visible[static_cast<std::size_t>(atlas.primary_index)]];
        apply_universe_project(app.ui.seeded, project);
        app.settings.last_project_id = project.id;
        result.open_workspace = true;
    }

    draw_card(list_rect, {6, 14, 24, 226}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Saved universes", {list_rect.x + 16.0f * scale, list_rect.y + 14.0f * scale}, 20.0f * scale, WL::TEXT_PRIMARY);
    draw_text(std::to_string(visible.size()) + " result(s)", {list_rect.x + 16.0f * scale, list_rect.y + 40.0f * scale}, 12.0f * scale, WL::TEXT_SECONDARY);

    const Rectangle list_view = {list_rect.x + 12.0f * scale, list_rect.y + 64.0f * scale, list_rect.width - 24.0f * scale, list_rect.height - 76.0f * scale};
    const float row_h = 104.0f * scale;
    const float content_h = static_cast<float>(visible.size()) * (row_h + 8.0f * scale);
    const float max_scroll = std::max(0.0f, content_h - list_view.height);
    atlas.scroll = std::clamp(atlas.scroll, 0.0f, max_scroll);
    if (CheckCollisionPointRec(GetMousePosition(), list_view)) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) {
            atlas.scroll = std::clamp(atlas.scroll - wheel * 36.0f * scale, 0.0f, max_scroll);
        }
    }

    if (visible.empty()) {
        draw_text("No saved universes yet.",
                  {list_view.x + 12.0f * scale, list_view.y + 12.0f * scale},
                  18.0f * scale,
                  WL::TEXT_PRIMARY);
        draw_text_block("Save a universe from the workspace to build a local atlas. Search updates automatically once projects exist.",
                        {list_view.x + 12.0f * scale, list_view.y + 40.0f * scale, list_view.width - 24.0f * scale, 72.0f * scale},
                        13.0f * scale,
                        WL::TEXT_SECONDARY,
                        3.0f * scale);
    } else {
        BeginScissorMode(static_cast<int>(list_view.x), static_cast<int>(list_view.y), static_cast<int>(list_view.width), static_cast<int>(list_view.height));
        float y = list_view.y - atlas.scroll;
        for (std::size_t row = 0; row < visible.size(); ++row) {
            UniverseProject& project = app.catalog.projects[visible[row]];
            const Rectangle row_rect = {list_view.x, y, list_view.width, row_h};
            const bool selected = static_cast<int>(row) == atlas.primary_index;
            const bool compare = static_cast<int>(row) == atlas.compare_index;
            draw_card(row_rect,
                      selected ? Color{9, 22, 38, 238} : Color{7, 16, 28, 224},
                      with_alpha(compare ? WL::XENON_CORE : WL::CYAN_DIM, selected ? 130 : 80));
            if (CheckCollisionPointRec(GetMousePosition(), row_rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    atlas.compare_index = static_cast<int>(row);
                } else {
                    atlas.primary_index = static_cast<int>(row);
                }
            }

            draw_fingerprint({row_rect.x + 10.0f * scale, row_rect.y + 10.0f * scale, 82.0f * scale, row_rect.height - 20.0f * scale},
                             project.thumbnail_points,
                             project.dynamic_p ? WL::VIOLET_CORE : WL::CYAN_CORE);
            draw_text(project.title.empty() ? project.seed : project.title,
                      {row_rect.x + 104.0f * scale, row_rect.y + 12.0f * scale},
                      18.0f * scale,
                      WL::TEXT_PRIMARY);
            draw_text(project.seed,
                      {row_rect.x + 104.0f * scale, row_rect.y + 34.0f * scale},
                      12.0f * scale,
                      WL::TEXT_SECONDARY);
            draw_text(project.dynamic_p ? "Dynamic p" : "Seed-locked p",
                      {row_rect.x + 104.0f * scale, row_rect.y + 54.0f * scale},
                      12.0f * scale,
                      with_alpha(project.dynamic_p ? WL::VIOLET_CORE : WL::CYAN_CORE, 180));
            draw_text(first_line(project.descriptor),
                      {row_rect.x + 104.0f * scale, row_rect.y + 74.0f * scale},
                      11.0f * scale,
                      WL::TEXT_TERTIARY);
            y += row_h + 8.0f * scale;
        }
        EndScissorMode();
        draw_scrollbar(list_view, atlas.scroll, max_scroll);
    }

    draw_card(detail_rect, {6, 14, 24, 226}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Comparison", {detail_rect.x + 16.0f * scale, detail_rect.y + 14.0f * scale}, 20.0f * scale, WL::TEXT_PRIMARY);
    draw_text("Select a saved universe. Shift-click another result to compare it side by side. Press Enter to open the current selection.",
              {detail_rect.x + 16.0f * scale, detail_rect.y + 40.0f * scale},
              12.0f * scale,
              WL::TEXT_SECONDARY);

    auto draw_detail = [&](const UniverseProject* project, Rectangle rect, Color accent) {
        draw_card(rect, {8, 18, 30, 236}, with_alpha(accent, 90));
        if (project == nullptr) {
            draw_text("No selection", {rect.x + 14.0f * scale, rect.y + 18.0f * scale}, 16.0f * scale, WL::TEXT_SECONDARY);
            return;
        }

        draw_text(project->title.empty() ? project->seed : project->title,
                  {rect.x + 14.0f * scale, rect.y + 16.0f * scale},
                  22.0f * scale,
                  WL::TEXT_PRIMARY);
        draw_text(project->seed,
                  {rect.x + 14.0f * scale, rect.y + 42.0f * scale},
                  12.0f * scale,
                  WL::TEXT_SECONDARY);
        draw_fingerprint({rect.x + 14.0f * scale, rect.y + 68.0f * scale, rect.width - 28.0f * scale, 110.0f * scale},
                         project->thumbnail_points,
                         accent);
        draw_metric({rect.x + 14.0f * scale, rect.y + 190.0f * scale, rect.width * 0.5f - 21.0f * scale, 54.0f * scale},
                    "Gain",
                    metric_text(project->linear_gain, 2),
                    scale);
        draw_metric({rect.x + rect.width * 0.5f + 7.0f * scale, rect.y + 190.0f * scale, rect.width * 0.5f - 21.0f * scale, 54.0f * scale},
                    "Peak Accel",
                    metric_text(project->max_accel, 2),
                    scale);
        draw_text_block(first_line(project->descriptor),
                        {rect.x + 14.0f * scale, rect.y + 256.0f * scale, rect.width - 28.0f * scale, 72.0f * scale},
                        13.0f * scale,
                        WL::TEXT_SECONDARY,
                        3.0f * scale);
        if (draw_button({rect.x + 14.0f * scale, rect.y + rect.height - 48.0f * scale, rect.width - 28.0f * scale, 34.0f * scale},
                        "Open In Workspace",
                        {10, 84, 98, 240},
                        {18, 126, 140, 255},
                        accent,
                        true,
                        scale)) {
            apply_universe_project(app.ui.seeded, *project);
            app.settings.last_project_id = project->id;
            result.open_workspace = true;
        }
    };

    const UniverseProject* primary = visible.empty() ? nullptr : &app.catalog.projects[visible[static_cast<std::size_t>(atlas.primary_index)]];
    const UniverseProject* compare = (atlas.compare_index >= 0 && atlas.compare_index < static_cast<int>(visible.size()))
        ? &app.catalog.projects[visible[static_cast<std::size_t>(atlas.compare_index)]]
        : nullptr;
    const float detail_gap = 12.0f * scale;
    const float detail_w = (detail_rect.width - 16.0f * scale * 2.0f - detail_gap) * 0.5f;
    const Rectangle left = {detail_rect.x + 16.0f * scale, detail_rect.y + 68.0f * scale, detail_w, detail_rect.height - 84.0f * scale};
    const Rectangle right = {left.x + detail_w + detail_gap, left.y, detail_w, left.height};
    draw_detail(primary, left, WL::CYAN_CORE);
    draw_detail(compare, right, WL::XENON_CORE);

    return result;
}
