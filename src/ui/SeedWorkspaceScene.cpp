#include "ui/SeedWorkspaceScene.hpp"

#include "app/SeededUniverseRuntime.hpp"
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

std::string first_paragraph(const std::string& text) {
    const std::size_t split = text.find("\n\n");
    if (split == std::string::npos) {
        return text;
    }
    return text.substr(0, split);
}

void draw_glossary_modal(bool& open, Rectangle viewport, float scale) {
    if (!open) {
        return;
    }

    DrawRectangleRec(viewport, {2, 8, 14, 200});
    const Rectangle modal = {
        viewport.x + viewport.width * 0.16f,
        viewport.y + viewport.height * 0.12f,
        viewport.width * 0.68f,
        viewport.height * 0.74f
    };
    if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), modal))
        || IsKeyPressed(KEY_ESCAPE)) {
        open = false;
        return;
    }

    draw_card(modal, {8, 16, 28, 246}, with_alpha(WL::CYAN_CORE, 110));
    draw_text("Glossary",
              {modal.x + 18.0f * scale, modal.y + 16.0f * scale},
              28.0f * scale,
              WL::TEXT_PRIMARY);
    float y = modal.y + 60.0f * scale;
    for (const GlossaryEntry& entry : Copy::glossary_entries()) {
        draw_text(entry.term,
                  {modal.x + 18.0f * scale, y},
                  16.0f * scale,
                  with_alpha(WL::CYAN_CORE, 200));
        draw_text_block(entry.detail,
                        {modal.x + 18.0f * scale, y + 18.0f * scale, modal.width - 36.0f * scale, 72.0f * scale},
                        13.0f * scale,
                        WL::TEXT_SECONDARY,
                        3.0f * scale);
        y += 86.0f * scale;
        if (y > modal.y + modal.height - 60.0f * scale) {
            break;
        }
    }

    draw_button({modal.x + modal.width - 90.0f * scale, modal.y + 14.0f * scale, 72.0f * scale, 30.0f * scale},
                "Close",
                {22, 40, 64, 230},
                {32, 58, 90, 255},
                WL::TEXT_PRIMARY,
                true,
                scale);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
        && CheckCollisionPointRec(GetMousePosition(),
                                  {modal.x + modal.width - 90.0f * scale, modal.y + 14.0f * scale, 72.0f * scale, 30.0f * scale})) {
        open = false;
    }
}

