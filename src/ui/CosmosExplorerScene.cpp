#include "ui/CosmosExplorerInternal.hpp"
#include "ui/CosmosNavigator.hpp"

#include "app/WorldlineStorage.hpp"
#include "cosmos/Analysis.hpp"
#include "cosmos/Sandbox.hpp"
#include "renderer/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace cosmos;
using namespace cosmos_ui;

void CosmosState::configure(const std::string& seed_text) {
    Universe u = generate_universe(seed_text);
    seed = u.seed;
    genome = u.genome;
    classification = u.classification;
    palette = u.palette;
    catalog = std::move(u.catalog);
    initialized = true;
    selected_object = 0;
    compare_object = -1;
    clear_sim();
}

void CosmosState::set_scale(Scale next) {
    scale = next;
    selected_object = 0;
    compare_object = -1;
}

void CosmosState::clear_sim() {
    system.bodies.clear();
    has_sim = false;
    running = false;
    step_count = 0;
    accumulator = 0.0;
    elapsed = 0.0;
}

void CosmosState::spawn() {
    clear_sim();
    populate_sandbox(system, catalog, genome, scale);
    has_sim = !system.bodies.empty();
    running = has_sim;
}

void step_cosmos(CosmosState& cosmos, float frame_time) {
    if (!cosmos.has_sim || !cosmos.running) {
        return;
    }
    // Fixed-step accumulator: deterministic and frame-rate independent, so the
    // sandbox can be reproduced exactly from its step count.
    cosmos.accumulator += std::min(static_cast<double>(frame_time), 0.05);
    int steps = 0;
    while (cosmos.accumulator >= kSandboxDt && steps < 8) {
        advance_sandbox(cosmos.system, 1);
        cosmos.accumulator -= kSandboxDt;
        ++cosmos.step_count;
        ++steps;
    }
    cosmos.elapsed = cosmos.step_count * kSandboxDt;
}

