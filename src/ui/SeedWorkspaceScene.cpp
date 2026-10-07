#include "ui/SeedWorkspaceInternal.hpp"

#include "app/SeededUniverseRuntime.hpp"
#include "app/UniverseProject.hpp"
#include "app/WorldlineCopy.hpp"
#include "app/WorldlineStorage.hpp"
#include "ui/TextInput.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cstdio>

using namespace seed_ws;

SeedWorkspaceSceneResult draw_seed_workspace_scene(AppState& app,
                                                   Rectangle viewport,
                                                   const FieldReadout& field_readout) {
    SeedWorkspaceSceneResult result;
    SeededUniverseUiState& seeded = app.ui.seeded;
    if (!seeded.result.ready && seeded.result.error.empty()) {
        regenerate_seeded_universe(seeded);
    }
    if (seeded.save_feedback_timer > 0.0f) {
        seeded.save_feedback_timer = std::max(0.0f, seeded.save_feedback_timer - GetFrameTime());
    }

    const SeedWorkspaceLayout L = seed_workspace_layout(viewport);
    const float scale = L.scale;
    const Rectangle header = L.header;
    const Rectangle stage = L.stage;
    const Rectangle inspector = L.inspector;
    const Rectangle timeline = L.timeline;

    if (!seeded.input_active && IsKeyPressed(KEY_ESCAPE)) {
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
        if (IsKeyPressed(KEY_V)) result.toggle_view = true;
    }

    // The back key lives inside the header card (drawn after it, so it is
    // visible; it used to sit underneath the card, hidden but still clickable).
    const Vector2 back_size = back_button_size(scale);
    draw_header(header, seeded, scale, back_size.x + 16.0f * scale);
    if (draw_back_button({header.x + 16.0f * scale, header.y + 14.0f * scale, back_size.x, back_size.y}, scale)) {
        result.back_requested = true;
    }
    draw_inspector(app, inspector, seeded, scale, result, field_readout);
    draw_timeline(timeline, seeded, scale);
    draw_stage_overlay(stage, seeded, runtime, scale, field_readout, result);

    if (!seeded.result.ready && !seeded.result.error.empty()) {
        draw_text_block(std::string("Generation failed.\n") + seeded.result.error,
                        {stage.x + 18.0f * scale, stage.y + 18.0f * scale, stage.width - 36.0f * scale, 80.0f * scale},
                        20.0f * scale,
                        WL::XENON_CORE,
                        4.0f * scale);
    } else if (runtime != nullptr && runtime->ready()) {
        draw_metric({header.x + header.width - 290.0f * scale, header.y + 62.0f * scale, 84.0f * scale, 44.0f * scale},
                    "Gain",
                    metric_text(seeded.result.law_preview.linear_gain, 2),
                    scale);
        draw_metric({header.x + header.width - 196.0f * scale, header.y + 62.0f * scale, 84.0f * scale, 44.0f * scale},
                    "Peak",
                    metric_text(seeded.result.law_preview.max_accel, 2),
                    scale);
        draw_metric({header.x + header.width - 102.0f * scale, header.y + 62.0f * scale, 84.0f * scale, 44.0f * scale},
                    "p",
                    metric_text(runtime->law_state.p, 2),
                    scale);

    }

    draw_glossary_modal(seeded.workspace.glossary_open, viewport, scale);
    return result;
}