void draw_timeline(Rectangle rect,
                   SeededUniverseUiState& seeded,
                   float scale) {
    draw_card(rect, {8, 16, 28, 236}, with_alpha(WL::CYAN_DIM, 90));
    draw_text("Timeline",
              {rect.x + 14.0f * scale, rect.y + 12.0f * scale},
              16.0f * scale,
              WL::TEXT_PRIMARY);

    SeededUniverseRuntime* runtime = seeded.runtime.get();
    if (runtime == nullptr || runtime->history.size() < 2) {
        draw_text("Run the universe to build a recorded timeline.",
                  {rect.x + 14.0f * scale, rect.y + 38.0f * scale},
                  12.0f * scale,
                  WL::TEXT_SECONDARY);
        return;
    }

    const Rectangle plot = {
        rect.x + 14.0f * scale,
        rect.y + 40.0f * scale,
        rect.width - 28.0f * scale,
        rect.height - 52.0f * scale
    };
    DrawRectangleRounded(plot, 0.08f, 6, {6, 14, 24, 220});
    DrawRectangleRoundedLines(plot, 0.08f, 6, 1.0f, with_alpha(WL::GLASS_BORDER, 80));

    double min_p = runtime->history.front().p_value;
    double max_p = runtime->history.front().p_value;
    for (const UniverseSnapshot& snapshot : runtime->history) {
        min_p = std::min(min_p, snapshot.p_value);
        max_p = std::max(max_p, snapshot.p_value);
    }
    const double span = std::max(1.0e-6, max_p - min_p);
    for (std::size_t index = 0; index + 1 < runtime->history.size(); ++index) {
        const UniverseSnapshot& a = runtime->history[index];
        const UniverseSnapshot& b = runtime->history[index + 1];
        const float x0 = plot.x + static_cast<float>(index) / static_cast<float>(runtime->history.size() - 1) * plot.width;
        const float x1 = plot.x + static_cast<float>(index + 1) / static_cast<float>(runtime->history.size() - 1) * plot.width;
        const float y0 = plot.y + plot.height - static_cast<float>((a.p_value - min_p) / span) * plot.height;
        const float y1 = plot.y + plot.height - static_cast<float>((b.p_value - min_p) / span) * plot.height;
        DrawLineEx({x0, y0}, {x1, y1}, 1.5f, with_alpha(WL::CYAN_CORE, 190));
    }

    for (const TimelineMarker& marker : runtime->markers) {
        const float t = runtime->elapsed_time > 1.0e-6
            ? static_cast<float>(marker.time / std::max(runtime->elapsed_time, 1.0e-6))
            : 0.0f;
        const float x = plot.x + std::clamp(t, 0.0f, 1.0f) * plot.width;
        DrawLineEx({x, plot.y}, {x, plot.y + plot.height}, 1.0f, with_alpha(WL::XENON_CORE, 130));
    }

    const int active_index = runtime->active_snapshot_index >= 0
        ? runtime->active_snapshot_index
        : static_cast<int>(runtime->history.size()) - 1;
    const float active_t = static_cast<float>(active_index)
        / static_cast<float>(std::max<std::size_t>(1u, runtime->history.size() - 1));
    const float handle_x = plot.x + active_t * plot.width;
    DrawCircleV({handle_x, plot.y + plot.height * 0.5f}, 5.0f * scale, WL::XENON_CORE);

    if (CheckCollisionPointRec(GetMousePosition(), plot)
        && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        const float ratio = std::clamp((GetMousePosition().x - plot.x) / plot.width, 0.0f, 1.0f);
        const int index = static_cast<int>(ratio * static_cast<float>(runtime->history.size() - 1));
        runtime->mode = RunMode::PAUSED;
        runtime->scrub_to_index(index);
        seeded.scrub_active = true;
        seeded.workspace.selected_snapshot = index;
    }

    draw_text(metric_text(runtime->elapsed_time, 2) + " s",
              {plot.x + plot.width - 56.0f * scale, rect.y + 12.0f * scale},
              11.0f * scale,
              WL::TEXT_SECONDARY);
    draw_text("Drag to scrub",
              {rect.x + 14.0f * scale, rect.y + rect.height - 18.0f * scale},
              11.0f * scale,
              WL::TEXT_TERTIARY);
}

