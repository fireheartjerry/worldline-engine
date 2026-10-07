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

// "2026-10-07 18:54:50" -> "2026-10-07 18:54"
std::string short_timestamp(const std::string& stamp) {
    return stamp.size() > 16 ? stamp.substr(0, 16) : stamp;
}

// Thumbnail points are normalised to 0..1; draw them aspect-correct and
// centred so a fingerprint looks the same in a square chip or a wide strip.
void draw_fingerprint(Rectangle rect, const std::vector<Vec2>& points, Color accent) {
    DrawRectangleRounded(rect, 0.08f, 6, {8, 18, 30, 236});
    DrawRectangleRoundedLines(rect, 0.08f, 6, 1.0f, with_alpha(accent, 70));
    if (points.size() < 2) {
        const char* none = "no preview";
        const float size = std::min(12.0f, rect.height * 0.3f);
        const Vector2 m = measure_ui_text(none, size);
        draw_text(none, {rect.x + (rect.width - m.x) * 0.5f, rect.y + (rect.height - m.y) * 0.5f},
                  size, WL::TEXT_INACTIVE);
        return;
    }
    const float inset = std::max(6.0f, std::min(rect.width, rect.height) * 0.10f);
    const float side = std::min(rect.width, rect.height) - inset * 2.0f;
    const float ox = rect.x + (rect.width - side) * 0.5f;
    const float oy = rect.y + (rect.height - side) * 0.5f;
    const float n = static_cast<float>(points.size() - 1);
    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const Vec2& a = points[index];
        const Vec2& b = points[index + 1];
        const Vector2 p0 = {ox + static_cast<float>(a.x) * side, oy + static_cast<float>(a.y) * side};
        const Vector2 p1 = {ox + static_cast<float>(b.x) * side, oy + static_cast<float>(b.y) * side};
        const float t = static_cast<float>(index) / n;
        DrawLineEx(p0, p1, 1.4f + 0.6f * t, with_alpha(accent, static_cast<unsigned char>(120 + 100 * t)));
    }
    const Vector2 head = {ox + static_cast<float>(points.back().x) * side, oy + static_cast<float>(points.back().y) * side};
    DrawCircleV(head, 2.6f, accent);
}

void sync_selection(UniverseAtlasSceneState& atlas, int visible_count) {
    if (visible_count <= 0) {
        atlas.primary_index = 0;
        atlas.compare_index = -1;
        return;
    }
    atlas.primary_index = std::clamp(atlas.primary_index, 0, visible_count - 1);
    if (atlas.compare_index >= visible_count || atlas.compare_index == atlas.primary_index) {
        atlas.compare_index = -1;
    }
}

const char* p_mode_label(const UniverseProject& project) {
    return project.dynamic_p ? "dynamic p" : "seed-locked p";
}

} // namespace

