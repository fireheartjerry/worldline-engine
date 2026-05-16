#pragma once

#include "CanvasOverlayLayout.hpp"
#include "MainMenuScreen.hpp"
#include "UiPrimitives.hpp"

#include "../app/SeededUniverseRuntime.hpp"
#include "../app/AppTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

struct SeededUniverseLiveResult {
    bool back_requested = false;
    bool open_debug = false;
};

namespace SeededUniverseLiveUi {

inline std::string format_metric(double value, int precision = 2) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

inline std::string first_paragraph(const std::string& text) {
    const std::size_t split = text.find("\n\n");
    if (split == std::string::npos) {
        return text;
    }
    return text.substr(0, split);`
}ld

inline const char* mode_label(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return "LIVE";
    case RunMode::PAUSED: return "HOLD";
    case RunMode::STOPPED: return "IDLE";
    }
    return "IDLE";
}

inline Color mode_fill(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return with_alpha(WL::PLASMA_DIM, 210);
    case RunMode::PAUSED: return with_alpha(WL::XENON_DIM, 210);
    case RunMode::STOPPED: return Color{18, 44, 76, 210};
    }
    return Color{18, 44, 76, 210};
}

inline Color mode_text(RunMode mode) {
    switch (mode) {
    case RunMode::RUNNING: return WL::PLASMA_GREEN;
    case RunMode::PAUSED: return WL::XENON_CORE;
    case RunMode::STOPPED: return WL::ACCENT_GRAVITY;
    }
    return WL::TEXT_SECONDARY;
}

inline const char* law_mode_label(const MetaSpec& meta_spec) {
    return meta_spec.p_dynamic ? "Dynamic p" : "Seed-Locked p";
}

inline void handle_seed_input(SeededUniverseUiState& seeded,
                              Rectangle input_rect) {
    const Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        seeded.input_active = CheckCollisionPointRec(mouse, input_rect);
        seeded.input_select_all = seeded.input_active;
        seeded.backspace_repeat_timer = 0.0f;
    }

    if (!seeded.input_active) {
        return;
    }

    const bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl_down && IsKeyPressed(KEY_A)) {
        seeded.input_select_all = true;
    }

    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch <= 126 && seeded.seed_input.size() < 128u) {
            if (seeded.input_select_all) {
                seeded.seed_input.clear();
                seeded.input_select_all = false;
            }
            seeded.seed_input.push_back(static_cast<char>(ch));
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (seeded.input_select_all) {
            seeded.seed_input.clear();
            seeded.input_select_all = false;
        } else if (!seeded.seed_input.empty()) {
            seeded.seed_input.pop_back();
        }
        seeded.backspace_repeat_timer = 0.0f;
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        seeded.backspace_repeat_timer += GetFrameTime();
        const float repeat_delay = 0.42f;
        const float repeat_step = 0.035f;
        while (seeded.backspace_repeat_timer >= repeat_delay) {
            if (seeded.input_select_all) {
                seeded.seed_input.clear();
                seeded.input_select_all = false;
                seeded.backspace_repeat_timer = 0.0f;
                break;
            }
            if (!seeded.seed_input.empty()) {
                seeded.seed_input.pop_back();
            }
            seeded.backspace_repeat_timer -= repeat_step;
        }
    } else {
        seeded.backspace_repeat_timer = 0.0f;
    }

    if (ctrl_down && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (seeded.input_select_all) {
            seeded.seed_input.clear();
            seeded.input_select_all = false;
        }
        while (clipboard != nullptr && *clipboard != '\0' && seeded.seed_input.size() < 128u) {
            const unsigned char value = static_cast<unsigned char>(*clipboard);
            if (value >= 32u && value <= 126u) {
                seeded.seed_input.push_back(static_cast<char>(value));
            }
            ++clipboard;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        seeded.input_active = false;
        seeded.input_select_all = false;
    }
}

inline void draw_metric_card(Rectangle rect,
                             const char* label,
                             const std::string& value,
                             Color accent,
                             float scale) {
    DrawRectangleRounded(rect, 0.10f, 8, {8, 18, 30, 236});
    DrawRectangleRoundedLines(rect, 0.10f, 8, 1.0f, with_alpha(accent, 75));
    draw_text(label,
              {rect.x + 10.0f * scale, rect.y + 8.0f * scale},
              11.0f * scale,
              with_alpha(accent, 170));
    draw_text(value,
              {rect.x + 10.0f * scale, rect.y + 24.0f * scale},
              19.0f * scale,
              WL::TEXT_PRIMARY);
}

inline void draw_placeholder_stage(Rectangle rect,
                                   float scale,
                                   const std::string& message,
                                   Color accent) {
    DrawRectangleRounded(rect, 0.028f, 20, {7, 14, 21, 230});
    DrawRectangleRoundedLines(rect, 0.028f, 20, 2.0f, {70, 104, 120, 180});
    DrawCircleGradient(static_cast<int>(rect.x + rect.width * 0.5f),
                       static_cast<int>(rect.y + rect.height * 0.5f),
                       std::min(rect.width, rect.height) * 0.32f,
                       with_alpha(accent, 42),
                       {0, 0, 0, 0});
    draw_text("SEED FIELD",
              {rect.x + 22.0f * scale, rect.y + 18.0f * scale},
              13.0f * scale,
              with_alpha(accent, 180));
    draw_text_block(message,
                    {rect.x + 22.0f * scale,
                     rect.y + rect.height * 0.5f - 34.0f * scale,
                     rect.width - 44.0f * scale,
                     90.0f * scale},
                    20.0f * scale,
                    WL::TEXT_SECONDARY,
                    4.0f * scale);
}

inline void draw_header(Rectangle rect,
                        SeededUniverseUiState& seeded,
                        float scale,
                        SeededUniverseLiveResult& result) {
    draw_card(rect, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 120));
    DrawRectangleGradientEx(rect,
                            {16, 60, 78, 58},
                            {10, 18, 30, 10},
                            {0, 0, 0, 0},
                            {46, 18, 84, 38});

    const float pad = 16.0f * scale;
    draw_text("SEEDED UNIVERSE",
              {rect.x + pad, rect.y + 14.0f * scale},
              13.0f * scale,
              with_alpha(WL::CYAN_CORE, 180));
    draw_text("LawSpec now drives the motion. The pendulum renderer is only the lens.",
              {rect.x + pad, rect.y + 30.0f * scale},
              16.0f * scale,
              WL::TEXT_SECONDARY);

    const float input_y = rect.y + 58.0f * scale;
    const float input_h = 38.0f * scale;
    const float button_w = 112.0f * scale;
    const float inspect_w = 122.0f * scale;
    const float gap = 10.0f * scale;
    const float input_w = rect.width - pad * 2.0f - button_w - inspect_w - gap * 2.0f;
    const Rectangle input_rect = {rect.x + pad, input_y, input_w, input_h};
    handle_seed_input(seeded, input_rect);

    const Vector2 mouse = GetMousePosition();
    const bool input_hot = CheckCollisionPointRec(mouse, input_rect);
    const Color outline = seeded.input_active
        ? WL::CYAN_CORE
        : input_hot ? with_alpha(WL::CYAN_DIM, 170) : with_alpha(WL::GLASS_BORDER, 120);
    DrawRectangleRounded(input_rect, 0.10f, 8, {8, 16, 28, 240});
    DrawRectangleRoundedLines(input_rect, 0.10f, 8, 1.15f, outline);
    draw_text(seeded.seed_input.empty() ? std::string("Type a seed and press Enter...") : seeded.seed_input,
              {input_rect.x + 10.0f * scale, input_rect.y + 9.0f * scale},
              17.0f * scale,
              seeded.seed_input.empty() ? WL::TEXT_INACTIVE : WL::TEXT_PRIMARY);

    if (draw_button({input_rect.x + input_rect.width + gap, input_y, button_w, input_h},
                    "Generate",
                    {10, 84, 98, 240},
                    {18, 126, 140, 255},
                    WL::CYAN_CORE,
                    true,
                    scale)
        || (seeded.input_active && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))) {
        regenerate_seeded_universe(seeded);
    }

    if (draw_button({input_rect.x + input_rect.width + gap + button_w + gap, input_y, inspect_w, input_h},
                    "Inspect Build",
                    {26, 42, 72, 236},
                    {38, 62, 102, 255},
                    WL::TEXT_PRIMARY,
                    seeded.result.ready,
                    scale)) {
        result.open_debug = true;
    }

    static constexpr const char* kPresetSeeds[] = {
        "worldline", "andromeda", "tachyon", "amber-signal", "quiet singularity"
    };
    const float preset_y = input_y + input_h + 10.0f * scale;
    draw_text("Presets",
              {rect.x + pad, preset_y + 4.0f * scale},
              10.0f * scale,
              with_alpha(WL::TEXT_TERTIARY, 170));

    float chip_x = rect.x + pad + 54.0f * scale;
    for (const char* preset : kPresetSeeds) {
        const Vector2 text_size = measure_ui_text(preset, 10.0f * scale);
        const float chip_w = text_size.x + 18.0f * scale;
        const Rectangle chip = {chip_x, preset_y, chip_w, 22.0f * scale};
        const bool active = seeded.seed_input == preset;
        if (draw_button(chip,
                        preset,
                        active ? Color{18, 76, 90, 236} : Color{10, 20, 34, 228},
                        Color{18, 96, 112, 255},
                        active ? WL::CYAN_CORE : WL::TEXT_SECONDARY,
                        true,
                        scale * 0.66f)) {
            seeded.seed_input = preset;
            regenerate_seeded_universe(seeded);
        }
        chip_x += chip_w + 6.0f * scale;
    }

    if (!seeded.result.ready) {
        return;
    }

    const float metric_y = preset_y + 34.0f * scale;
    const float metric_gap = 8.0f * scale;
    const float metric_w = (rect.width - pad * 2.0f - metric_gap * 2.0f) / 3.0f;
    draw_metric_card({rect.x + pad, metric_y, metric_w, 54.0f * scale},
                     "Law Mode",
                     law_mode_label(seeded.result.meta_spec),
                     WL::CYAN_CORE,
                     scale);
    draw_metric_card({rect.x + pad + metric_w + metric_gap, metric_y, metric_w, 54.0f * scale},
                     "Gain",
                     format_metric(seeded.result.law_preview.linear_gain, 2),
                     WL::VIOLET_CORE,
                     scale);
    draw_metric_card({rect.x + pad + (metric_w + metric_gap) * 2.0f, metric_y, metric_w, 54.0f * scale},
                     "Accel Ceiling",
                     format_metric(seeded.result.law_preview.accel_ceiling, 2),
                     WL::PLASMA_GREEN,
                     scale);
}

inline void draw_status_dock(Rectangle rect,
                             SeededUniverseUiState& seeded,
                             float scale,
                             SeededUniverseLiveResult& result) {
    draw_card(rect, WL::GLASS_1, with_alpha(WL::CYAN_DIM, 95));
    DrawLineEx({rect.x + 4, rect.y + 1},
               {rect.x + rect.width - 4, rect.y + 1},
               1.5f,
               with_alpha(WL::CYAN_CORE, 60));

    const float pad = 14.0f * scale;
    SeededUniverseRuntime* runtime = seeded.runtime.get();
    const RunMode mode = (runtime != nullptr) ? runtime->mode : RunMode::STOPPED;

    draw_text("SEED STATUS",
              {rect.x + pad, rect.y + 12.0f * scale},
              12.0f * scale,
              with_alpha(WL::CYAN_CORE, 170));
    draw_badge({rect.x + pad, rect.y + 31.0f * scale, 74.0f * scale, 22.0f * scale},
               mode_label(mode),
               mode_fill(mode),
               mode_text(mode),
               scale);

    const char* law_mode = seeded.result.ready
        ? law_mode_label(seeded.result.meta_spec)
        : "Awaiting Seed";
    draw_badge({rect.x + rect.width - 132.0f * scale,
                rect.y + 31.0f * scale,
                118.0f * scale,
                22.0f * scale},
               law_mode,
               {22, 42, 62, 200},
               WL::TEXT_SECONDARY,
               scale);

    const float metric_y = rect.y + 64.0f * scale;
    const float metric_w = (rect.width - pad * 2.0f - 8.0f * scale) * 0.5f;
    const float metric_h = 48.0f * scale;

    const std::string p_value = (runtime != nullptr && runtime->ready())
        ? format_metric(runtime->law_state.p, 3)
        : "--";
    const std::string q_value = (runtime != nullptr && runtime->ready())
        ? format_metric(runtime->law_state.q.length(), 2)
        : "--";
    const std::string v_value = (runtime != nullptr && runtime->ready())
        ? format_metric(runtime->law_state.v.length(), 2)
        : "--";
    const std::string t_value = (runtime != nullptr && runtime->ready())
        ? format_metric(runtime->elapsed_time, 2) + " s"
        : "--";

    draw_metric_card({rect.x + pad, metric_y, metric_w, metric_h},
                     "Observed p",
                     p_value,
                     WL::CYAN_CORE,
                     scale);
    draw_metric_card({rect.x + pad + metric_w + 8.0f * scale, metric_y, metric_w, metric_h},
                     "|q|",
                     q_value,
                     WL::VIOLET_CORE,
                     scale);
    draw_metric_card({rect.x + pad, metric_y + metric_h + 8.0f * scale, metric_w, metric_h},
                     "|v|",
                     v_value,
                     WL::XENON_CORE,
                     scale);
    draw_metric_card({rect.x + pad + metric_w + 8.0f * scale, metric_y + metric_h + 8.0f * scale, metric_w, metric_h},
                     "Runtime",
                     t_value,
                     WL::PLASMA_GREEN,
                     scale);

    const float button_y = metric_y + (metric_h + 8.0f * scale) * 2.0f + 8.0f * scale;
    const float gap = 8.0f * scale;
    const float button_w = (rect.width - pad * 2.0f - gap) * 0.5f;
    const bool enabled = runtime != nullptr && runtime->ready();

    if (draw_button({rect.x + pad, button_y, button_w, 30.0f * scale},
                    "Restart",
                    with_alpha(WL::PLASMA_DIM, 228),
                    with_alpha(WL::PLASMA_GREEN, 80),
                    WL::PLASMA_GREEN,
                    enabled,
                    scale)) {
        runtime->restart();
    }
    if (draw_button({rect.x + pad + button_w + gap, button_y, button_w, 30.0f * scale},
                    mode == RunMode::PAUSED ? "Resume" : "Pause",
                    {26, 48, 74, 232},
                    {36, 64, 96, 255},
                    WL::TEXT_PRIMARY,
                    enabled,
                    scale)) {
        runtime->mode = (mode == RunMode::PAUSED) ? RunMode::RUNNING : RunMode::PAUSED;
    }

    const float button_y2 = button_y + 36.0f * scale;
    if (draw_button({rect.x + pad, button_y2, button_w, 30.0f * scale},
                    "Clear Trail",
                    {14, 30, 50, 228},
                    {22, 48, 76, 255},
                    WL::TEXT_SECONDARY,
                    enabled,
                    scale)) {
        runtime->clear_trail();
    }
    if (draw_button({rect.x + pad + button_w + gap, button_y2, button_w, 30.0f * scale},
                    "Inspect Build",
                    with_alpha(WL::VIOLET_DIM, 220),
                    with_alpha(WL::VIOLET_CORE, 80),
                    WL::VIOLET_CORE,
                    seeded.result.ready,
                    scale)) {
        result.open_debug = true;
    }
}

inline void draw_footer(Rectangle rect,
                        const SeededUniverseUiState& seeded,
                        float scale) {
    draw_card(rect, WL::GLASS_1, with_alpha(WL::GLASS_BORDER, 90));
    const float pad = 16.0f * scale;

    draw_text("UNIVERSE READOUT",
              {rect.x + pad, rect.y + 12.0f * scale},
              11.0f * scale,
              with_alpha(WL::CYAN_CORE, 165));

    const std::string summary = seeded.result.ready
        ? first_paragraph(seeded.result.descriptor)
        : "Enter a seed to assemble a universe. Generation rebuilds the MetaSpec, LawSpec, observable mapping, and the live simulation state from scratch.";
    draw_text_block(summary,
                    {rect.x + pad,
                     rect.y + 28.0f * scale,
                     rect.width - pad * 2.0f,
                     rect.height - 38.0f * scale},
                    14.0f * scale,
                    WL::TEXT_SECONDARY,
                    3.0f * scale);
}

inline SeededUniverseLiveResult draw_seeded_universe_live_screen(AppState& app,
                                                                 Rectangle viewport) {
    SeededUniverseLiveResult result;
    SeededUniverseUiState& seeded = app.ui.seeded;
    if (!seeded.result.ready && seeded.result.error.empty()) {
        regenerate_seeded_universe(seeded);
    }

    SeededUniverseRuntime* runtime = seeded.runtime.get();
    if (!seeded.input_active && runtime != nullptr && runtime->ready()) {
        if (IsKeyPressed(KEY_SPACE)) {
            runtime->mode = (runtime->mode == RunMode::PAUSED)
                ? RunMode::RUNNING
                : RunMode::PAUSED;
        }
        if (IsKeyPressed(KEY_R)) {
            runtime->restart();
        }
        if (IsKeyPressed(KEY_C)) {
            runtime->clear_trail();
        }
        if (IsKeyPressed(KEY_TAB)) {
            result.open_debug = true;
        }
    }

    const float scale = canvas_overlay_scale(viewport);
    const CanvasOverlayRects hud = make_canvas_overlay_layout(viewport, false, true);
    const float margin = 16.0f * scale;
    const float gap = 20.0f * scale;
    const Rectangle header = {
        viewport.x + margin,
        viewport.y + margin,
        hud.sim_dock.x - (viewport.x + margin) - gap,
        hud.sim_dock.height
    };

    if (draw_back_to_menu_button(viewport, scale)
        || (!seeded.input_active && IsKeyPressed(KEY_ESCAPE))) {
        result.back_requested = true;
    }

    draw_header(header, seeded, scale, result);
    draw_status_dock(hud.sim_dock, seeded, scale, result);
    draw_footer(hud.hint_bar, seeded, scale);

    if (!seeded.result.ready && !seeded.result.error.empty()) {
        draw_placeholder_stage(hud.stage_rect,
                               scale,
                               std::string("Universe generation failed.\n") + seeded.result.error,
                               WL::XENON_CORE);
    }

    return result;
}

} // namespace SeededUniverseLiveUi

using SeededUniverseLiveUi::draw_seeded_universe_live_screen;