void draw_stage_overlay(Rectangle rect,
                        const SeededUniverseUiState& seeded,
                        const SeededUniverseRuntime* runtime,
                        float scale) {
    DrawRectangleRoundedLines(rect, 0.04f, 8, 1.2f, with_alpha(WL::CYAN_DIM, 80));
    DrawLineEx({rect.x + 12.0f * scale, rect.y + 1.0f},
               {rect.x + rect.width - 12.0f * scale, rect.y + 1.0f},
               1.0f,
               with_alpha(WL::CYAN_CORE, 45));

    const Rectangle label_bar = {rect.x + 14.0f * scale, rect.y + 14.0f * scale, rect.width - 28.0f * scale, 34.0f * scale};
    DrawRectangleRounded(label_bar, 0.12f, 8, {4, 10, 18, 148});
    draw_text("Live Stage",
              {label_bar.x + 12.0f * scale, label_bar.y + 9.0f * scale},
              14.0f * scale,
              WL::TEXT_PRIMARY);

    const bool live = runtime != nullptr && runtime->ready() && runtime->mode == RunMode::RUNNING && !runtime->scrubbing;
    const bool paused = runtime != nullptr && runtime->ready() && runtime->mode == RunMode::PAUSED && !runtime->scrubbing;
    const char* state_label = live ? "LIVE" : paused ? "PAUSED" : runtime != nullptr && runtime->scrubbing ? "SCRUB" : "IDLE";
    const Color badge_fill = live ? with_alpha(WL::PLASMA_DIM, 210)
        : paused ? with_alpha(WL::XENON_DIM, 210)
        : runtime != nullptr && runtime->scrubbing ? with_alpha(WL::VIOLET_DIM, 210)
        : Color{18, 46, 76, 210};
    const Color badge_text = live ? WL::PLASMA_GREEN
        : paused ? WL::XENON_CORE
        : runtime != nullptr && runtime->scrubbing ? WL::VIOLET_CORE
        : WL::ACCENT_GRAVITY;
    draw_badge({label_bar.x + label_bar.width - 86.0f * scale, label_bar.y + 5.0f * scale, 74.0f * scale, 22.0f * scale},
               state_label,
               badge_fill,
               badge_text,
               scale);

    const Rectangle footer = {rect.x + 14.0f * scale, rect.y + rect.height - 54.0f * scale, rect.width - 28.0f * scale, 40.0f * scale};
    DrawRectangleRounded(footer, 0.10f, 8, {4, 10, 18, 148});
    draw_text(seeded.workspace.title.empty() ? seeded.seed_input : seeded.workspace.title,
              {footer.x + 12.0f * scale, footer.y + 7.0f * scale},
              15.0f * scale,
              WL::TEXT_PRIMARY);
    std::string status = "Space pause  R restart  C clear trail  Tab trace";
    if (runtime != nullptr && runtime->ready()) {
        status = std::to_string(runtime->markers.size()) + " marker(s)  |  "
            + std::to_string(runtime->history.size()) + " samples  |  "
            + status;
    }
    draw_text_block(status,
                    {footer.x + 12.0f * scale, footer.y + 22.0f * scale, footer.width - 24.0f * scale, 14.0f * scale},
                    10.5f * scale,
                    WL::TEXT_TERTIARY,
                    2.0f * scale);
}

