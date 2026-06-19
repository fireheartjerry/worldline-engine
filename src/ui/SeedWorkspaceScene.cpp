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