UniverseAtlasSceneResult draw_universe_atlas_scene(AppState& app,
                                                   Rectangle viewport) {
    UniverseAtlasSceneResult result;
    UniverseAtlasSceneState& atlas = app.ui.atlas;

    const float scale = std::clamp(std::min(viewport.width / 1500.0f, viewport.height / 900.0f), 0.90f, 1.20f);
    const float margin = 20.0f * scale;
    const float gap = 16.0f * scale;
    const Rectangle header = {viewport.x + margin, viewport.y + margin, viewport.width - margin * 2.0f, 76.0f * scale};
    const float body_y = header.y + header.height + gap;
    const float body_h = viewport.y + viewport.height - margin - body_y;
    const float list_w = std::floor((header.width - gap) * 0.42f);
    const Rectangle list_rect = {header.x, body_y, list_w, body_h};
    const Rectangle detail_rect = {list_rect.x + list_w + gap, body_y, header.width - list_w - gap, body_h};

    // ── Header: back key, title, search, actions ──────────────────────────────
    draw_card(header, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 120));
    const Vector2 back_size = back_button_size(scale);
    if (draw_back_button({header.x + 16.0f * scale, header.y + (header.height - back_size.y) * 0.5f, back_size.x, back_size.y}, scale)) {
        result.back_requested = true;
    }
    const float title_x = header.x + 16.0f * scale + back_size.x + 20.0f * scale;
    DrawLineEx({title_x - 10.0f * scale, header.y + 16.0f * scale}, {title_x - 10.0f * scale, header.y + header.height - 16.0f * scale},
               1.0f, with_alpha(WL::GLASS_BORDER, 90));

    const float refresh_w = 96.0f * scale;
    const float ref_w = 176.0f * scale;
    const float search_w = std::clamp(header.width * 0.24f, 200.0f * scale, 320.0f * scale);
    const float ctl_h = 38.0f * scale;
    const float ctl_y = header.y + (header.height - ctl_h) * 0.5f;
    const float refresh_x = header.x + header.width - 16.0f * scale - refresh_w;
    const float ref_x = refresh_x - 10.0f * scale - ref_w;
    const Rectangle search_rect = {ref_x - 14.0f * scale - search_w, ctl_y, search_w, ctl_h};
    const float title_w = search_rect.x - 16.0f * scale - title_x;

    draw_text_fit("Universe Atlas", {title_x, header.y + 14.0f * scale}, title_w, 26.0f * scale, 18.0f * scale, WL::TEXT_PRIMARY);
    draw_text_fit("Saved universes: search, inspect, and compare two side by side.",
                  {title_x, header.y + 46.0f * scale}, title_w, 14.0f * scale, 11.0f * scale, WL::TEXT_SECONDARY);

    if (draw_text_field(search_rect, atlas.query, "Search title, seed, notes", atlas.query_active,
                        atlas.query_select_all, WL::CYAN_CORE, scale)) {
        atlas.query_active = true;
        atlas.query_select_all = true;
        atlas.backspace_repeat_timer = 0.0f;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), search_rect)) {
        atlas.query_active = false;
        atlas.query_select_all = false;
    }
    // Ctrl+F focuses the search; Esc leaves it before it leaves the screen.
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_F)) {
        atlas.query_active = true;
        atlas.query_select_all = true;
    }
    bool escape_consumed = false;
    if (atlas.query_active && IsKeyPressed(KEY_ESCAPE)) {
        atlas.query_active = false;
        atlas.query_select_all = false;
        escape_consumed = true;
    }
    handle_text_input(atlas.query, atlas.query_active, atlas.query_select_all, atlas.backspace_repeat_timer, 96u);
    if (!atlas.query.empty()) {
        const float cs = 22.0f * scale;
        if (draw_button({search_rect.x + search_rect.width - cs - 8.0f * scale, search_rect.y + (search_rect.height - cs) * 0.5f, cs, cs},
                        "x", {20, 36, 56, 220}, {34, 58, 88, 255}, WL::TEXT_SECONDARY, true, scale)) {
            atlas.query.clear();
            atlas.query_select_all = false;
            atlas.scroll = 0.0f;
        }
    }
    if (!escape_consumed && IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
    }

    if (draw_button({ref_x, ctl_y, ref_w, ctl_h}, Copy::kReferenceLabel,
                    {24, 30, 48, 228}, {36, 46, 70, 255}, WL::TEXT_PRIMARY, true, scale)) {
        result.open_reference = true;
    }
    if (draw_button({refresh_x, ctl_y, refresh_w, ctl_h}, "Refresh",
                    {22, 36, 62, 232}, {34, 56, 92, 255}, WL::TEXT_PRIMARY, true, scale)) {
        result.refresh_catalog = true;
    }

    const std::vector<std::size_t> visible = Storage::query_catalog(app.catalog, {atlas.query});
    const int visible_count = static_cast<int>(visible.size());
    sync_selection(atlas, visible_count);

    // ── List ──────────────────────────────────────────────────────────────────
    draw_card(list_rect, {6, 14, 24, 226}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Saved universes", {list_rect.x + 16.0f * scale, list_rect.y + 14.0f * scale}, 19.0f * scale, WL::TEXT_PRIMARY);
    {
        std::string count = std::to_string(visible.size());
        if (!atlas.query.empty()) count += " of " + std::to_string(app.catalog.projects.size());
        count += (app.catalog.projects.size() == 1 && atlas.query.empty()) ? " universe" : " universes";
        draw_text(count, {list_rect.x + 16.0f * scale, list_rect.y + 40.0f * scale}, 12.5f * scale, WL::TEXT_TERTIARY);
        if (visible_count > 0) {
            const char* keys = "Up/Down select  |  Enter open";
            const float kw = measure_ui_text(keys, 11.5f * scale).x;
            draw_text(keys, {list_rect.x + list_rect.width - 16.0f * scale - kw, list_rect.y + 41.0f * scale},
                      11.5f * scale, with_alpha(WL::TEXT_TERTIARY, 150));
        }
    }

    const Rectangle list_view = {list_rect.x + 10.0f * scale, list_rect.y + 64.0f * scale,
                                 list_rect.width - 20.0f * scale, list_rect.height - 74.0f * scale};
    const float row_h = 92.0f * scale;
    const float row_gap = 8.0f * scale;
    const float content_h = static_cast<float>(visible.size()) * (row_h + row_gap) - (visible.empty() ? 0.0f : row_gap);
    const float max_scroll = std::max(0.0f, content_h - list_view.height);
    const bool scrollable = max_scroll > 0.0f;
    const Vector2 mouse = GetMousePosition();
    const bool mouse_in_list = CheckCollisionPointRec(mouse, list_view);
    if (mouse_in_list) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) {
            atlas.scroll -= wheel * 48.0f * scale;
        }
    }

    // Keyboard selection (search field is single-line, so arrows are free).
    if (visible_count > 0) {
        int moved = 0;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) moved = 1;
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) moved = -1;
        if (moved != 0) {
            atlas.primary_index = std::clamp(atlas.primary_index + moved, 0, visible_count - 1);
            if (atlas.compare_index == atlas.primary_index) atlas.compare_index = -1;
            const float top = static_cast<float>(atlas.primary_index) * (row_h + row_gap);
            if (top < atlas.scroll) atlas.scroll = top;
            if (top + row_h > atlas.scroll + list_view.height) atlas.scroll = top + row_h - list_view.height;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            UniverseProject& project = app.catalog.projects[visible[static_cast<std::size_t>(atlas.primary_index)]];
            apply_universe_project(app.ui.seeded, project);
            app.settings.last_project_id = project.id;
            result.open_workspace = true;
        }
    }
    atlas.scroll = std::clamp(atlas.scroll, 0.0f, max_scroll);

    if (visible.empty()) {
        // Empty states: nothing saved yet vs. nothing matching the search.
        const bool searching = !app.catalog.projects.empty();
        const float cx = list_view.x + list_view.width * 0.5f;
        const float cy = list_view.y + list_view.height * 0.32f;
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 30.0f * scale, with_alpha(WL::CYAN_DIM, 120));
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 18.0f * scale, with_alpha(WL::CYAN_DIM, 80));
        DrawCircleV({cx, cy}, 3.0f * scale, with_alpha(WL::CYAN_CORE, 200));
        const std::string headline = searching ? "No universes match \"" + atlas.query + "\"" : "No saved universes yet";
        const float hs = 18.0f * scale;
        const std::string shown = ellipsize_ui_text(headline, list_view.width - 40.0f * scale, hs);
        const float hw = measure_ui_text(shown, hs).x;
        draw_text(shown, {cx - hw * 0.5f, cy + 46.0f * scale}, hs, WL::TEXT_PRIMARY);
        const char* sub = searching ? "Try a seed, a title word, or part of a note."
                                    : "Generate a universe and press Save in the workspace to keep it here.";
        const Rectangle sub_rect = {list_view.x + 30.0f * scale, cy + 76.0f * scale, list_view.width - 60.0f * scale, 44.0f * scale};
        const int lines = count_wrapped_ui_lines(sub, sub_rect.width, 13.5f * scale);
        if (lines == 1) {
            const float sw = measure_ui_text(sub, 13.5f * scale).x;
            draw_text(sub, {cx - sw * 0.5f, sub_rect.y}, 13.5f * scale, WL::TEXT_SECONDARY);
        } else {
            draw_text_block_fit(sub, sub_rect, 13.5f * scale, WL::TEXT_SECONDARY, 4.0f * scale);
        }
        const float bw = 220.0f * scale;
        const Rectangle cta = {cx - bw * 0.5f, cy + 130.0f * scale, bw, 38.0f * scale};
        if (searching) {
            if (draw_button(cta, "Clear search", {22, 36, 62, 232}, {34, 56, 92, 255}, WL::TEXT_PRIMARY, true, scale)) {
                atlas.query.clear();
                atlas.scroll = 0.0f;
            }
        } else if (draw_button(cta, "Create a universe", {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, scale)) {
            result.back_requested = true;
        }
    } else {
        const float row_w = list_view.width - (scrollable ? 12.0f * scale : 0.0f);
        BeginScissorMode(static_cast<int>(list_view.x), static_cast<int>(list_view.y),
                         static_cast<int>(list_view.width), static_cast<int>(list_view.height));
        float y = list_view.y - atlas.scroll;
        for (std::size_t row = 0; row < visible.size(); ++row, y += row_h + row_gap) {
            if (y + row_h < list_view.y || y > list_view.y + list_view.height) continue;   // off-screen
            UniverseProject& project = app.catalog.projects[visible[row]];
            const Rectangle row_rect = {list_view.x, y, row_w, row_h};
            const bool selected = static_cast<int>(row) == atlas.primary_index;
            const bool compare = static_cast<int>(row) == atlas.compare_index;
            const bool hot = mouse_in_list && CheckCollisionPointRec(mouse, row_rect);
            const Color accent = project.dynamic_p ? WL::VIOLET_CORE : WL::CYAN_CORE;

            if (hot) ui_request_cursor(MOUSE_CURSOR_POINTING_HAND);
            if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    if (!selected) atlas.compare_index = compare ? -1 : static_cast<int>(row);
                } else {
                    atlas.primary_index = static_cast<int>(row);
                    if (compare) atlas.compare_index = -1;
                }
            }

            if (selected) {
                draw_card_accented(row_rect, {12, 30, 48, 242}, with_alpha(WL::CYAN_CORE, 150), WL::CYAN_CORE);
            } else if (compare) {
                draw_card_accented(row_rect, {26, 22, 20, 236}, with_alpha(WL::XENON_CORE, 150), WL::XENON_CORE);
            } else {
                draw_card(row_rect, hot ? Color{11, 24, 40, 236} : Color{7, 16, 28, 224},
                          with_alpha(WL::CYAN_DIM, hot ? 120 : 70));
            }

            const float fp = row_h - 18.0f * scale;
            draw_fingerprint({row_rect.x + 12.0f * scale, row_rect.y + 9.0f * scale, fp, fp}, project.thumbnail_points, accent);

            const float tx = row_rect.x + 12.0f * scale + fp + 14.0f * scale;
            float tw = row_rect.x + row_rect.width - 12.0f * scale - tx;
            if (selected || compare) {
                const char* tag = selected ? "SELECTED" : "COMPARE";
                const float bw = measure_ui_text(tag, 11.0f * scale).x + 18.0f * scale;
                draw_badge({row_rect.x + row_rect.width - 12.0f * scale - bw, row_rect.y + 10.0f * scale, bw, 20.0f * scale},
                           tag, selected ? with_alpha(WL::CYAN_DIM, 200) : with_alpha(WL::XENON_DIM, 210),
                           selected ? WL::CYAN_CORE : WL::XENON_CORE, scale * 0.8f);
                tw -= bw + 8.0f * scale;
            }
            const bool titled = !project.title.empty() && project.title != project.seed;
            draw_text_fit(titled ? project.title : project.seed, {tx, row_rect.y + 11.0f * scale}, tw,
                          17.5f * scale, 14.0f * scale, WL::TEXT_PRIMARY);
            const float full_w = row_rect.x + row_rect.width - 12.0f * scale - tx;
            const std::string meta = (titled ? "seed \"" + project.seed + "\"  |  " : std::string()) + p_mode_label(project)
                + "  |  " + short_timestamp(project.updated_at);
            draw_text_fit(meta, {tx, row_rect.y + 36.0f * scale}, full_w, 12.5f * scale, 10.5f * scale,
                          with_alpha(accent, 190));
            draw_text_block_fit(ui_prose_summary(project.descriptor),
                                {tx, row_rect.y + 56.0f * scale, full_w, row_h - 62.0f * scale},
                                12.0f * scale, WL::TEXT_TERTIARY, 3.0f * scale);
        }
        EndScissorMode();
        draw_scrollbar(list_view, atlas.scroll, max_scroll);
    }

    // ── Comparison ────────────────────────────────────────────────────────────
    draw_card(detail_rect, {6, 14, 24, 226}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Comparison", {detail_rect.x + 16.0f * scale, detail_rect.y + 14.0f * scale}, 19.0f * scale, WL::TEXT_PRIMARY);
    draw_text_fit("Click a universe to inspect it. Shift-click another to compare side by side.",
                  {detail_rect.x + 16.0f * scale, detail_rect.y + 40.0f * scale}, detail_rect.width - 32.0f * scale,
                  12.5f * scale, 10.5f * scale, WL::TEXT_SECONDARY);

    auto draw_detail = [&](const UniverseProject* project, Rectangle rect, Color accent, bool is_compare) {
        if (project == nullptr) {
            // Placeholder that explains what goes here rather than an empty box.
            DrawRectangleRounded(rect, 0.04f, 8, {8, 16, 28, 150});
            DrawRectangleRoundedLines(rect, 0.04f, 8, 1.0f, with_alpha(accent, 60));
            const char* head = is_compare ? "Nothing to compare yet" : "Nothing selected";
            const char* body = is_compare ? "Shift-click a second universe in the list to see it here, next to the selection."
                                          : (app.catalog.projects.empty() ? "Saved universes will show their metrics and fingerprint here."
                                                                          : "Click a universe in the list.");
            const float hs = 16.0f * scale;
            const float hw = measure_ui_text(head, hs).x;
            const float cy = rect.y + rect.height * 0.40f;
            DrawCircleLines(static_cast<int>(rect.x + rect.width * 0.5f), static_cast<int>(cy - 34.0f * scale),
                            16.0f * scale, with_alpha(accent, 110));
            draw_text(head, {rect.x + (rect.width - hw) * 0.5f, cy}, hs, WL::TEXT_SECONDARY);
            draw_text_block_fit(body, {rect.x + 24.0f * scale, cy + 28.0f * scale, rect.width - 48.0f * scale, 60.0f * scale},
                                13.0f * scale, WL::TEXT_TERTIARY, 3.0f * scale);
            return;
        }

        draw_card(rect, {8, 18, 30, 236}, with_alpha(accent, 100));
        const float pad = 16.0f * scale;
        const float inner = rect.width - pad * 2.0f;
        float title_w = inner;
        if (is_compare) {
            const float cw = 70.0f * scale;
            if (draw_button({rect.x + rect.width - pad - cw, rect.y + 12.0f * scale, cw, 26.0f * scale}, "Clear",
                            {30, 24, 22, 220}, {52, 38, 30, 255}, WL::XENON_CORE, true, scale)) {
                atlas.compare_index = -1;
            }
            title_w -= cw + 8.0f * scale;
        }
        const bool titled = !project->title.empty() && project->title != project->seed;
        draw_text_fit(titled ? project->title : project->seed, {rect.x + pad, rect.y + 14.0f * scale}, title_w,
                      21.0f * scale, 15.0f * scale, WL::TEXT_PRIMARY);
        const std::string meta = (titled ? "seed \"" + project->seed + "\"  |  " : std::string()) + "saved "
            + short_timestamp(project->updated_at);
        draw_text_fit(meta, {rect.x + pad, rect.y + 42.0f * scale}, inner, 12.5f * scale, 10.5f * scale, with_alpha(accent, 190));

        const float button_h = 38.0f * scale;
        const float bottom = rect.y + rect.height - pad - button_h - 12.0f * scale;
        float y = rect.y + 66.0f * scale;
        const float fp_h = std::clamp(rect.height * 0.22f, 70.0f * scale, 150.0f * scale);
        draw_fingerprint({rect.x + pad, y, inner, fp_h}, project->thumbnail_points, accent);
        y += fp_h + 10.0f * scale;

        const float tile_gap = 8.0f * scale;
        const float tile_w = (inner - tile_gap) * 0.5f;
        const float tile_h = 50.0f * scale;
        const std::string p_value = project->dynamic_p
            ? metric_text(project->p_min, 2) + " .. " + metric_text(project->p_max, 2)
            : metric_text(project->seeded_p, 3);
        const std::pair<const char*, std::string> tiles[] = {
            {"Gain", metric_text(project->linear_gain, 2)},
            {"Peak Accel", metric_text(project->max_accel, 2)},
            {"Accel Ceiling", metric_text(project->accel_ceiling, 2)},
            {"Radius Mean", metric_text(project->radius_mean, 2)},
            {"Handedness", metric_text(project->handedness, 2)},
            {project->dynamic_p ? "p range" : "p (seed-locked)", p_value},
        };
        // Show as many metric rows as fit above the summary/button.
        const int rows_fit = std::clamp(static_cast<int>((bottom - y + tile_gap) / (tile_h + tile_gap)), 1, 3);
        for (int i = 0; i < rows_fit * 2; ++i) {
            const float tx = rect.x + pad + static_cast<float>(i % 2) * (tile_w + tile_gap);
            const float ty = y + static_cast<float>(i / 2) * (tile_h + tile_gap);
            draw_metric({tx, ty, tile_w, tile_h}, tiles[i].first, tiles[i].second, scale);
        }
        y += static_cast<float>(rows_fit) * (tile_h + tile_gap) + 6.0f * scale;

        if (!project->notes.empty() && bottom - y > 40.0f * scale) {
            draw_text("NOTES", {rect.x + pad, y}, 11.0f * scale, with_alpha(accent, 170));
            y += 16.0f * scale;
            y += draw_text_block_fit(project->notes, {rect.x + pad, y, inner, std::min(48.0f * scale, bottom - y)},
                                     13.0f * scale, WL::TEXT_SECONDARY, 3.0f * scale) + 10.0f * scale;
        }
        if (bottom - y > 16.0f * scale) {
            draw_text_block_fit(ui_prose_summary(project->descriptor), {rect.x + pad, y, inner, bottom - y},
                                13.0f * scale, WL::TEXT_SECONDARY, 3.5f * scale);
        }

        if (draw_button({rect.x + pad, rect.y + rect.height - pad - button_h, inner, button_h},
                        "Open in Workspace",
                        is_compare ? Color{70, 40, 14, 236} : Color{10, 84, 98, 240},
                        is_compare ? Color{104, 60, 20, 255} : Color{18, 126, 140, 255},
                        accent,
                        true,
                        scale)) {
            apply_universe_project(app.ui.seeded, *project);
            app.settings.last_project_id = project->id;
            result.open_workspace = true;
        }
    };

    const UniverseProject* primary = visible.empty() ? nullptr : &app.catalog.projects[visible[static_cast<std::size_t>(atlas.primary_index)]];
    const UniverseProject* compare = (atlas.compare_index >= 0 && atlas.compare_index < visible_count)
        ? &app.catalog.projects[visible[static_cast<std::size_t>(atlas.compare_index)]]
        : nullptr;
    const float detail_gap = 12.0f * scale;
    const float detail_w = (detail_rect.width - 16.0f * scale * 2.0f - detail_gap) * 0.5f;
    const Rectangle left = {detail_rect.x + 16.0f * scale, detail_rect.y + 66.0f * scale, detail_w, detail_rect.height - 82.0f * scale};
    const Rectangle right = {left.x + detail_w + detail_gap, left.y, detail_w, left.height};
    draw_detail(primary, left, WL::CYAN_CORE, false);
    draw_detail(compare, right, WL::XENON_CORE, true);

    return result;
}
