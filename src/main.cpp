#include "raylib.h"
#include "app/AppRuntime.hpp"
#include "app/SeededUniverseRuntime.hpp"
#include "app/UniverseProject.hpp"
#include "app/WorldlineCopy.hpp"
#include "app/WorldlineStorage.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/FieldRenderer.hpp"
#include "ui/GuidedFirstUniverseScene.hpp"
#include "ui/SeedWorkspaceScene.hpp"
#include "ui/SettingsModal.hpp"
#include "ui/SimulationDock.hpp"
#include "ui/TraceScene.hpp"
#include "ui/UiPrimitives.hpp"
#include "ui/UniverseAtlasScene.hpp"

#include <algorithm>

int main() {
    PersistentAppSettings boot_settings = Storage::load_settings();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(boot_settings.window_width, boot_settings.window_height, Copy::kAppTitle);
    SetTargetFPS(60);
    init_ui_font();

    Renderer renderer(GetScreenWidth(), GetScreenHeight());
    FieldRenderer field(GetScreenWidth(), GetScreenHeight());
    AppState app;
    app.settings = boot_settings;
    app.catalog = Storage::load_catalog();
    apply_overlay_preset(app.visuals, OverlayPreset::FULL);
    sync_preview(app);
    app.ui.seeded.seed_input = app.settings.last_seed;
    app.ui.atlas.query = app.settings.atlas_query;
    if (app.settings.last_screen == "SeedWorkspace") app.ui.screen = AppScreen::SEEDED_WORKSPACE;
    else if (app.settings.last_screen == "UniverseAtlas") app.ui.screen = AppScreen::UNIVERSE_ATLAS;
    else if (app.settings.last_screen == "ReferenceLab") app.ui.screen = AppScreen::REFERENCE_LAB;
    else if (app.settings.last_screen == "Trace") app.ui.screen = AppScreen::TRACE;
    if (!app.settings.last_project_id.empty()) {
        UniverseProject restored_project;
        if (Storage::load_project(app.settings.last_project_id, restored_project)) {
            apply_universe_project(app.ui.seeded, restored_project);
        }
    }
    AppScreen trail_screen = app.ui.screen;
    bool default_trail_needs_upload = true;

    while (!WindowShouldClose()) {
        renderer.ensure_size(GetScreenWidth(), GetScreenHeight());
        field.ensure_size(GetScreenWidth(), GetScreenHeight());
        if (trail_screen != app.ui.screen) {
            renderer.reset_trail();
            default_trail_needs_upload = (app.ui.screen == AppScreen::REFERENCE_LAB);
            if (app.ui.seeded.runtime != nullptr) {
                app.ui.seeded.runtime->trail_needs_upload = (app.ui.screen == AppScreen::SEEDED_WORKSPACE);
            }
            trail_screen = app.ui.screen;
        }
        const int screen_width = GetScreenWidth();
        const int screen_height = GetScreenHeight();
        const float gutter = 22.0f;
        const Rectangle canvas = {
            gutter,
            gutter,
            screen_width - gutter * 2.0f,
            screen_height - gutter * 2.0f
        };

        if (app.ui.screen == AppScreen::REFERENCE_LAB) {
            handle_active_field_input(app);
        }
        if (app.ui.screen == AppScreen::REFERENCE_LAB && !app.ui.settings_open) {
            handle_shortcuts(app, renderer);
        }

        SimulationDockResult dock_result{};
        SettingsModalResult modal_result{};
        GuidedFirstUniverseSceneResult guided_result{};
        SeedWorkspaceSceneResult workspace_result{};
        UniverseAtlasSceneResult atlas_result{};
        TraceSceneResult trace_result{};
        bool navigate_back = false;

        PendulumLayout display_layout{};
        VectorOverlayConfig vector_overlay{};
        const Simulation::VisualDiagnostics* diagnostics = nullptr;
        CanvasOverlayView canvas_overlay{};

        if (app.ui.screen == AppScreen::REFERENCE_LAB) {
            const PendulumLayout layout = renderer.make_layout(
                canvas,
                app.visuals.show_vectors,
                app.simulation.rigid_connectors(),
                app.simulation.bob1_pos(),
                app.simulation.bob2_pos(),
                app.simulation.reach());
            if (!app.ui.settings_open) {
                handle_canvas_editing(app, renderer, layout);
            } else {
                app.ui.drag_handle = 0;
            }

            const int new_segments = step_live_simulation(app, GetFrameTime());
            display_layout = renderer.make_layout(
                canvas,
                app.visuals.show_vectors,
                app.simulation.rigid_connectors(),
                app.simulation.bob1_pos(),
                app.simulation.bob2_pos(),
                app.simulation.reach());
            vector_overlay = make_vector_overlay_config(app.visuals);
            const bool need_diagnostics = vector_overlay.enabled;
            diagnostics = need_diagnostics ? &app.simulation.diagnostics() : nullptr;
            const double dissipation_power = app.simulation.dissipation_power();
            canvas_overlay = make_canvas_overlay_view(
                app,
                diagnostics,
                dissipation_power);
            if (default_trail_needs_upload) {
                if (!app.trail.empty()) {
                    renderer.advance_trail(
                        app.trail,
                        static_cast<int>(app.trail.size()),
                        display_layout,
                        1);
                }
                default_trail_needs_upload = false;
            }
            if (app.mode == RunMode::RUNNING && new_segments > 0) {
                renderer.advance_trail(app.trail, new_segments, display_layout, trail_fade_alpha(app.visuals));
            }
        } else if (app.ui.screen == AppScreen::SEEDED_WORKSPACE) {
            if (!app.ui.seeded.result.ready && app.ui.seeded.result.error.empty()) {
                regenerate_seeded_universe(app.ui.seeded);
            }
            if (app.ui.seeded.runtime != nullptr && app.ui.seeded.runtime->ready()) {
                SeededUniverseRuntime& runtime = *app.ui.seeded.runtime;
                runtime.step(GetFrameTime());
                if (!field.configured_for(runtime.config_token)) {
                    field.configure(*runtime.law_spec, runtime.config_token, app.ui.seeded.result.seed);
                }
                const bool field_running =
                    runtime.mode == RunMode::RUNNING && !runtime.scrubbing;
                field.update(GetFrameTime(), field_running,
                             runtime.law_state.q, runtime.law_state.v);
            }
        } else {
            app.ui.drag_handle = 0;
        }

        BeginDrawing();
        draw_background(screen_width, screen_height);
        if (app.ui.screen == AppScreen::GUIDED_FIRST_UNIVERSE) {
            guided_result = draw_guided_first_universe_scene(app, canvas);
        } else if (app.ui.screen == AppScreen::SEEDED_WORKSPACE) {
            FieldReadout readout{};
            if (app.ui.seeded.runtime != nullptr && app.ui.seeded.runtime->ready()
                && field.configured_for(app.ui.seeded.runtime->config_token)) {
                const SeedWorkspaceLayout layout = seed_workspace_layout(canvas);
                field.draw(layout.stage);
                const FieldRenderer::Metrics& m = field.metrics();
                readout.valid = true;
                readout.exotic_index = m.exotic_index;
                readout.vorticity = m.vorticity;
                readout.flux = m.flux;
                readout.coherence = m.coherence;
                readout.handedness = m.handedness;
                readout.particles = field.particle_count();
                readout.cool = field.palette_cool();
                readout.hot = field.palette_hot();
                readout.accent = field.palette_accent();
            }
            workspace_result = draw_seed_workspace_scene(app, canvas, readout);
        } else if (app.ui.screen == AppScreen::UNIVERSE_ATLAS) {
            atlas_result = draw_universe_atlas_scene(app, canvas);
        } else if (app.ui.screen == AppScreen::TRACE) {
            trace_result = draw_trace_scene(app, canvas);
        } else {
            renderer.draw_scene(app.simulation,
                                display_layout,
                                vector_overlay,
                                diagnostics);
            draw_canvas_overlay(canvas_overlay, display_layout);
            draw_editor_handles(app, renderer, display_layout);
            if (draw_back_to_menu_button(canvas, canvas_overlay_scale(canvas))) {
                navigate_back = true;
            }
            if (app.ui.settings_open) {
                modal_result = draw_settings_modal(app, canvas);
            } else {
                dock_result = draw_simulation_dock(app, canvas);
            }
        }
        EndDrawing();

        PanelCommand command = PanelCommand::NONE;
        if (guided_result.open_workspace) {
            app.ui.screen = AppScreen::SEEDED_WORKSPACE;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (guided_result.open_atlas) {
            app.ui.screen = AppScreen::UNIVERSE_ATLAS;
            app.ui.settings_open = false;
        } else if (guided_result.open_reference) {
            app.ui.screen = AppScreen::REFERENCE_LAB;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (workspace_result.refresh_catalog || atlas_result.refresh_catalog) {
            app.catalog = Storage::load_catalog();
        } else if (app.ui.screen == AppScreen::SEEDED_WORKSPACE && workspace_result.open_trace) {
            app.ui.seeded_debug_return_screen = AppScreen::SEEDED_WORKSPACE;
            app.ui.screen = AppScreen::TRACE;
        } else if (app.ui.screen == AppScreen::SEEDED_WORKSPACE && workspace_result.open_atlas) {
            app.ui.screen = AppScreen::UNIVERSE_ATLAS;
            app.ui.settings_open = false;
        } else if (app.ui.screen == AppScreen::SEEDED_WORKSPACE && workspace_result.open_reference) {
            app.ui.screen = AppScreen::REFERENCE_LAB;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (app.ui.screen == AppScreen::SEEDED_WORKSPACE && workspace_result.back_requested) {
            app.ui.screen = AppScreen::GUIDED_FIRST_UNIVERSE;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (app.ui.screen == AppScreen::UNIVERSE_ATLAS && atlas_result.open_workspace) {
            app.ui.screen = AppScreen::SEEDED_WORKSPACE;
            app.ui.settings_open = false;
        } else if (app.ui.screen == AppScreen::UNIVERSE_ATLAS && atlas_result.open_reference) {
            app.ui.screen = AppScreen::REFERENCE_LAB;
            app.ui.settings_open = false;
        } else if (app.ui.screen == AppScreen::UNIVERSE_ATLAS && atlas_result.back_requested) {
            app.ui.screen = AppScreen::GUIDED_FIRST_UNIVERSE;
            app.ui.settings_open = false;
        } else if (app.ui.screen == AppScreen::TRACE && trace_result.open_workspace) {
            app.ui.screen = app.ui.seeded_debug_return_screen;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (app.ui.screen == AppScreen::TRACE && trace_result.back_requested) {
            app.ui.screen = app.ui.seeded_debug_return_screen;
            app.ui.settings_open = false;
        } else if (navigate_back) {
            app.ui.screen = AppScreen::GUIDED_FIRST_UNIVERSE;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (app.ui.screen == AppScreen::REFERENCE_LAB && app.ui.settings_open) {
            const Rectangle modal = make_settings_modal_rect(canvas);
            handle_control_panel_wheel_scroll(app, modal, modal_result.panel.max_scroll);
            if (modal_result.panel.toggled_section != PanelSection::COUNT) {
                const std::size_t index = static_cast<std::size_t>(modal_result.panel.toggled_section);
                app.ui.collapsed_sections[index] = !app.ui.collapsed_sections[index];
            }
            if (modal_result.close_requested) {
                commit_active_field(app);
                app.ui.active_slider = FieldId::NONE;
                app.ui.settings_open = false;
            }
            command = modal_result.panel.command;
        } else if (app.ui.screen == AppScreen::REFERENCE_LAB) {
            if (dock_result.open_settings) {
                prepare_toggle_interaction(app);
                app.ui.settings_open = true;
            }
            if (dock_result.open_seed_debug) {
                prepare_toggle_interaction(app);
                app.ui.seeded_debug_return_screen = AppScreen::REFERENCE_LAB;
                app.ui.screen = AppScreen::TRACE;
                app.ui.settings_open = false;
            }
            command = dock_result.command;
        }

        if (app.ui.screen != AppScreen::REFERENCE_LAB || command == PanelCommand::NONE) {
        } else if (command == PanelCommand::LAUNCH) {
            launch_simulation(app, renderer);
        } else if (command == PanelCommand::TOGGLE_PAUSE) {
            if (app.mode != RunMode::STOPPED) {
                app.mode = (app.mode == RunMode::PAUSED)
                    ? RunMode::RUNNING
                    : RunMode::PAUSED;
            }
        } else if (command == PanelCommand::STOP) {
            stop_simulation(app, renderer);
        } else if (command == PanelCommand::CLEAR_TRAIL) {
            clear_trail(app, renderer);
        }
    }

    app.settings.last_seed = app.ui.seeded.seed_input;
    app.settings.atlas_query = app.ui.atlas.query;
    app.settings.last_screen =
        (app.ui.screen == AppScreen::SEEDED_WORKSPACE) ? "SeedWorkspace" :
        (app.ui.screen == AppScreen::UNIVERSE_ATLAS) ? "UniverseAtlas" :
        (app.ui.screen == AppScreen::REFERENCE_LAB) ? "ReferenceLab" :
        (app.ui.screen == AppScreen::TRACE) ? "Trace" :
        "GuidedFirstUniverse";
    app.settings.window_width = GetScreenWidth();
    app.settings.window_height = GetScreenHeight();
    Storage::save_settings(app.settings);

    shutdown_ui_font();
    CloseWindow();
    return 0;
}
