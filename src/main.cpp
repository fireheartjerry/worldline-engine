#include "raylib.h"
#include "app/AppRuntime.hpp"
#include "renderer/Renderer.hpp"
#include "ui/MainMenuScreen.hpp"
#include "ui/SettingsModal.hpp"
#include "ui/SimulationDock.hpp"

#include <algorithm>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1480, 920, "Worldline Double Pendulum Lab");
    SetTargetFPS(60);
    init_ui_font();

    Renderer renderer(GetScreenWidth(), GetScreenHeight());
    AppState app;
    apply_overlay_preset(app.visuals, OverlayPreset::FULL);
    sync_preview(app);

    while (!WindowShouldClose()) {
        renderer.ensure_size(GetScreenWidth(), GetScreenHeight());
        const int screen_width = GetScreenWidth();
        const int screen_height = GetScreenHeight();
        const float gutter = 22.0f;
        const Rectangle canvas = {
            gutter,
            gutter,
            screen_width - gutter * 2.0f,
            screen_height - gutter * 2.0f
        };

        if (app.ui.screen == AppScreen::DEFAULT_UNIVERSE) {
            handle_active_field_input(app);
        }
        if (app.ui.screen == AppScreen::DEFAULT_UNIVERSE && !app.ui.settings_open) {
            handle_shortcuts(app, renderer);
        }

        SimulationDockResult dock_result{};
        SettingsModalResult modal_result{};
        MainMenuResult menu_result{};
        bool back_to_menu = false;

        PendulumLayout display_layout{};
        VectorOverlayConfig vector_overlay{};
        const Simulation::VisualDiagnostics* diagnostics = nullptr;
        CanvasOverlayView canvas_overlay{};

        if (app.ui.screen == AppScreen::DEFAULT_UNIVERSE) {
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
            if (app.mode == RunMode::RUNNING && new_segments > 0) {
                renderer.advance_trail(app.trail, new_segments, display_layout, trail_fade_alpha(app.visuals));
            }
        } else {
            app.ui.drag_handle = 0;
        }

        BeginDrawing();
        draw_background(screen_width, screen_height);
        if (app.ui.screen == AppScreen::MAIN_MENU) {
            menu_result = draw_main_menu_screen(canvas);
        } else if (app.ui.screen == AppScreen::SEEDED_UNIVERSE) {
            back_to_menu = draw_seeded_universe_screen(canvas);
        } else {
            renderer.draw_scene(app.simulation,
                                display_layout,
                                vector_overlay,
                                diagnostics);
            draw_canvas_overlay(canvas_overlay, display_layout);
            draw_editor_handles(app, renderer, display_layout);
            if (draw_back_to_menu_button(canvas, canvas_overlay_scale(canvas))) {
                back_to_menu = true;
            }
            if (app.ui.settings_open) {
                modal_result = draw_settings_modal(app, canvas);
            } else {
                dock_result = draw_simulation_dock(app, canvas);
            }
        }
        EndDrawing();

        PanelCommand command = PanelCommand::NONE;
        if (app.ui.screen == AppScreen::MAIN_MENU) {
            if (menu_result.target != AppScreen::MAIN_MENU) {
                app.ui.screen = menu_result.target;
                app.ui.settings_open = false;
                app.ui.panel_scroll = 0.0f;
                prepare_toggle_interaction(app);
            }
        } else if (back_to_menu) {
            app.ui.screen = AppScreen::MAIN_MENU;
            app.ui.settings_open = false;
            app.ui.panel_scroll = 0.0f;
            prepare_toggle_interaction(app);
        } else if (app.ui.screen == AppScreen::DEFAULT_UNIVERSE && app.ui.settings_open) {
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
        } else if (app.ui.screen == AppScreen::DEFAULT_UNIVERSE) {
            if (dock_result.open_settings) {
                prepare_toggle_interaction(app);
                app.ui.settings_open = true;
            }
            command = dock_result.command;
        }

        if (app.ui.screen != AppScreen::DEFAULT_UNIVERSE || command == PanelCommand::NONE) {
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

    shutdown_ui_font();
    CloseWindow();
    return 0;
}