CosmosExplorerResult draw_cosmos_explorer_scene(AppState& app,
                                                CosmosState& cosmos,
                                                Renderer& renderer,
                                                Rectangle viewport) {
    CosmosExplorerResult result;

    if (!cosmos.initialized || cosmos.seed != app.ui.seeded.seed_input) {
        cosmos.configure(app.ui.seeded.seed_input);
    }

    const float scale =
        std::clamp(std::min(viewport.width / 1500.0f, viewport.height / 920.0f), 0.85f, 1.2f);
    const float margin = 26.0f * scale;
    const float top = viewport.y + 76.0f * scale;
    const float gap = 16.0f * scale;
    const float ladder_w = 210.0f * scale;
    const float panel_w = 360.0f * scale;
    const float controls_h = 46.0f * scale;

    const float column_h = viewport.y + viewport.height - margin - top;
    const Rectangle ladder = {viewport.x + margin, top, ladder_w, column_h};
    const Rectangle panel = {viewport.x + viewport.width - margin - panel_w, top, panel_w, column_h};
    const float stage_x = ladder.x + ladder_w + gap;
    const float stage_w = panel.x - gap - stage_x;
    const Rectangle stage = {stage_x, top, stage_w, column_h - controls_h - 10.0f * scale};
    const Rectangle controls = {stage_x, stage.y + stage.height + 10.0f * scale, stage_w, controls_h};

    // ── Header dossier (offset to clear the top-left back button) ────────────
    const float header_x = viewport.x + 200.0f * scale;
    const UniverseClassification& cls = cosmos.classification;
    draw_text("COSMOS EXPLORER", {header_x, viewport.y + 18.0f * scale},
              22.0f * scale, WL::TEXT_PRIMARY);
    const std::string dossier = cls.codename + "   " + cls.class_name + "   -   complexity " +
                                std::to_string(static_cast<int>(cls.complexity * 100.0 + 0.5)) +
                                "%   [dossier]";
    const Rectangle dossier_hit = {header_x, viewport.y + 42.0f * scale, 540.0f * scale,
                                   20.0f * scale};
    const bool dossier_hot = CheckCollisionPointRec(GetMousePosition(), dossier_hit);
    draw_text(dossier, {header_x, viewport.y + 44.0f * scale}, 14.0f * scale,
              palette_color(cosmos.palette.accent, dossier_hot ? 255 : 220));
    if (clicked(dossier_hit)) {
        cosmos.dossier_open = !cosmos.dossier_open;
    }
    std::string traits;
    for (std::size_t i = 0; i < cls.traits.size() && i < 4; ++i) {
        traits += (i ? "  -  " : "") + cls.traits[i];
    }
    draw_text("seed '" + cosmos.seed + "'   |   " + traits,
              {header_x, viewport.y + 62.0f * scale}, 11.5f * scale,
              with_alpha(WL::TEXT_TERTIARY, 220));

    // Static UI is drawn first. The N-body stage uses a scissor + render-texture
    // passes, so it is rendered LAST to guarantee it cannot clip the panels.

    // ── Controls ─────────────────────────────────────────────────────────────
    const float btn_gap = 8.0f * scale;
    const float btn_w = (controls.width - btn_gap * 4.0f) / 5.0f;
    auto control_rect = [&](int i) {
        return Rectangle{controls.x + i * (btn_w + btn_gap), controls.y, btn_w, controls.height};
    };
    if (draw_button(control_rect(0), cosmos.has_sim ? "Re-spawn" : "Spawn",
                    {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, scale)) {
        cosmos.spawn();
    }
    if (draw_button(control_rect(1), cosmos.running ? "Pause" : "Resume",
                    {18, 40, 36, 235}, {26, 70, 56, 255}, WL::PLASMA_GREEN, cosmos.has_sim, scale)) {
        cosmos.running = !cosmos.running;
    }
    if (draw_button(control_rect(2), "Clear", {26, 30, 48, 228}, {38, 46, 70, 255},
                    WL::TEXT_PRIMARY, cosmos.has_sim, scale)) {
        cosmos.clear_sim();
    }
    if (draw_button(control_rect(3), "Save", {30, 22, 60, 232}, {46, 32, 92, 255},
                    WL::VIOLET_CORE, cosmos.has_sim, scale)) {
        save_current_sandbox(cosmos);
    }
    if (draw_button(control_rect(4), "Saved...", {22, 34, 58, 230}, {32, 50, 86, 255},
                    WL::TEXT_PRIMARY, true, scale)) {
        cosmos.browser_open = !cosmos.browser_open;
    }

    // ── Side columns ─────────────────────────────────────────────────────────
    draw_ladder(cosmos, ladder, scale);

    const float obs_h = 158.0f * scale;
    const Rectangle library = {panel.x, panel.y, panel.width, panel.height - obs_h - 10.0f * scale};
    const Rectangle observables = {panel.x, library.y + library.height + 10.0f * scale, panel.width,
                                   obs_h};
    draw_inspector(cosmos, library, scale);
    draw_observables(cosmos, observables, scale);

    if (draw_back_to_menu_button(viewport, scale)) {
        result.back_requested = true;
    }

    // ── Stage (the live N-body sandbox) — rendered last ──────────────────────
    draw_universe_backdrop(cosmos.palette, cosmos.genome.signature, stage,
                           static_cast<float>(GetTime()));

    const float cx = stage.x + stage.width * 0.5f;
    const float cy = stage.y + stage.height * 0.5f;

    // Advance the navigation camera (zoom/pan + tier traversal), disabled while a
    // modal is up so it never fights an overlay.
    const bool nav_interactive = !cosmos.browser_open && !cosmos.dossier_open;
    update_cosmos_camera(cosmos, stage, GetFrameTime(), nav_interactive);

    const float zoomf = static_cast<float>(cosmos.camera.zoom);
    std::vector<Renderer::FieldSprite> sprites;
    sprites.reserve(cosmos.system.bodies.size());
    for (const Body& b : cosmos.system.bodies) {
        Renderer::FieldSprite s;
        s.pos = cosmos_world_to_screen(b.pos, stage, cosmos.camera);
        s.radius = std::clamp(static_cast<float>(2.5 + b.radius * 3.0) * std::sqrt(zoomf),
                              1.5f, 26.0f);
        s.color = to_raylib(b.color);
        sprites.push_back(s);
    }

    if (cosmos.has_sim && !cosmos.browser_open && !cosmos.dossier_open) {
        if (cosmos.running) {
            renderer.accumulate_field(sprites, stage, 24);
        }
        renderer.draw_field(sprites, stage);
        draw_text(std::string(tier_for(cosmos.scale).name) + " sandbox  -  " +
                      std::to_string(cosmos.system.bodies.size()) + " bodies" +
                      (cosmos.running ? "  [running]" : "  [paused]"),
                  {stage.x + 14.0f * scale, stage.y + 12.0f * scale}, 13.0f * scale,
                  with_alpha(WL::TEXT_SECONDARY, 220));
        draw_cosmos_scale_hud(cosmos, stage, scale);
    } else {
        draw_text("Empty stage", {stage.x + 14.0f * scale, stage.y + 12.0f * scale},
                  13.0f * scale, with_alpha(WL::TEXT_SECONDARY, 200));
        const char* prompt = "Select a scale, then Spawn to drop this tier's objects into a live sandbox.";
        draw_text_block(prompt,
                        {cx - 220.0f * scale, cy - 20.0f * scale, 440.0f * scale, 60.0f * scale},
                        15.0f * scale, WL::TEXT_TERTIARY, 4.0f * scale);
    }
    DrawRectangleRoundedLines(stage, 0.02f, 12, 1.4f, with_alpha(WL::GLASS_BORDER, 150));

    // Saved-sandbox browser overlay (drawn last; the field pass above is skipped
    // while it is open, so nothing clips it).
    if (cosmos.browser_open) {
        draw_browser_modal(app, cosmos, viewport, scale);
    }
    if (cosmos.dossier_open) {
        draw_dossier_modal(cosmos, viewport, scale);
    }

    return result;
}