void draw_inspector(AppState& app,
                    Rectangle rect,
                    SeededUniverseUiState& seeded,
                    float scale,
                    SeedWorkspaceSceneResult& result) {
    draw_card(rect, {6, 14, 24, 234}, with_alpha(WL::GLASS_BORDER, 100));
    draw_text("Inspector",
              {rect.x + 16.0f * scale, rect.y + 14.0f * scale},
              22.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text("Project, metrics, notes, and scene actions.",
              {rect.x + 16.0f * scale, rect.y + 40.0f * scale},
              12.0f * scale,
              WL::TEXT_SECONDARY);
    draw_text("Project",
              {rect.x + 16.0f * scale, rect.y + 58.0f * scale},
              11.0f * scale,
              with_alpha(WL::CYAN_CORE, 175));

    const Rectangle title_rect = {rect.x + 16.0f * scale, rect.y + 70.0f * scale, rect.width - 32.0f * scale, 36.0f * scale};
    if (draw_text_field(title_rect, seeded.workspace.title, "Project title", seeded.title_input_active, WL::CYAN_CORE, scale)) {
        seeded.title_input_active = true;
        seeded.title_input_select_all = true;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), title_rect)) {
        seeded.title_input_active = false;
        seeded.title_input_select_all = false;
    }
    handle_text_input(seeded.workspace.title,
                      seeded.title_input_active,
                      seeded.title_input_select_all,
                      seeded.title_backspace_repeat_timer,
                      96u);

    const Rectangle note_rect = {rect.x + 16.0f * scale, title_rect.y + 48.0f * scale, rect.width - 32.0f * scale, 36.0f * scale};
    if (draw_text_field(note_rect, seeded.workspace.notes, "Short notes", seeded.notes_input_active, WL::VIOLET_CORE, scale)) {
        seeded.notes_input_active = true;
        seeded.notes_input_select_all = true;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), note_rect)) {
        seeded.notes_input_active = false;
        seeded.notes_input_select_all = false;
    }
    handle_text_input(seeded.workspace.notes,
                      seeded.notes_input_active,
                      seeded.notes_input_select_all,
                      seeded.notes_backspace_repeat_timer,
                      160u);

    SeededUniverseRuntime* runtime = seeded.runtime.get();
    const float metric_y = note_rect.y + 52.0f * scale;
    draw_text("Live state",
              {rect.x + 16.0f * scale, metric_y - 16.0f * scale},
              11.0f * scale,
              with_alpha(WL::CYAN_CORE, 175));
    if (runtime != nullptr && runtime->ready()) {
        draw_metric({rect.x + 16.0f * scale, metric_y, rect.width - 32.0f * scale, 52.0f * scale},
                    "Observed p",
                    metric_text(runtime->law_state.p, 3),
                    scale);
        draw_metric({rect.x + 16.0f * scale, metric_y + 60.0f * scale, rect.width - 32.0f * scale, 52.0f * scale},
                    "State radius",
                    metric_text(runtime->law_state.q.length(), 2),
                    scale);
        draw_metric({rect.x + 16.0f * scale, metric_y + 120.0f * scale, rect.width - 32.0f * scale, 52.0f * scale},
                    "Speed",
                    metric_text(runtime->law_state.v.length(), 2),
                    scale);
    }

    const float button_y = metric_y + 188.0f * scale;
    draw_text("Actions",
              {rect.x + 16.0f * scale, button_y - 16.0f * scale},
              11.0f * scale,
              with_alpha(WL::CYAN_CORE, 175));
    const float button_gap = 8.0f * scale;
    const float button_w = (rect.width - 32.0f * scale - button_gap) * 0.5f;

    if (draw_button({rect.x + 16.0f * scale, button_y, button_w, 32.0f * scale},
                    "Save",
                    {10, 84, 98, 240},
                    {18, 126, 140, 255},
                    WL::CYAN_CORE,
                    seeded.result.ready,
                    scale)) {
        UniverseProject project = make_universe_project(seeded);
        UniverseProject existing;
        if (!project.id.empty() && Storage::load_project(project.id, existing)) {
            project.created_at = existing.created_at;
        }
        project.updated_at = Storage::now_timestamp();
        if (project.created_at.empty()) {
            project.created_at = project.updated_at;
        }
        if (Storage::save_project(project)) {
            seeded.workspace.project_id = project.id;
            seeded.save_feedback_timer = 2.5f;
            app.settings.last_project_id = project.id;
            app.settings.recent_project_ids.erase(
                std::remove(app.settings.recent_project_ids.begin(), app.settings.recent_project_ids.end(), project.id),
                app.settings.recent_project_ids.end());
            app.settings.recent_project_ids.insert(app.settings.recent_project_ids.begin(), project.id);
            if (app.settings.recent_project_ids.size() > 8u) {
                app.settings.recent_project_ids.resize(8u);
            }
            result.refresh_catalog = true;
        }
    }
    if (draw_button({rect.x + 16.0f * scale + button_w + button_gap, button_y, button_w, 32.0f * scale},
                    "Pin Frame",
                    {28, 36, 62, 232},
                    {40, 56, 94, 255},
                    WL::TEXT_PRIMARY,
                    runtime != nullptr && runtime->ready(),
                    scale)) {
        runtime->pin_marker("Pinned frame");
    }

    const float button_y2 = button_y + 40.0f * scale;
    if (draw_button({rect.x + 16.0f * scale, button_y2, button_w, 32.0f * scale},
                    Copy::kTraceLabel,
                    {26, 42, 72, 236},
                    {38, 62, 102, 255},
                    WL::TEXT_PRIMARY,
                    seeded.result.ready,
                    scale)) {
        result.open_trace = true;
    }
    if (draw_button({rect.x + 16.0f * scale + button_w + button_gap, button_y2, button_w, 32.0f * scale},
                    Copy::kAtlasLabel,
                    {22, 36, 62, 232},
                    {34, 56, 92, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale)) {
        result.open_atlas = true;
    }

    const float button_y3 = button_y2 + 40.0f * scale;
    if (draw_button({rect.x + 16.0f * scale, button_y3, button_w, 32.0f * scale},
                    Copy::kReferenceLabel,
                    {24, 30, 46, 228},
                    {36, 46, 70, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale)) {
        result.open_reference = true;
    }
    if (draw_button({rect.x + 16.0f * scale + button_w + button_gap, button_y3, button_w, 32.0f * scale},
                    "Glossary",
                    with_alpha(WL::VIOLET_DIM, 220),
                    with_alpha(WL::VIOLET_CORE, 80),
                    WL::VIOLET_CORE,
                    true,
                    scale)) {
        seeded.workspace.glossary_open = true;
    }

    if (seeded.save_feedback_timer > 0.0f) {
        draw_text("Saved to Atlas",
                  {rect.x + 16.0f * scale, rect.y + rect.height - 22.0f * scale},
                  12.0f * scale,
                  with_alpha(WL::PLASMA_GREEN, 210));
    }
}

void draw_header(Rectangle rect,
                 SeededUniverseUiState& seeded,
                 float scale) {
    draw_card(rect, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 110));
    draw_text("Seed Workspace",
              {rect.x + 16.0f * scale, rect.y + 14.0f * scale},
              24.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text("Generated law, live renderer, timeline, and one-step access to detailed trace.",
              {rect.x + 16.0f * scale, rect.y + 42.0f * scale},
              13.0f * scale,
              WL::TEXT_SECONDARY);
    draw_badge({rect.x + rect.width - 180.0f * scale, rect.y + 16.0f * scale, 76.0f * scale, 22.0f * scale},
               "LOCAL",
               {16, 36, 62, 210},
               WL::TEXT_SECONDARY,
               scale);
    draw_badge({rect.x + rect.width - 94.0f * scale, rect.y + 16.0f * scale, 78.0f * scale, 22.0f * scale},
               "SEED",
               with_alpha(WL::CYAN_DIM, 190),
               WL::CYAN_CORE,
               scale);

    const Rectangle seed_rect = {rect.x + 16.0f * scale, rect.y + 68.0f * scale, rect.width * 0.34f, 36.0f * scale};
    if (draw_text_field(seed_rect, seeded.seed_input, "Seed", seeded.input_active, WL::CYAN_CORE, scale)) {
        seeded.input_active = true;
        seeded.input_select_all = true;
        seeded.backspace_repeat_timer = 0.0f;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), seed_rect)) {
        seeded.input_active = false;
        seeded.input_select_all = false;
    }
    handle_text_input(seeded.seed_input,
                      seeded.input_active,
                      seeded.input_select_all,
                      seeded.backspace_repeat_timer,
                      128u);

    const float button_x = seed_rect.x + seed_rect.width + 10.0f * scale;
    if (draw_button({button_x, seed_rect.y, 100.0f * scale, seed_rect.height},
                    "Generate",
                    {10, 84, 98, 240},
                    {18, 126, 140, 255},
                    WL::CYAN_CORE,
                    true,
                    scale)
        || (seeded.input_active && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))) {
        regenerate_seeded_universe(seeded);
    }

    SeededUniverseRuntime* runtime = seeded.runtime.get();
    const RunMode mode = (runtime != nullptr) ? runtime->mode : RunMode::STOPPED;
    if (draw_button({button_x + 110.0f * scale, seed_rect.y, 92.0f * scale, seed_rect.height},
                    mode == RunMode::PAUSED ? "Resume" : "Pause",
                    {22, 40, 64, 230},
                    {34, 58, 90, 255},
                    WL::TEXT_PRIMARY,
                    runtime != nullptr && runtime->ready(),
                    scale)) {
        runtime->mode = (mode == RunMode::PAUSED) ? RunMode::RUNNING : RunMode::PAUSED;
        if (runtime->mode == RunMode::RUNNING) {
            runtime->resume_live();
        }
    }

    if (draw_button({button_x + 212.0f * scale, seed_rect.y, 92.0f * scale, seed_rect.height},
                    "Restart",
                    with_alpha(WL::PLASMA_DIM, 228),
                    with_alpha(WL::PLASMA_GREEN, 80),
                    WL::PLASMA_GREEN,
                    runtime != nullptr && runtime->ready(),
                    scale)) {
        runtime->restart();
    }
}

} // namespace

SeedWorkspaceSceneResult draw_seed_workspace_scene(AppState& app,
                                                   Rectangle viewport) {
    SeedWorkspaceSceneResult result;
    SeededUniverseUiState& seeded = app.ui.seeded;
    if (!seeded.result.ready && seeded.result.error.empty()) {
        regenerate_seeded_universe(seeded);
    }
    if (seeded.save_feedback_timer > 0.0f) {
        seeded.save_feedback_timer = std::max(0.0f, seeded.save_feedback_timer - GetFrameTime());
    }

    const float scale = std::clamp(std::min(viewport.width / 1540.0f, viewport.height / 940.0f), 0.84f, 1.12f);
    const float margin = 18.0f * scale;
    const Rectangle header = {viewport.x + margin, viewport.y + margin, viewport.width - margin * 2.0f, 116.0f * scale};
    const float gap = 16.0f * scale;
    const float inspector_w = std::clamp(viewport.width * 0.24f, 300.0f * scale, 360.0f * scale);
    const float timeline_h = 132.0f * scale;
    const Rectangle stage = {
        viewport.x + margin,
        header.y + header.height + gap,
        viewport.width - margin * 2.0f - inspector_w - gap,
        viewport.height - header.height - timeline_h - margin * 3.0f - gap * 2.0f
    };
    const Rectangle inspector = {stage.x + stage.width + gap, stage.y, inspector_w, stage.height + timeline_h + gap};
    const Rectangle timeline = {stage.x, stage.y + stage.height + gap, stage.width, timeline_h};

    if (draw_back_to_menu_button(viewport, scale) || (!seeded.input_active && IsKeyPressed(KEY_ESCAPE))) {
        result.back_requested = true;
    }

    SeededUniverseRuntime* runtime = seeded.runtime.get();
    if (!seeded.input_active && runtime != nullptr && runtime->ready()) {
        if (IsKeyPressed(KEY_SPACE)) {
            runtime->mode = (runtime->mode == RunMode::PAUSED) ? RunMode::RUNNING : RunMode::PAUSED;
            if (runtime->mode == RunMode::RUNNING) {
                runtime->resume_live();
            }
        }
        if (IsKeyPressed(KEY_R)) runtime->restart();
        if (IsKeyPressed(KEY_C)) runtime->clear_trail();
        if (IsKeyPressed(KEY_TAB)) result.open_trace = true;
    }

    draw_header(header, seeded, scale);
    draw_inspector(app, inspector, seeded, scale, result);
    draw_timeline(timeline, seeded, scale);
    draw_stage_overlay(stage, seeded, runtime, scale);

    if (!seeded.result.ready && !seeded.result.error.empty()) {
        draw_text_block(std::string("Generation failed.\n") + seeded.result.error,
                        {stage.x + 18.0f * scale, stage.y + 18.0f * scale, stage.width - 36.0f * scale, 80.0f * scale},
                        20.0f * scale,
                        WL::XENON_CORE,
                        4.0f * scale);
    } else if (runtime != nullptr && runtime->ready()) {
        draw_metric({header.x + header.width - 290.0f * scale, header.y + 14.0f * scale, 84.0f * scale, 44.0f * scale},
                    "Gain",
                    metric_text(seeded.result.law_preview.linear_gain, 2),
                    scale);
        draw_metric({header.x + header.width - 196.0f * scale, header.y + 14.0f * scale, 84.0f * scale, 44.0f * scale},
                    "Peak",
                    metric_text(seeded.result.law_preview.max_accel, 2),
                    scale);
        draw_metric({header.x + header.width - 102.0f * scale, header.y + 14.0f * scale, 84.0f * scale, 44.0f * scale},
                    "p",
                    metric_text(runtime->law_state.p, 2),
                    scale);

        draw_text_block(first_paragraph(seeded.result.descriptor),
                        {inspector.x + 16.0f * scale, inspector.y + inspector.height - 116.0f * scale, inspector.width - 32.0f * scale, 88.0f * scale},
                        12.0f * scale,
                        WL::TEXT_TERTIARY,
                        3.0f * scale);
    }

    draw_glossary_modal(seeded.workspace.glossary_open, viewport, scale);
    return result;
}
