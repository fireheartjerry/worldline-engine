#pragma once

#include "MainMenuScreen.hpp"
#include "UiPrimitives.hpp"

#include "../app/AppTypes.hpp"
#include "../physics/LawSpec.hpp"
#include "../seed/MetaSpec.hpp"
#include "../seed/SeedDebug.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace SeededUniverseUi {

// ── Utility ──────────────────────────────────────────────────────────────────

struct PanelHeaderResult {
    bool info_clicked = false;
};

inline std::string format_number(double value, int precision = 3) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

inline constexpr std::array<float, 5> kStageThresholds{{
    0.0f, 0.12f, 1.25f, 2.0f, 2.5f
}};

inline void set_focus(SeededUniverseUiState& seeded,
                      SeededFocusKind kind,
                      int index) {
    seeded.focus_kind = kind;
    seeded.focus_index = std::max(0, index);
}

inline void open_info_modal(SeededUniverseUiState& seeded,
                            SeededInfoTopic topic) {
    seeded.info_topic = topic;
    seeded.info_ignore_mouse_until_release = true;
    seeded.info_modal_scroll = 0.0f;
}

inline int clamped_focus_index(const SeededUniverseUiState& seeded,
                               int count) {
    if (count <= 0) return 0;
    return std::clamp(seeded.focus_index, 0, count - 1);
}

inline float clamp_playback(float time_value) {
    return std::clamp(time_value, 0.0f, 8.0f);
}

inline SeededLawPreview build_law_preview(const MetaSpec& meta_spec) {
    SeededLawPreview preview;
    LawSpec law(meta_spec);
    LawState state = law.initial_state();
    constexpr int kSamples = 192;
    constexpr double kDt = 0.02;

    preview.phase_path.reserve(kSamples + 1);
    preview.p_samples.reserve(kSamples + 1);
    preview.phase_path.push_back(state.q);
    preview.p_samples.push_back(state.p);
    preview.linear_gain = law.potential_linear_gain();
    preview.accel_ceiling = law.acceleration_ceiling();
    preview.p_min = state.p;
    preview.p_max = state.p;

    double speed_sum = state.v.length();
    double radius_sum = state.q.length();
    double handed_sum = 0.0;
    preview.radius_peak = state.q.length();
    preview.max_accel = law.acceleration(state).length();

    for (int i = 0; i < kSamples; ++i) {
        const double angular = state.q.x * state.v.y - state.q.y * state.v.x;
        handed_sum += angular;

        state = law.step(state, kDt);
        preview.phase_path.push_back(state.q);
        preview.p_samples.push_back(state.p);
        preview.p_min = std::min(preview.p_min, state.p);
        preview.p_max = std::max(preview.p_max, state.p);

        const double speed = state.v.length();
        const double radius = state.q.length();
        speed_sum += speed;
        radius_sum += radius;
        preview.radius_peak = std::max(preview.radius_peak, radius);
        preview.max_accel = std::max(preview.max_accel, law.acceleration(state).length());
    }

    const double sample_count = static_cast<double>(preview.phase_path.size());
    preview.mean_speed = speed_sum / sample_count;
    preview.radius_mean = radius_sum / sample_count;
    preview.handedness = handed_sum / (static_cast<double>(kSamples) + std::abs(handed_sum) + 1.0e-9);
    return preview;
}

inline std::string law_mode_label(const MetaSpec& meta_spec) {
    return meta_spec.p_dynamic ? "dynamic p" : "seed-locked p";
}

inline std::string law_readout(const MetaSpec& meta_spec,
                               const SeededLawPreview& preview) {
    std::string out = "LAW WEAVE\n";
    out += "LawSpec turns the tensor vault into a bounded phase flow. ";
    out += meta_spec.p_dynamic
        ? "Here the exponent is allowed to breathe with angular momentum before it is pulled back toward its seed. "
        : "Here the exponent remains pinned to its seeded value, so the flow is structurally stable rather than self-adjusting. ";

    out += "The construction-time potential gain is ";
    out += format_number(preview.linear_gain, 2);
    out += ", the observed acceleration peak in the preview is ";
    out += format_number(preview.max_accel, 2);
    out += " against a ceiling of ";
    out += format_number(preview.accel_ceiling, 2);
    out += ". ";

    out += "The symmetry split is carried explicitly: additive ";
    out += format_number(meta_spec.s_a, 2);
    out += ", filter ";
    out += format_number(meta_spec.s_b, 2);
    out += ", torque ";
    out += format_number(meta_spec.s_c, 2);
    out += ". ";

    out += "Across the preview, the orbit radius averages ";
    out += format_number(preview.radius_mean, 2);
    out += " and the handedness bias is ";
    out += format_number(preview.handedness, 2);
    out += ".";
    return out;
}

inline void run_generation(SeededUniverseUiState& state) {
    SeededUniverseResult result;
    result.seed = state.seed_input;

    try {
        result.expansion_trace = expand_with_trace(result.seed);
        result.machine_trace = compress_with_trace(result.expansion_trace.expanded);
        result.lanes.assign(result.machine_trace.output.begin(), result.machine_trace.output.end());
        result.meta_spec = generate_meta_spec(result.lanes);
        result.descriptor = generate_descriptor(result.meta_spec);
        result.law_preview = build_law_preview(result.meta_spec);
        result.descriptor += "\n\n";
        result.descriptor += law_readout(result.meta_spec, result.law_preview);
        result.ready = true;
    } catch (const std::exception& ex) {
        result.error = ex.what();
    }

    state.result = std::move(result);
    state.playback_time = 0.0f;
    state.body_scroll = 0.0f;
    state.descriptor_scroll = 0.0f;
    state.info_modal_scroll = 0.0f;
    state.scrub_active = false;
    state.focus_kind = SeededFocusKind::LANE;
    state.focus_index = 0;
}

inline float matrix_peak(const double matrix[2][2]) {
    return static_cast<float>(std::max({
        std::abs(matrix[0][0]),
        std::abs(matrix[0][1]),
        std::abs(matrix[1][0]),
        std::abs(matrix[1][1]),
        1.0e-6
    }));
}

inline Color signed_heat_color(double value, float peak, unsigned char alpha = 220) {
    const float normalized = std::clamp(static_cast<float>(value) / peak, -1.0f, 1.0f);
    if (normalized >= 0.0f) {
        const float t = normalized;
        return {
            static_cast<unsigned char>(12 + 52 * (1.0f - t) + WL::CYAN_CORE.r * t),
            static_cast<unsigned char>(24 + 30 * (1.0f - t) + WL::CYAN_CORE.g * t),
            static_cast<unsigned char>(34 + 50 * (1.0f - t) + WL::PLASMA_GREEN.b * t),
            alpha
        };
    }
    const float t = -normalized;
    return {
        static_cast<unsigned char>(18 + 26 * (1.0f - t) + WL::XENON_CORE.r * t),
        static_cast<unsigned char>(18 + 20 * (1.0f - t) + WL::VIOLET_CORE.g * t),
        static_cast<unsigned char>(30 + 44 * (1.0f - t) + WL::VIOLET_CORE.b * t),
        alpha
    };
}

inline std::vector<float> bucketize_cells(const std::vector<std::uint8_t>& cells,
                                          std::size_t bucket_count) {
    std::vector<float> buckets(bucket_count, 0.0f);
    if (cells.empty() || bucket_count == 0u) return buckets;
    const std::size_t stride = std::max<std::size_t>(1u, cells.size() / bucket_count);
    for (std::size_t bucket = 0; bucket < bucket_count; ++bucket) {
        const std::size_t begin = bucket * stride;
        const std::size_t end = std::min(cells.size(), begin + stride);
        float sum = 0.0f;
        for (std::size_t index = begin; index < end; ++index) {
            sum += static_cast<float>(cells[index]) / 255.0f;
        }
        buckets[bucket] = sum / static_cast<float>(std::max<std::size_t>(1u, end - begin));
    }
    return buckets;
}

inline std::array<double, 6> stage_strengths(const MetaSpec& ms) {
    const auto frob = [](const double matrix[2][2]) {
        return std::sqrt(
            matrix[0][0] * matrix[0][0] +
            matrix[0][1] * matrix[0][1] +
            matrix[1][0] * matrix[1][0] +
            matrix[1][1] * matrix[1][1]);
    };
    return {{
        frob(ms.g),
        frob(ms.V),
        frob(ms.S),
        frob(ms.C[0]) + frob(ms.C[1]),
        std::abs(ms.T[0][1]) + std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]),
        std::sqrt(ms.q0[0] * ms.q0[0] + ms.q0[1] * ms.q0[1])
            + std::sqrt(ms.qdot0[0] * ms.qdot0[0] + ms.qdot0[1] * ms.qdot0[1])
    }};
}

inline std::size_t visible_count(float playback_time,
                                 float start_time,
                                 float step_time,
                                 std::size_t total_count,
                                 bool reveal_all) {
    if (reveal_all) return total_count;
    if (playback_time < start_time) return 0u;
    return std::min(total_count,
                    static_cast<std::size_t>(1.0f + std::floor((playback_time - start_time) / std::max(0.001f, step_time))));
}

inline float average_bytes(const std::vector<std::uint8_t>& cells) {
    if (cells.empty()) return 0.0f;
    double sum = 0.0;
    for (std::uint8_t value : cells) sum += static_cast<double>(value);
    return static_cast<float>(sum / (255.0 * static_cast<double>(cells.size())));
}

inline float max_abs_register(const std::array<double, SEED_OUTPUT_SIZE>& values) {
    double peak = 0.0;
    for (double value : values) peak = std::max(peak, std::abs(value));
    return static_cast<float>(std::max(1.0, peak));
}

// Pulse alpha for newly revealed items (fades from bright to normal over 0.4s)
inline unsigned char reveal_alpha(float playback_time, float item_reveal_time, unsigned char base = 200) {
    const float age = playback_time - item_reveal_time;
    if (age < 0.0f) return 0;
    if (age > 0.4f) return base;
    const float pulse = 1.0f - (age / 0.4f);
    return static_cast<unsigned char>(std::min(255.0f, base + 55.0f * pulse));
}

// ── Shared Drawing Primitives ────────────────────────────────────────────────

inline void draw_separator_h(float x, float y, float w, Color color) {
    DrawLineEx({x, y}, {x + w, y}, 1.0f, color);
}

inline void draw_separator_v(float x, float y, float h, Color color) {
    DrawLineEx({x, y}, {x, y + h}, 1.0f, color);
}

inline void draw_corner_brackets(Rectangle rect, float arm, float thickness, Color color) {
    // Top-left
    DrawLineEx({rect.x, rect.y}, {rect.x + arm, rect.y}, thickness, color);
    DrawLineEx({rect.x, rect.y}, {rect.x, rect.y + arm}, thickness, color);
    // Top-right
    DrawLineEx({rect.x + rect.width, rect.y}, {rect.x + rect.width - arm, rect.y}, thickness, color);
    DrawLineEx({rect.x + rect.width, rect.y}, {rect.x + rect.width, rect.y + arm}, thickness, color);
    // Bottom-left
    DrawLineEx({rect.x, rect.y + rect.height}, {rect.x + arm, rect.y + rect.height}, thickness, color);
    DrawLineEx({rect.x, rect.y + rect.height}, {rect.x, rect.y + rect.height - arm}, thickness, color);
    // Bottom-right
    DrawLineEx({rect.x + rect.width, rect.y + rect.height}, {rect.x + rect.width - arm, rect.y + rect.height}, thickness, color);
    DrawLineEx({rect.x + rect.width, rect.y + rect.height}, {rect.x + rect.width, rect.y + rect.height - arm}, thickness, color);
}

inline bool draw_info_button(Rectangle rect, float scale, Color accent) {
    const bool hot = CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRectangleRounded(rect, 0.35f, 8, hot ? Color{16, 34, 58, 244} : Color{8, 18, 32, 230});
    DrawRectangleRoundedLines(rect, 0.35f, 8, 1.0f, with_alpha(accent, hot ? 180 : 100));
    if (hot) {
        DrawRectangleRounded({rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4},
                             0.35f, 8, with_alpha(accent, 20));
    }
    draw_text("i",
              {rect.x + rect.width * 0.5f - 3.0f * scale, rect.y + 4.0f * scale},
              14.0f * scale,
              hot ? WL::TEXT_PRIMARY : with_alpha(accent, 210));
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

inline PanelHeaderResult draw_panel_header(Rectangle rect,
                                           const char* eyebrow,
                                           const char* title,
                                           const char* subtitle,
                                           float scale,
                                           Color accent) {
    PanelHeaderResult result;
    draw_text(eyebrow, {rect.x + 14.0f * scale, rect.y + 10.0f * scale}, 12.0f * scale, with_alpha(accent, 180));
    draw_text(title, {rect.x + 14.0f * scale, rect.y + 24.0f * scale}, 24.0f * scale, WL::TEXT_PRIMARY);
    draw_text_block(subtitle,
                    {rect.x + 14.0f * scale, rect.y + 52.0f * scale, rect.width - 60.0f * scale, 38.0f * scale},
                    13.0f * scale,
                    WL::TEXT_SECONDARY,
                    3.0f * scale);
    result.info_clicked = draw_info_button(
        {rect.x + rect.width - 30.0f * scale, rect.y + 10.0f * scale, 20.0f * scale, 20.0f * scale},
        scale,
        accent);
    return result;
}

inline void draw_scrollbar(Rectangle viewport, float scroll, float max_scroll) {
    if (max_scroll <= 0.0f) return;
    const float track_w = 5.0f;
    const float track_x = viewport.x + viewport.width - track_w - 3.0f;
    const float track_y = viewport.y + 3.0f;
    const float track_h = viewport.height - 6.0f;
    const float thumb_h = std::max(28.0f, track_h * (viewport.height / (viewport.height + max_scroll)));
    const float thumb_y = track_y + (track_h - thumb_h) * (scroll / max_scroll);
    DrawRectangleRounded({track_x, track_y, track_w, track_h}, 0.5f, 8, {255, 255, 255, 8});
    DrawRectangleRounded({track_x, thumb_y, track_w, thumb_h}, 0.5f, 8, {64, 208, 224, 100});
}

// ── Hover Tooltip ────────────────────────────────────────────────────────────

inline void draw_tooltip(const std::string& text, Vector2 pos, float scale) {
    const float font_size = 12.0f * scale;
    const Vector2 text_size = measure_ui_text(text, font_size);
    const float pad_x = 8.0f * scale;
    const float pad_y = 5.0f * scale;
    const Rectangle bg = {
        pos.x - pad_x,
        pos.y - text_size.y - pad_y * 2.0f - 4.0f * scale,
        text_size.x + pad_x * 2.0f,
        text_size.y + pad_y * 2.0f
    };
    DrawRectangleRounded(bg, 0.12f, 8, {4, 12, 22, 245});
    DrawRectangleRoundedLines(bg, 0.12f, 8, 1.0f, with_alpha(WL::CYAN_CORE, 130));
    draw_text(text, {bg.x + pad_x, bg.y + pad_y}, font_size, WL::TEXT_PRIMARY);
}

// ── Snapshot Strip ───────────────────────────────────────────────────────────

inline void draw_snapshot_strip(Rectangle rect,
                                const CellularCheckpoint& checkpoint,
                                float intensity,
                                float scale) {
    DrawRectangleRounded(rect, 0.06f, 6, {6, 14, 24, 204});
    DrawRectangleRoundedLines(rect, 0.06f, 6, 1.0f, with_alpha(WL::CYAN_DIM, 50));
    const std::size_t bucket_count = std::max<std::size_t>(16u, static_cast<std::size_t>(rect.width / (3.0f * scale)));
    const std::vector<float> buckets = bucketize_cells(checkpoint.cells, bucket_count);
    const float slot_w = rect.width / static_cast<float>(buckets.size());
    for (std::size_t index = 0; index < buckets.size(); ++index) {
        const float value = buckets[index];
        const float height = std::max(2.0f * scale, (rect.height - 6.0f * scale) * value);
        const Rectangle bar = {
            rect.x + index * slot_w + 0.5f,
            rect.y + rect.height - 3.0f * scale - height,
            std::max(1.0f, slot_w - 1.0f),
            height
        };
        const Color fill = {
            static_cast<unsigned char>(28 + 60 * (1.0f - value) + 40 * intensity),
            static_cast<unsigned char>(70 + 160 * value),
            static_cast<unsigned char>(88 + 120 * value),
            static_cast<unsigned char>(60 + 170 * intensity)
        };
        DrawRectangleRec(bar, fill);
    }
}

// ── Top Strip: Seed Input + Pipeline + Stats ─────────────────────────────────

inline void draw_top_strip(Rectangle rect,
                           SeededUniverseUiState& seeded,
                           float scale) {
    // Background
    draw_card(rect, {5, 12, 22, 238}, with_alpha(WL::CYAN_DIM, 80));
    DrawRectangleGradientEx(rect,
                            {14, 56, 78, 60},
                            {8, 18, 30, 16},
                            {5, 12, 22, 8},
                            {44, 16, 76, 40});

    const float pad = 14.0f * scale;
    const float inner_w = rect.width - pad * 2.0f;

    // ── Row 1: Eyebrow + seed input + buttons ────────────────────────────────
    const float row1_y = rect.y + pad;
    draw_text("WORLDLINE", {rect.x + pad, row1_y}, 11.0f * scale, with_alpha(WL::CYAN_CORE, 160));
    DrawLineEx({rect.x + pad + 82.0f * scale, row1_y + 5.0f * scale},
               {rect.x + pad + 82.0f * scale + 28.0f * scale, row1_y + 5.0f * scale},
               1.0f, with_alpha(WL::CYAN_DIM, 60));
    draw_text("SEED DRIVE", {rect.x + pad + 116.0f * scale, row1_y}, 11.0f * scale, with_alpha(WL::CYAN_DIM, 120));

    // Input field
    const float input_y = row1_y + 16.0f * scale;
    const float input_h = 34.0f * scale;
    const float btn_w = 110.0f * scale;
    const float replay_w = 72.0f * scale;
    const float debug_w = 140.0f * scale;
    const float input_w = inner_w - btn_w - replay_w - debug_w - 12.0f * scale * 3.0f;
    const Rectangle input_rect = {rect.x + pad, input_y, input_w, input_h};

    const Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        seeded.input_active = CheckCollisionPointRec(mouse, input_rect);
        if (seeded.input_active) {
            seeded.input_select_all = true;
            seeded.backspace_repeat_timer = 0.0f;
        } else {
            seeded.input_select_all = false;
            seeded.backspace_repeat_timer = 0.0f;
        }
    }

    if (seeded.input_active) {
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
        if (IsKeyPressed(KEY_ESCAPE)) {
            seeded.input_active = false;
            seeded.input_select_all = false;
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
    }

    const bool input_hot = CheckCollisionPointRec(mouse, input_rect);
    const Color outline = seeded.input_active
        ? WL::CYAN_CORE
        : input_hot ? with_alpha(WL::CYAN_DIM, 170) : with_alpha(WL::GLASS_BORDER, 120);
    DrawRectangleRounded(input_rect, 0.10f, 8, {8, 16, 28, 240});
    DrawRectangleRoundedLines(input_rect, 0.10f, 8, 1.15f, outline);
    if (seeded.input_active) {
        DrawRectangleRounded({input_rect.x - 2, input_rect.y - 2, input_rect.width + 4, input_rect.height + 4},
                             0.10f, 8, with_alpha(WL::CYAN_CORE, 14));
    }
    draw_text(seeded.seed_input.empty() ? std::string("Type a seed and press Enter...") : seeded.seed_input,
              {input_rect.x + 10.0f * scale, input_rect.y + 8.0f * scale},
              16.5f * scale,
              seeded.seed_input.empty() ? WL::TEXT_INACTIVE : WL::TEXT_PRIMARY);

    // Buttons
    const float btns_x = input_rect.x + input_rect.width + 12.0f * scale;
    if (draw_button({btns_x, input_y, btn_w, input_h},
                    "Generate",
                    {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, scale)
        || (seeded.input_active && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))) {
        run_generation(seeded);
    }
    if (draw_button({btns_x + btn_w + 8.0f * scale, input_y, replay_w, input_h},
                    "Replay",
                    {34, 40, 74, 240}, {46, 56, 100, 255}, WL::TEXT_PRIMARY, seeded.result.ready, scale)) {
        seeded.playback_time = 0.0f;
    }
    // Debug toggle
    const Rectangle debug_rect = {btns_x + btn_w + replay_w + 16.0f * scale, input_y, debug_w, input_h};
    if (draw_checkbox(debug_rect, seeded.debug_enabled, "Debug Trace",
                      "Staged reveal with animation.",
                      WL::CYAN_CORE, scale * 0.82f)) {
        seeded.debug_enabled = !seeded.debug_enabled;
    }

    // ── Row 2: Quick presets ─────────────────────────────────────────────────
    static constexpr const char* kPresetSeeds[] = {
        "worldline", "andromeda", "tachyon", "amber-signal", "quiet singularity"
    };
    const float row2_y = input_y + input_h + 8.0f * scale;
    draw_text("Presets", {rect.x + pad, row2_y + 3.0f * scale}, 10.0f * scale, with_alpha(WL::TEXT_TERTIARY, 160));
    const float presets_x = rect.x + pad + 52.0f * scale;
    const float chip_gap = 6.0f * scale;
    float chip_x = presets_x;
    for (int index = 0; index < 5; ++index) {
        const Vector2 text_sz = measure_ui_text(kPresetSeeds[index], 10.0f * scale);
        const float cw = text_sz.x + 16.0f * scale;
        const Rectangle chip = {chip_x, row2_y, cw, 20.0f * scale};
        const bool active = seeded.seed_input == kPresetSeeds[index];
        if (draw_button(chip,
                        kPresetSeeds[index],
                        active ? Color{18, 76, 90, 236} : Color{10, 20, 34, 228},
                        Color{18, 96, 112, 255},
                        active ? WL::CYAN_CORE : WL::TEXT_SECONDARY,
                        true,
                        scale * 0.62f)) {
            seeded.seed_input = kPresetSeeds[index];
            run_generation(seeded);
        }
        chip_x += cw + chip_gap;
    }

    if (!seeded.result.ready) return;

    // ── Row 3: Pipeline stages ───────────────────────────────────────────────
    const float row3_y = row2_y + 26.0f * scale;
    draw_separator_h(rect.x + pad, row3_y, inner_w, with_alpha(WL::GLASS_BORDER, 60));

    const float pipe_y = row3_y + 6.0f * scale;
    draw_text("PIPELINE", {rect.x + pad, pipe_y}, 9.0f * scale, with_alpha(WL::VIOLET_CORE, 150));

    static constexpr const char* kTitles[5] = {
        "01 Fold", "02 Expand", "03 Mutate", "04 Emit", "05 Assemble"
    };
    static constexpr const char* kNotes[5] = {
        "Text becomes raw bytes",
        "Bytes spread and mix",
        "Machine rewrites itself",
        "32 output values emerge",
        "Values become physics"
    };
    const bool reveal_all = !seeded.debug_enabled;
    const float stage_gap = 6.0f * scale;
    const float arrow_w = 14.0f * scale;
    const float total_arrows = arrow_w * 4.0f;
    const float stage_w = (inner_w - stage_gap * 4.0f - total_arrows) / 5.0f;
    const float stage_h = 44.0f * scale;
    const float stages_y = pipe_y + 14.0f * scale;

    for (int index = 0; index < 5; ++index) {
        const float sx = rect.x + pad + index * (stage_w + stage_gap + arrow_w) - (index > 0 ? 0 : 0);
        const float actual_x = rect.x + pad + index * (stage_w + stage_gap) + index * arrow_w;
        const Rectangle chip = {actual_x, stages_y, stage_w, stage_h};
        const bool active = reveal_all || seeded.playback_time >= kStageThresholds[index];
        const bool hot = CheckCollisionPointRec(mouse, chip);
        const Color accent = index % 2 == 0 ? WL::CYAN_CORE : WL::VIOLET_CORE;

        DrawRectangleRounded(chip, 0.10f, 8, active ? Color{10, 36, 50, 235} : Color{8, 14, 28, 222});
        DrawRectangleRoundedLines(chip, 0.10f, 8, 1.0f, with_alpha(accent, active ? 140 : 50));
        if (active) {
            DrawLineEx({chip.x + 2, chip.y + 1}, {chip.x + chip.width * 0.4f, chip.y + 1},
                       1.5f, with_alpha(accent, 100));
        }

        // Status dot
        const float dot_x = chip.x + 10.0f * scale;
        const float dot_y = chip.y + 12.0f * scale;
        if (active) {
            DrawCircleGradient(static_cast<int>(dot_x), static_cast<int>(dot_y),
                               7.0f * scale, with_alpha(accent, 80), {0, 0, 0, 0});
        }
        DrawCircleV({dot_x, dot_y}, 3.0f * scale, active ? accent : with_alpha(accent, 60));

        draw_text(kTitles[index], {chip.x + 22.0f * scale, chip.y + 6.0f * scale}, 11.0f * scale, WL::TEXT_PRIMARY);
        draw_text(kNotes[index], {chip.x + 22.0f * scale, chip.y + 22.0f * scale}, 9.0f * scale, WL::TEXT_TERTIARY);

        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::STAGE, index);
            seeded.playback_time = clamp_playback(kStageThresholds[index]);
        }

        // Arrow between stages
        if (index < 4) {
            const float ax = actual_x + stage_w + stage_gap * 0.5f + arrow_w * 0.2f;
            const float ay = stages_y + stage_h * 0.5f;
            DrawLineEx({ax, ay}, {ax + arrow_w * 0.5f, ay}, 1.0f, with_alpha(WL::TEXT_INACTIVE, 120));
            // Arrowhead
            DrawTriangle(
                {ax + arrow_w * 0.6f, ay - 3.0f * scale},
                {ax + arrow_w * 0.6f, ay + 3.0f * scale},
                {ax + arrow_w * 0.8f, ay},
                with_alpha(WL::TEXT_INACTIVE, 100));
        }
    }

    // ── Row 4: Stats strip ───────────────────────────────────────────────────
    const float row4_y = stages_y + stage_h + 8.0f * scale;
    draw_separator_h(rect.x + pad, row4_y, inner_w, with_alpha(WL::GLASS_BORDER, 40));

    const float stat_y = row4_y + 4.0f * scale;
    const int stat_count = 7;
    const float stat_gap = 4.0f * scale;
    const float stat_w = (inner_w - stat_gap * (stat_count - 1)) / static_cast<float>(stat_count);
    const float stat_h = 36.0f * scale;

    const std::string stat_labels[] = {"seed bytes", "snapshots", "mutations", "lanes", "full edits", "soft edits", "power p"};
    const std::size_t full_count = std::count_if(
        seeded.result.machine_trace.mutation_events.begin(),
        seeded.result.machine_trace.mutation_events.end(),
        [](const MutationEvent& e) { return e.mode == MutationMode::FULL; });
    const std::size_t soft_count = seeded.result.machine_trace.mutation_events.size() - full_count;
    const std::string stat_values[] = {
        std::to_string(seeded.result.seed.size()),
        std::to_string(seeded.result.expansion_trace.checkpoints.size()),
        std::to_string(seeded.result.machine_trace.mutation_events.size()),
        std::to_string(seeded.result.lanes.size()),
        std::to_string(full_count),
        std::to_string(soft_count),
        format_number(seeded.result.meta_spec.p, 2)
    };
    const Color stat_accents[] = {WL::CYAN_CORE, WL::CYAN_CORE, WL::VIOLET_CORE, WL::PLASMA_GREEN,
                                   WL::XENON_CORE, WL::VIOLET_CORE, WL::XENON_CORE};

    for (int i = 0; i < stat_count; ++i) {
        const Rectangle cell = {rect.x + pad + i * (stat_w + stat_gap), stat_y, stat_w, stat_h};
        DrawRectangleRounded(cell, 0.06f, 6, {6, 14, 24, 220});
        DrawRectangleRoundedLines(cell, 0.06f, 6, 1.0f, {24, 56, 76, 80});
        DrawLineEx({cell.x + 2, cell.y + 1}, {cell.x + cell.width * 0.35f, cell.y + 1},
                   1.2f, with_alpha(stat_accents[i], 80));
        draw_text(stat_labels[i], {cell.x + 7.0f * scale, cell.y + 4.0f * scale}, 9.0f * scale, WL::TEXT_TERTIARY);
        draw_text(stat_values[i], {cell.x + 7.0f * scale, cell.y + 16.0f * scale},
                  14.0f * scale, WL::TEXT_PRIMARY);
    }

    // ── Scrubber ─────────────────────────────────────────────────────────────
    const float scrub_y = stat_y + stat_h + 8.0f * scale;
    const Rectangle scrubber = {rect.x + pad, scrub_y, inner_w - 90.0f * scale, 10.0f * scale};

    draw_text("PLAYBACK " + format_number(seeded.playback_time, 1) + "s",
              {rect.x + pad, scrub_y - 12.0f * scale}, 9.0f * scale, WL::TEXT_TERTIARY);
    draw_text(seeded.debug_enabled ? "DEBUG ON" : "INSTANT",
              {scrubber.x + scrubber.width + 8.0f * scale, scrub_y - 12.0f * scale},
              9.0f * scale,
              seeded.debug_enabled ? with_alpha(WL::PLASMA_GREEN, 180) : WL::TEXT_INACTIVE);

    const bool scrub_hot = CheckCollisionPointRec(mouse, {scrubber.x, scrubber.y - 4, scrubber.width, scrubber.height + 8});
    if (scrub_hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        seeded.scrub_active = true;
    }
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        seeded.scrub_active = false;
    }
    if (seeded.scrub_active) {
        const float t = std::clamp((mouse.x - scrubber.x) / scrubber.width, 0.0f, 1.0f);
        seeded.playback_time = clamp_playback(t * 8.0f);
    }
    DrawRectangleRounded(scrubber, 0.5f, 8, {255, 255, 255, 12});
    const float progress = clamp_playback(seeded.playback_time) / 8.0f;
    DrawRectangleRounded({scrubber.x, scrubber.y, scrubber.width * progress, scrubber.height}, 0.5f, 8, {64, 228, 240, 90});
    const float knob_x = scrubber.x + scrubber.width * progress;
    DrawCircleGradient(static_cast<int>(knob_x), static_cast<int>(scrubber.y + scrubber.height * 0.5f),
                       8.0f * scale, {64, 228, 240, 80}, {0, 0, 0, 0});
    DrawCircleV({knob_x, scrubber.y + scrubber.height * 0.5f}, 4.0f * scale, WL::CYAN_CORE);

    // Stage threshold markers on scrubber
    for (int i = 0; i < 5; ++i) {
        const float mx = scrubber.x + scrubber.width * (kStageThresholds[i] / 8.0f);
        DrawLineEx({mx, scrubber.y + scrubber.height + 1}, {mx, scrubber.y + scrubber.height + 4.0f * scale},
                   1.0f, with_alpha(WL::TEXT_INACTIVE, 80));
    }
}

// ── Expansion Panel ──────────────────────────────────────────────────────────

inline void draw_expansion_panel(Rectangle rect,
                                 SeededUniverseUiState& seeded,
                                 float scale) {
    draw_card(rect, {5, 14, 25, 225}, with_alpha(WL::CYAN_DIM, 100));
    DrawRectangleGradientEx(rect,
                            {16, 42, 68, 55},
                            {7, 16, 30, 10},
                            {0, 0, 0, 0},
                            {8, 22, 34, 70});
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "SEEDER",
        "How the seed spreads",
        "Each row shows the 512-byte tape at a different point in time. Brighter = more active.",
        scale,
        WL::CYAN_CORE);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::EXPANSION);
    }

    const SeededUniverseResult& result = seeded.result;
    const float content_y = rect.y + 72.0f * scale;
    const float content_h = rect.height - 72.0f * scale - 28.0f * scale;
    const float row_h = std::min(30.0f * scale,
        content_h / std::max(1.0f, static_cast<float>(result.expansion_trace.checkpoints.size())));
    const std::size_t visible = visible_count(seeded.playback_time, 0.12f, 0.14f,
        result.expansion_trace.checkpoints.size(), !seeded.debug_enabled);

    for (std::size_t index = 0; index < visible; ++index) {
        const CellularCheckpoint& checkpoint = result.expansion_trace.checkpoints[index];
        const float y = content_y + index * row_h;
        if (y + row_h > rect.y + rect.height - 24.0f * scale) break;

        const Rectangle row = {rect.x + 10.0f * scale, y, rect.width - 20.0f * scale, row_h - 2.0f * scale};
        const bool selected = seeded.focus_kind == SeededFocusKind::CHECKPOINT
            && clamped_focus_index(seeded, static_cast<int>(result.expansion_trace.checkpoints.size())) == static_cast<int>(index);
        const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::CHECKPOINT, static_cast<int>(index));
        }
        if (selected || hot) {
            DrawRectangleRounded(row, 0.08f, 8, selected ? Color{12, 34, 52, 150} : Color{10, 20, 36, 90});
        }

        const float label_w = 44.0f * scale;
        draw_text(index == 0 ? std::string("fold") : "gen " + std::to_string(checkpoint.generation),
                  {row.x + 4.0f * scale, y + (row_h - 2.0f * scale) * 0.5f - 6.0f * scale},
                  11.0f * scale,
                  selected ? WL::TEXT_PRIMARY : WL::TEXT_SECONDARY);
        draw_snapshot_strip({row.x + label_w, y + 1.0f * scale, row.width - label_w - 4.0f * scale, row_h - 4.0f * scale},
                            checkpoint,
                            selected ? 1.0f : 0.45f + 0.08f * static_cast<float>(index),
                            scale);
    }

    // Footer
    if (!result.expansion_trace.checkpoints.empty()) {
        const int fi = clamped_focus_index(seeded, static_cast<int>(result.expansion_trace.checkpoints.size()));
        const CellularCheckpoint& cp = result.expansion_trace.checkpoints[fi];
        const float energy = average_bytes(cp.cells);
        draw_text("Selected: " + std::string(fi == 0 ? "fold" : "gen " + std::to_string(cp.generation))
                  + "  |  avg intensity " + format_number(energy, 3),
                  {rect.x + 14.0f * scale, rect.y + rect.height - 20.0f * scale},
                  10.0f * scale, WL::TEXT_TERTIARY);
    }
}

// ── Machine Panel ────────────────────────────────────────────────────────────

inline void draw_machine_panel(Rectangle rect,
                               SeededUniverseUiState& seeded,
                               float scale) {
    draw_card(rect, {6, 13, 26, 225}, with_alpha(WL::VIOLET_DIM, 100));
    DrawRectangleGradientEx(rect,
                            {48, 18, 78, 45},
                            {8, 16, 28, 10},
                            {0, 0, 0, 0},
                            {10, 20, 34, 70});
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "SEEDER",
        "Machine edits",
        "The machine rewrites its own rules as it runs. FULL = major rewrite, SOFT = small tweak.",
        scale,
        WL::VIOLET_CORE);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::MACHINE);
    }

    const SeededUniverseResult& result = seeded.result;
    const std::size_t max_cards = std::min<std::size_t>(8u, result.machine_trace.mutation_events.size());
    const std::size_t visible = visible_count(seeded.playback_time, 1.25f, 0.07f, max_cards, !seeded.debug_enabled);

    const float content_y = rect.y + 72.0f * scale;
    const float content_w = rect.width - 28.0f * scale;
    const int cols = 2;
    const float gap = 6.0f * scale;
    const float card_w = (content_w - gap * (cols - 1)) / static_cast<float>(cols);
    const float card_h = std::min(78.0f * scale,
        (rect.height - 72.0f * scale - 28.0f * scale - gap * 3.0f) / 4.0f);

    for (std::size_t index = 0; index < visible; ++index) {
        const MutationEvent& event = result.machine_trace.mutation_events[index];
        const std::size_t row = index / cols;
        const std::size_t col = index % cols;
        const Rectangle card = {
            rect.x + 14.0f * scale + col * (card_w + gap),
            content_y + row * (card_h + gap),
            card_w,
            card_h
        };
        if (card.y + card_h > rect.y + rect.height - 22.0f * scale) break;

        const bool hot = CheckCollisionPointRec(GetMousePosition(), card);
        const bool selected = seeded.focus_kind == SeededFocusKind::MUTATION
            && clamped_focus_index(seeded, static_cast<int>(result.machine_trace.mutation_events.size())) == static_cast<int>(index);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::MUTATION, static_cast<int>(index));
        }
        const Color accent = event.mode == MutationMode::FULL ? WL::XENON_CORE : WL::VIOLET_CORE;
        draw_card(card, {8, 16, 30, 232}, with_alpha(accent, selected ? 180 : (hot ? 150 : 90)));

        draw_badge({card.x + 8.0f * scale, card.y + 6.0f * scale, 48.0f * scale, 17.0f * scale},
                   mutation_mode_name(event.mode),
                   with_alpha(accent, 70),
                   accent,
                   scale * 0.84f);
        const std::string primitive = primitive_name(event.after.primitive);
        const float primitive_size = fit_ui_text_size(primitive, card.width - 16.0f * scale, 13.0f * scale, 10.0f * scale);
        draw_text(primitive,
                  {card.x + 8.0f * scale, card.y + 27.0f * scale},
                  primitive_size,
                  WL::TEXT_PRIMARY);
        draw_text("slot " + std::to_string(event.target_index) + " from " + std::to_string(event.source_index),
                  {card.x + 8.0f * scale, card.y + 45.0f * scale},
                  11.0f * scale,
                  WL::TEXT_SECONDARY);
        draw_text("p " + format_number(event.before.parameter, 2) + " -> " + format_number(event.after.parameter, 2),
                  {card.x + 8.0f * scale, card.y + 60.0f * scale},
                  11.0f * scale,
                  WL::TEXT_TERTIARY);

        // Hover tooltip
        if (hot) {
            draw_tooltip("Step " + std::to_string(event.step)
                         + (event.from_finalization ? " (finalization)" : ""),
                         {card.x + card_w * 0.5f, card.y},
                         scale);
        }
    }

    // Footer
    const std::size_t fc = std::count_if(result.machine_trace.mutation_events.begin(),
        result.machine_trace.mutation_events.end(),
        [](const MutationEvent& e) { return e.mode == MutationMode::FULL; });
    draw_text(std::to_string(fc) + " full  |  "
              + std::to_string(result.machine_trace.mutation_events.size() - fc) + " soft",
              {rect.x + 14.0f * scale, rect.y + rect.height - 18.0f * scale},
              10.0f * scale, WL::TEXT_TERTIARY);
}

// ── Lanes Panel ──────────────────────────────────────────────────────────────

inline void draw_lanes_panel(Rectangle rect,
                             SeededUniverseUiState& seeded,
                             float scale) {
    draw_card(rect, {5, 12, 24, 225}, with_alpha(WL::PLASMA_DIM, 95));
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "OUTPUT",
        "Lane spectrum",
        "The 32 final output values. Bars show magnitude, the line below shows the flow between neighbors.",
        scale,
        WL::PLASMA_GREEN);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::LANES);
    }

    const SeededUniverseResult& result = seeded.result;
    const std::size_t visible = visible_count(seeded.playback_time, 2.0f, 0.026f, result.lanes.size(), !seeded.debug_enabled);

    const float plot_x = rect.x + 14.0f * scale;
    const float plot_y = rect.y + 72.0f * scale;
    const float plot_w = rect.width - 28.0f * scale;
    const float plot_h = rect.height - 72.0f * scale - 60.0f * scale;
    DrawRectangleRounded({plot_x, plot_y, plot_w, plot_h}, 0.06f, 8, {7, 16, 30, 210});
    DrawRectangleRoundedLines({plot_x, plot_y, plot_w, plot_h}, 0.06f, 8, 1.0f, with_alpha(WL::PLASMA_DIM, 40));

    // Grid lines
    for (int grid = 1; grid <= 3; ++grid) {
        const float y = plot_y + plot_h * (static_cast<float>(grid) / 4.0f);
        DrawLineEx({plot_x + 2, y}, {plot_x + plot_w - 2, y}, 1.0f, {255, 255, 255, 10});
    }

    // Bars
    const float slot_w = plot_w / 32.0f;
    const Vector2 mouse = GetMousePosition();
    int hovered_lane = -1;

    for (std::size_t lane = 0; lane < visible; ++lane) {
        const double value = result.lanes[lane];
        const float height = (plot_h - 4.0f * scale) * static_cast<float>(value);
        const Rectangle bar = {
            plot_x + lane * slot_w + 1.5f,
            plot_y + plot_h - 2.0f * scale - height,
            std::max(2.0f, slot_w - 3.0f),
            std::max(3.0f, height)
        };
        const bool hot = CheckCollisionPointRec(mouse, {bar.x - 1, plot_y, bar.width + 2, plot_h});
        const bool selected = seeded.focus_kind == SeededFocusKind::LANE
            && clamped_focus_index(seeded, static_cast<int>(result.lanes.size())) == static_cast<int>(lane);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::LANE, static_cast<int>(lane));
        }
        if (hot) hovered_lane = static_cast<int>(lane);

        const Color fill = selected ? glow_tint(WL::PLASMA_GREEN, 0.3f) : hot ? glow_tint(WL::CYAN_CORE, 0.2f) : Color{
            static_cast<unsigned char>(58 + 45 * value),
            static_cast<unsigned char>(136 + 82 * value),
            static_cast<unsigned char>(120 + 110 * value),
            220};
        DrawRectangleRec(bar, fill);
        if (selected) {
            DrawRectangleLinesEx(bar, 1.2f, WL::TEXT_PRIMARY);
        }
    }

    // Hover tooltip for lanes
    if (hovered_lane >= 0 && hovered_lane < static_cast<int>(result.lanes.size())) {
        const double val = result.lanes[hovered_lane];
        draw_tooltip("Lane " + std::to_string(hovered_lane) + ": " + format_number(val, 4),
                     {mouse.x, plot_y}, scale);
    }

    // Waveform
    const float wave_y = plot_y + plot_h + 6.0f * scale;
    const float wave_h = rect.height - (wave_y - rect.y) - 20.0f * scale;
    if (wave_h > 8.0f * scale) {
        for (std::size_t lane = 1; lane < visible; ++lane) {
            const Vector2 a = {
                plot_x + (lane - 1u) * slot_w + slot_w * 0.5f,
                wave_y + wave_h * 0.5f - wave_h * 0.4f * static_cast<float>(result.lanes[lane - 1u])
            };
            const Vector2 b = {
                plot_x + lane * slot_w + slot_w * 0.5f,
                wave_y + wave_h * 0.5f - wave_h * 0.4f * static_cast<float>(result.lanes[lane])
            };
            DrawLineEx(a, b, 1.5f * scale, with_alpha(WL::PLASMA_GREEN, 120));
            DrawCircleV(b, 1.8f * scale, WL::PLASMA_GREEN);
        }
    }
}

// ── Registers Panel ──────────────────────────────────────────────────────────

inline void draw_registers_panel(Rectangle rect,
                                 SeededUniverseUiState& seeded,
                                 float scale) {
    draw_card(rect, {7, 12, 24, 225}, with_alpha(WL::CYAN_DIM, 85));
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "MACHINE",
        "Register residue",
        "The machine's internal memory after all processing. Colors show positive (teal) vs negative (violet).",
        scale,
        WL::CYAN_CORE);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::REGISTERS);
    }

    const auto& regs = seeded.result.machine_trace.final_state.registers;
    const float peak = max_abs_register(regs);
    const float content_y = rect.y + 72.0f * scale;
    const float content_w = rect.width - 28.0f * scale;
    const float content_h = rect.height - 72.0f * scale - 12.0f * scale;
    const int columns = 8;
    const float gap = 4.0f * scale;
    const float cell_w = (content_w - gap * (columns - 1)) / static_cast<float>(columns);
    const int rows = (static_cast<int>(regs.size()) + columns - 1) / columns;
    const float cell_h = std::min(38.0f * scale, (content_h - gap * (rows - 1)) / static_cast<float>(rows));

    int hovered_reg = -1;
    for (std::size_t index = 0; index < regs.size(); ++index) {
        const int row = static_cast<int>(index / columns);
        const int col = static_cast<int>(index % columns);
        const Rectangle cell = {
            rect.x + 14.0f * scale + col * (cell_w + gap),
            content_y + row * (cell_h + gap),
            cell_w,
            cell_h
        };
        if (cell.y + cell_h > rect.y + rect.height - 6.0f * scale) break;

        const bool hot = CheckCollisionPointRec(GetMousePosition(), cell);
        const bool selected = seeded.focus_kind == SeededFocusKind::REGISTER_SLOT
            && clamped_focus_index(seeded, static_cast<int>(regs.size())) == static_cast<int>(index);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::REGISTER_SLOT, static_cast<int>(index));
        }
        if (hot) hovered_reg = static_cast<int>(index);

        DrawRectangleRounded(cell, 0.08f, 6, signed_heat_color(regs[index], peak, selected ? 250 : 210));
        DrawRectangleRoundedLines(cell, 0.08f, 6, 1.0f,
            selected ? with_alpha(WL::TEXT_PRIMARY, 120) : with_alpha(WL::TEXT_PRIMARY, 24));
        draw_text("r" + std::to_string(index), {cell.x + 4.0f * scale, cell.y + 3.0f * scale},
                  9.0f * scale, with_alpha(WL::TEXT_PRIMARY, 180));
        draw_text(format_number(regs[index], 2), {cell.x + 4.0f * scale, cell.y + 14.0f * scale},
                  11.0f * scale, WL::TEXT_PRIMARY);
    }

    // Hover tooltip
    if (hovered_reg >= 0 && hovered_reg < static_cast<int>(regs.size())) {
        draw_tooltip("r" + std::to_string(hovered_reg) + " = " + format_number(regs[hovered_reg], 6),
                     GetMousePosition(), scale);
    }
}

// ── Orbit Panel ──────────────────────────────────────────────────────────────

inline void draw_orbit_panel(Rectangle rect,
                             SeededUniverseUiState& seeded,
                             float scale) {
    draw_card(rect, {5, 12, 25, 225}, with_alpha(WL::CYAN_DIM, 90));
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "METASPEC",
        "Assembly map",
        "Each node is a piece of the final physics. Bigger and brighter = stronger influence on the universe.",
        scale,
        WL::CYAN_CORE);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::ORBIT);
    }

    static constexpr const char* kLabels[6] = {"Metric", "Potential", "Symmetry", "Coupling", "Arrow", "Launch"};
    static constexpr const char* kDescriptions[6] = {
        "Defines distances",
        "Shapes energy landscape",
        "Controls balance",
        "Links the two pendulums",
        "Time direction effects",
        "Starting position"
    };
    const std::array<double, 6> strengths = stage_strengths(seeded.result.meta_spec);
    const std::size_t visible = visible_count(seeded.playback_time, 2.5f, 0.18f, 6u, !seeded.debug_enabled);
    const float orbit_size = std::min(rect.width - 28.0f * scale, rect.height - 72.0f * scale - 20.0f * scale);
    const Vector2 center = {
        rect.x + rect.width * 0.5f,
        rect.y + 72.0f * scale + (rect.height - 72.0f * scale - 20.0f * scale) * 0.5f
    };
    const float radius = orbit_size * 0.32f;

    // Rings
    DrawRing(center, radius - 2.0f * scale, radius, 0.0f, 360.0f, 72, {64, 140, 180, 22});
    DrawRing(center, radius * 0.55f, radius * 0.55f + 1.0f * scale, 0.0f, 360.0f, 72, {64, 140, 180, 12});
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y),
                       20.0f * scale, {54, 210, 230, 45}, {0, 0, 0, 0});
    DrawCircleV(center, 5.0f * scale, WL::CYAN_CORE);

    for (std::size_t index = 0; index < visible; ++index) {
        const float angle = -1.5707963f + static_cast<float>(index) * (6.2831853f / 6.0f);
        const Vector2 node = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        const Rectangle hit = {node.x - 14.0f * scale, node.y - 14.0f * scale, 28.0f * scale, 28.0f * scale};
        const bool hot = CheckCollisionPointRec(GetMousePosition(), hit);
        const bool selected = seeded.focus_kind == SeededFocusKind::STAGE
            && clamped_focus_index(seeded, 6) == static_cast<int>(index);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::STAGE, static_cast<int>(index));
        }
        const float normalized = static_cast<float>(std::clamp(strengths[index] / 2.4, 0.1, 1.0));
        const Color accent = {
            static_cast<unsigned char>(WL::CYAN_CORE.r * normalized + WL::XENON_CORE.r * (1.0f - normalized) * 0.28f),
            static_cast<unsigned char>(WL::CYAN_CORE.g * normalized + 44.0f * (1.0f - normalized)),
            static_cast<unsigned char>(WL::VIOLET_CORE.b * (1.0f - normalized) * 0.35f + WL::PLASMA_GREEN.b * normalized),
            220
        };
        DrawLineEx(center, node, 1.2f * scale, with_alpha(accent, 70));
        const float glow_r = (selected ? 16.0f : 10.0f + 4.0f * normalized) * scale;
        DrawCircleGradient(static_cast<int>(node.x), static_cast<int>(node.y),
                           glow_r, with_alpha(accent, selected ? 110 : 70), {0, 0, 0, 0});
        DrawCircleV(node, (selected ? 7.0f : 5.0f + 3.0f * normalized) * scale, accent);
        const Vector2 label_sz = measure_ui_text(kLabels[index], 11.0f * scale);
        draw_text(kLabels[index], {node.x - label_sz.x * 0.5f, node.y + 12.0f * scale},
                  11.0f * scale, WL::TEXT_PRIMARY);

        // Hover tooltip with description
        if (hot) {
            draw_tooltip(std::string(kLabels[index]) + ": " + kDescriptions[index]
                         + " (strength " + format_number(strengths[index], 2) + ")",
                         {node.x, node.y - 16.0f * scale}, scale);
        }
    }
}

// ── Tensors Panel ────────────────────────────────────────────────────────────

inline void draw_tensors_panel(Rectangle rect,
                               SeededUniverseUiState& seeded,
                               float scale) {
    draw_card(rect, {6, 12, 25, 225}, with_alpha(WL::GLASS_BORDER, 100));
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "METASPEC",
        "Tensor vault",
        "The raw 2x2 matrices that define this universe's physics. Teal = positive, violet = negative.",
        scale,
        WL::XENON_CORE);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::TENSORS);
    }

    const MetaSpec& ms = seeded.result.meta_spec;
    const float content_y = rect.y + 72.0f * scale;
    const float content_w = rect.width - 28.0f * scale;
    const float content_h = rect.height - 72.0f * scale - 10.0f * scale;
    const int cols = 3;
    const float gap = 6.0f * scale;
    const float card_w = (content_w - gap * (cols - 1)) / static_cast<float>(cols);
    const float card_h = (content_h - gap) / 2.0f;

    const auto draw_matrix_card = [&](Rectangle card, const char* title, const char* friendly_name,
                                      const double matrix[2][2], Color accent, int tensor_index) {
        const bool hot = CheckCollisionPointRec(GetMousePosition(), card);
        const bool selected = seeded.focus_kind == SeededFocusKind::TENSOR
            && clamped_focus_index(seeded, 6) == tensor_index;
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::TENSOR, tensor_index);
        }
        draw_card(card, {8, 15, 27, 228}, with_alpha(accent, selected ? 160 : (hot ? 130 : 85)));
        draw_text(title, {card.x + 8.0f * scale, card.y + 6.0f * scale}, 12.0f * scale, WL::TEXT_PRIMARY);
        draw_text(friendly_name, {card.x + 8.0f * scale, card.y + 20.0f * scale}, 10.0f * scale, WL::TEXT_TERTIARY);

        const float peak = matrix_peak(matrix);
        const float cg = 4.0f * scale;
        const float cw = (card.width - 16.0f * scale - cg) * 0.5f;
        const float ch = (card.height - 36.0f * scale - cg) * 0.5f;
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 2; ++col) {
                const Rectangle cell = {
                    card.x + 8.0f * scale + col * (cw + cg),
                    card.y + 32.0f * scale + row * (ch + cg),
                    cw,
                    ch
                };
                DrawRectangleRounded(cell, 0.08f, 6, signed_heat_color(matrix[row][col], peak, 215));
                DrawRectangleRoundedLines(cell, 0.08f, 6, 1.0f, with_alpha(WL::TEXT_PRIMARY, 28));
                draw_text(format_number(matrix[row][col], 3),
                          {cell.x + 4.0f * scale, cell.y + cell.height * 0.5f - 5.0f * scale},
                          11.0f * scale, WL::TEXT_PRIMARY);
            }
        }

        if (hot) {
            draw_tooltip(std::string(title) + "  peak: " + format_number(peak, 3),
                         {card.x + card_w * 0.5f, card.y}, scale);
        }
    };

    const float cx = rect.x + 14.0f * scale;
    draw_matrix_card({cx, content_y, card_w, card_h}, "g", "metric / distances", ms.g, WL::CYAN_CORE, 0);
    draw_matrix_card({cx + card_w + gap, content_y, card_w, card_h}, "V", "potential / energy", ms.V, WL::XENON_CORE, 1);
    draw_matrix_card({cx + 2.0f * (card_w + gap), content_y, card_w, card_h}, "S", "symmetry / balance", ms.S, WL::VIOLET_CORE, 2);
    draw_matrix_card({cx, content_y + card_h + gap, card_w, card_h}, "C0", "coupling / link 1", ms.C[0], WL::PLASMA_GREEN, 3);
    draw_matrix_card({cx + card_w + gap, content_y + card_h + gap, card_w, card_h}, "C1", "coupling / link 2", ms.C[1], WL::PLASMA_GREEN, 4);
    draw_matrix_card({cx + 2.0f * (card_w + gap), content_y + card_h + gap, card_w, card_h}, "W", "warp / distortion", ms.W, WL::CYAN_DIM, 5);
}

// ── Law Weave Panel ─────────────────────────────────────────────────────────

inline void draw_law_panel(Rectangle rect,
                           SeededUniverseUiState& seeded,
                           float scale) {
    draw_card(rect, {7, 14, 27, 226}, with_alpha(WL::PLASMA_GREEN, 80));
    DrawRectangleGradientEx(rect,
                            {10, 34, 32, 34},
                            {7, 12, 22, 10},
                            {6, 12, 22, 8},
                            {24, 10, 36, 24});
    const PanelHeaderResult header = draw_panel_header(
        rect,
        "LAWSPEC",
        "Law weave",
        "The tensor vault is folded into a bounded phase flow here. The path is a real LawSpec preview, not a decoration.",
        scale,
        WL::PLASMA_GREEN);
    if (header.info_clicked) {
        open_info_modal(seeded, SeededInfoTopic::LAW_WEAVE);
    }

    const MetaSpec& ms = seeded.result.meta_spec;
    const SeededLawPreview& preview = seeded.result.law_preview;
    const float pad = 14.0f * scale;
    const float content_y = rect.y + 72.0f * scale;
    const float content_h = rect.height - (content_y - rect.y) - 12.0f * scale;
    const float plot_w = rect.width * 0.58f;
    const Rectangle plot = {
        rect.x + pad,
        content_y,
        plot_w - pad,
        content_h
    };
    const Rectangle side = {
        plot.x + plot.width + 10.0f * scale,
        content_y,
        rect.x + rect.width - (plot.x + plot.width + 10.0f * scale) - pad,
        content_h
    };

    DrawRectangleRounded(plot, 0.06f, 8, {6, 16, 28, 215});
    DrawRectangleRoundedLines(plot, 0.06f, 8, 1.0f, with_alpha(WL::PLASMA_GREEN, 70));

    const Vector2 center = {plot.x + plot.width * 0.5f, plot.y + plot.height * 0.5f};
    const float ring_radius = std::min(plot.width, plot.height) * 0.40f;
    DrawRing(center, ring_radius * 0.45f, ring_radius * 0.45f + 1.0f * scale, 0.0f, 360.0f, 72, {64, 220, 180, 14});
    DrawRing(center, ring_radius * 0.82f, ring_radius * 0.82f + 1.0f * scale, 0.0f, 360.0f, 72, {64, 220, 180, 18});
    DrawLineEx({plot.x + 10.0f * scale, center.y}, {plot.x + plot.width - 10.0f * scale, center.y}, 1.0f, {255, 255, 255, 14});
    DrawLineEx({center.x, plot.y + 10.0f * scale}, {center.x, plot.y + plot.height - 10.0f * scale}, 1.0f, {255, 255, 255, 14});

    double bound = 0.4;
    for (const Vec2& point : preview.phase_path) {
        bound = std::max(bound, std::max(std::abs(point.x), std::abs(point.y)));
    }
    bound *= 1.10;
    const float phase_scale = ring_radius / static_cast<float>(bound);
    const auto project = [&](Vec2 q) {
        return Vector2{
            center.x + static_cast<float>(q.x) * phase_scale,
            center.y - static_cast<float>(q.y) * phase_scale
        };
    };

    const std::size_t visible = visible_count(
        seeded.playback_time,
        3.2f,
        0.012f,
        preview.phase_path.size(),
        !seeded.debug_enabled);
    for (std::size_t index = 1; index < visible; ++index) {
        const Vector2 a = project(preview.phase_path[index - 1]);
        const Vector2 b = project(preview.phase_path[index]);
        const double p_here = preview.p_samples[std::min(index, preview.p_samples.size() - 1)];
        const double p_span = std::max(0.10, preview.p_max - preview.p_min);
        const float p_mix = static_cast<float>(std::clamp((p_here - preview.p_min) / p_span, 0.0, 1.0));
        const Color strand = {
            static_cast<unsigned char>(WL::CYAN_CORE.r * (1.0f - p_mix) + WL::VIOLET_CORE.r * p_mix),
            static_cast<unsigned char>(WL::PLASMA_GREEN.g * (1.0f - p_mix) + WL::VIOLET_CORE.g * p_mix),
            static_cast<unsigned char>(WL::CYAN_CORE.b * (1.0f - p_mix) + WL::VIOLET_CORE.b * p_mix),
            static_cast<unsigned char>(92 + 120 * (static_cast<float>(index) / std::max<std::size_t>(2u, visible)))
        };
        DrawLineEx(a, b, 1.6f * scale, strand);
    }
    if (!preview.phase_path.empty()) {
        const Vector2 start = project(preview.phase_path.front());
        const Vector2 current = project(preview.phase_path[std::max<std::size_t>(0u, visible == 0u ? 0u : visible - 1u)]);
        DrawCircleGradient(static_cast<int>(current.x), static_cast<int>(current.y), 11.0f * scale, {130, 255, 220, 90}, {0, 0, 0, 0});
        DrawCircleV(start, 3.0f * scale, with_alpha(WL::TEXT_PRIMARY, 180));
        DrawCircleV(current, 4.2f * scale, WL::PLASMA_GREEN);
        draw_text("seed", {start.x + 4.0f * scale, start.y - 8.0f * scale}, 10.0f * scale, WL::TEXT_TERTIARY);
        draw_text("live", {current.x + 4.0f * scale, current.y - 8.0f * scale}, 10.0f * scale, WL::TEXT_PRIMARY);
    }

    draw_text("phase portrait", {plot.x + 8.0f * scale, plot.y + 8.0f * scale}, 11.0f * scale, with_alpha(WL::PLASMA_GREEN, 180));
    draw_text(law_mode_label(ms), {plot.x + 8.0f * scale, plot.y + 22.0f * scale}, 10.0f * scale, WL::TEXT_SECONDARY);

    draw_metric({side.x, side.y, side.width * 0.48f, 40.0f * scale}, "mode",
                ms.p_dynamic ? "adaptive" : "fixed", scale * 0.90f);
    draw_metric({side.x + side.width * 0.52f, side.y, side.width * 0.48f, 40.0f * scale}, "beta",
                format_number(ms.p_beta, 2), scale * 0.90f);
    draw_metric({side.x, side.y + 46.0f * scale, side.width * 0.48f, 40.0f * scale}, "lin gain",
                format_number(preview.linear_gain, 2), scale * 0.90f);
    draw_metric({side.x + side.width * 0.52f, side.y + 46.0f * scale, side.width * 0.48f, 40.0f * scale}, "accel cap",
                format_number(preview.accel_ceiling, 1), scale * 0.90f);

    draw_text("p trace", {side.x, side.y + 98.0f * scale}, 11.0f * scale, with_alpha(WL::CYAN_CORE, 170));
    const Rectangle p_plot = {side.x, side.y + 114.0f * scale, side.width, 50.0f * scale};
    DrawRectangleRounded(p_plot, 0.08f, 6, {6, 14, 24, 215});
    DrawRectangleRoundedLines(p_plot, 0.08f, 6, 1.0f, with_alpha(WL::CYAN_CORE, 55));
    const double p_center = 0.5 * (preview.p_min + preview.p_max);
    const double p_half_span = std::max(0.12, 0.55 * (preview.p_max - preview.p_min));
    const std::size_t p_visible = std::max<std::size_t>(2u, visible);
    for (std::size_t index = 1; index < std::min<std::size_t>(p_visible, preview.p_samples.size()); ++index) {
        const float x0 = p_plot.x + (p_plot.width - 8.0f * scale) * static_cast<float>(index - 1) / static_cast<float>(preview.p_samples.size() - 1) + 4.0f * scale;
        const float x1 = p_plot.x + (p_plot.width - 8.0f * scale) * static_cast<float>(index) / static_cast<float>(preview.p_samples.size() - 1) + 4.0f * scale;
        const float y0 = p_plot.y + p_plot.height * 0.5f
            - static_cast<float>((preview.p_samples[index - 1] - p_center) / p_half_span) * (p_plot.height * 0.34f);
        const float y1 = p_plot.y + p_plot.height * 0.5f
            - static_cast<float>((preview.p_samples[index] - p_center) / p_half_span) * (p_plot.height * 0.34f);
        DrawLineEx({x0, y0}, {x1, y1}, 1.5f * scale, with_alpha(ms.p_dynamic ? WL::VIOLET_CORE : WL::CYAN_CORE, 180));
    }
    draw_text(format_number(preview.p_min, 2) + " -> " + format_number(preview.p_max, 2),
              {p_plot.x + 6.0f * scale, p_plot.y + p_plot.height - 13.0f * scale},
              10.0f * scale, WL::TEXT_SECONDARY);

    draw_text("symmetry weave", {side.x, side.y + 174.0f * scale}, 11.0f * scale, with_alpha(WL::XENON_CORE, 170));
    const auto draw_bar = [&](float y, const char* label, double value, Color accent) {
        draw_text(label, {side.x, y}, 10.0f * scale, WL::TEXT_SECONDARY);
        const Rectangle track = {side.x + 70.0f * scale, y + 3.0f * scale, side.width - 70.0f * scale, 8.0f * scale};
        DrawRectangleRounded(track, 0.5f, 8, {255, 255, 255, 10});
        DrawRectangleRounded({track.x, track.y, track.width * static_cast<float>(std::clamp(value, 0.0, 1.0)), track.height},
                             0.5f, 8, with_alpha(accent, 135));
        draw_text(format_number(value, 2), {track.x + track.width - 28.0f * scale, y - 2.0f * scale},
                  10.0f * scale, WL::TEXT_PRIMARY);
    };
    draw_bar(side.y + 190.0f * scale, "additive", ms.s_a, WL::CYAN_CORE);
    draw_bar(side.y + 208.0f * scale, "filter", ms.s_b, WL::PLASMA_GREEN);
    draw_bar(side.y + 226.0f * scale, "torque", ms.s_c, WL::VIOLET_CORE);

    draw_text("mean speed " + format_number(preview.mean_speed, 2)
                + "  |  radius " + format_number(preview.radius_mean, 2)
                + " / " + format_number(preview.radius_peak, 2),
              {side.x, side.y + side.height - 24.0f * scale},
              10.5f * scale, WL::TEXT_TERTIARY);
    draw_text("handedness " + format_number(preview.handedness, 2)
                + "  |  max accel " + format_number(preview.max_accel, 2),
              {side.x, side.y + side.height - 11.0f * scale},
              10.5f * scale, WL::TEXT_TERTIARY);
}

// ── Descriptor / Readout Panel ───────────────────────────────────────────────

inline void draw_descriptor_panel(Rectangle rect,
                                  SeededUniverseUiState& seeded,
                                  float scale) {
    draw_card(rect, {8, 15, 28, 228}, with_alpha(WL::XENON_DIM, 100));
    DrawRectangleGradientEx(rect,
                            {24, 10, 40, 35},
                            {8, 14, 26, 10},
                            {6, 12, 22, 8},
                            {14, 8, 24, 35});

    const float pad = 14.0f * scale;
    const float left_w = 290.0f * scale;
    const float divider_x = rect.x + left_w + pad;

    // ── Left: Active Focus ───────────────────────────────────────────────────
    draw_text("ACTIVE FOCUS", {rect.x + pad, rect.y + 10.0f * scale}, 11.0f * scale, with_alpha(WL::CYAN_CORE, 160));

    const MetaSpec& ms = seeded.result.meta_spec;
    std::string focus_title = "Nothing selected";
    std::string focus_value = "Click any element above";
    std::string focus_line1;
    std::string focus_line2;
    float focus_ratio = 0.0f;

    switch (seeded.focus_kind) {
    case SeededFocusKind::CHECKPOINT:
        if (!seeded.result.expansion_trace.checkpoints.empty()) {
            const int index = clamped_focus_index(seeded, static_cast<int>(seeded.result.expansion_trace.checkpoints.size()));
            const CellularCheckpoint& cp = seeded.result.expansion_trace.checkpoints[index];
            focus_title = index == 0 ? "Seed fold (initial)" : "Generation " + std::to_string(cp.generation);
            focus_value = "avg intensity " + format_number(average_bytes(cp.cells), 3);
            focus_line1 = std::to_string(seeded.result.expansion_trace.checkpoints.size()) + " total snapshots";
            focus_line2 = "Click rows to compare how the seed mixes over time.";
            focus_ratio = average_bytes(cp.cells);
        }
        break;
    case SeededFocusKind::MUTATION:
        if (!seeded.result.machine_trace.mutation_events.empty()) {
            const int index = clamped_focus_index(seeded, static_cast<int>(seeded.result.machine_trace.mutation_events.size()));
            const MutationEvent& event = seeded.result.machine_trace.mutation_events[index];
            focus_title = std::string("Mutation #") + std::to_string(index);
            focus_value = primitive_name(event.after.primitive);
            focus_line1 = std::string(mutation_mode_name(event.mode)) + " edit: slot " + std::to_string(event.target_index);
            focus_line2 = "parameter changed " + format_number(event.before.parameter, 2) + " -> " + format_number(event.after.parameter, 2);
            focus_ratio = std::clamp(static_cast<float>(std::abs(event.after.parameter)), 0.0f, 1.0f);
        }
        break;
    case SeededFocusKind::LANE:
        if (!seeded.result.lanes.empty()) {
            const int index = clamped_focus_index(seeded, static_cast<int>(seeded.result.lanes.size()));
            const double value = seeded.result.lanes[index];
            focus_title = "Lane " + std::to_string(index);
            focus_value = format_number(value, 4);
            focus_line1 = "left neighbor " + format_number(index > 0 ? seeded.result.lanes[index - 1] : value, 3);
            focus_line2 = "right neighbor " + format_number(index + 1 < static_cast<int>(seeded.result.lanes.size()) ? seeded.result.lanes[index + 1] : value, 3);
            focus_ratio = static_cast<float>(value);
        }
        break;
    case SeededFocusKind::REGISTER_SLOT:
        {
            const int index = clamped_focus_index(seeded, static_cast<int>(seeded.result.machine_trace.final_state.registers.size()));
            const double value = seeded.result.machine_trace.final_state.registers[index];
            focus_title = "Register r" + std::to_string(index);
            focus_value = format_number(value, 4);
            focus_line1 = value >= 0.0 ? "positive (teal tone)" : "negative (violet tone)";
            focus_line2 = "Internal machine memory after all processing.";
            focus_ratio = std::clamp(static_cast<float>(std::abs(value)), 0.0f, 1.0f);
        }
        break;
    case SeededFocusKind::STAGE:
        {
            static constexpr const char* kStageNames[] = {"Metric", "Potential", "Symmetry", "Coupling", "Arrow", "Launch"};
            const auto str = stage_strengths(ms);
            const int index = clamped_focus_index(seeded, 6);
            focus_title = std::string(kStageNames[index]) + " stage";
            focus_value = "strength " + format_number(str[index], 3);
            focus_line1 = "How much this piece of physics matters.";
            focus_line2 = "Stronger stages dominate the simulation.";
            focus_ratio = static_cast<float>(std::clamp(str[index] / 2.4, 0.0, 1.0));
        }
        break;
    case SeededFocusKind::TENSOR:
        {
            static constexpr const char* kTensorNames[] = {"g", "V", "S", "C0", "C1", "W"};
            const int index = clamped_focus_index(seeded, 6);
            const double (*matrix)[2] = nullptr;
            if (index == 0) matrix = ms.g;
            else if (index == 1) matrix = ms.V;
            else if (index == 2) matrix = ms.S;
            else if (index == 3) matrix = ms.C[0];
            else if (index == 4) matrix = ms.C[1];
            else matrix = ms.W;
            focus_title = std::string("Tensor: ") + kTensorNames[index];
            focus_value = "peak " + format_number(matrix_peak(matrix), 3);
            focus_line1 = "A 2x2 grid of numbers that shapes one aspect of physics.";
            focus_line2 = "Color + number together show sign and size.";
            focus_ratio = std::clamp(matrix_peak(matrix), 0.0f, 1.0f);
        }
        break;
    default:
        break;
    }

    draw_text(focus_title, {rect.x + pad, rect.y + 24.0f * scale}, 18.5f * scale, WL::TEXT_PRIMARY);
    draw_text(focus_value, {rect.x + pad, rect.y + 49.0f * scale}, 26.0f * scale, WL::TEXT_PRIMARY);
    draw_text(focus_line1, {rect.x + pad, rect.y + 82.0f * scale}, 12.0f * scale, WL::TEXT_SECONDARY);
    draw_text(focus_line2, {rect.x + pad, rect.y + 99.0f * scale}, 12.0f * scale, WL::TEXT_TERTIARY);

    // Gauge
    const Rectangle gauge = {rect.x + pad, rect.y + 118.0f * scale, left_w - pad, 7.0f * scale};
    DrawRectangleRounded(gauge, 0.5f, 8, {255, 255, 255, 12});
    DrawRectangleRounded({gauge.x, gauge.y, gauge.width * std::clamp(focus_ratio, 0.0f, 1.0f), gauge.height},
                         0.5f, 8, {64, 228, 240, 100});

    // ── Divider ──────────────────────────────────────────────────────────────
    draw_separator_v(divider_x, rect.y + 8.0f * scale, rect.height - 16.0f * scale, with_alpha(WL::GLASS_BORDER, 50));

    // ── Right: Summary + Descriptor ──────────────────────────────────────────
    const float right_x = divider_x + 14.0f * scale;
    const float right_w = rect.x + rect.width - right_x - pad;
    const SeededLawPreview& law = seeded.result.law_preview;

    draw_text("UNIVERSE SUMMARY", {right_x, rect.y + 10.0f * scale}, 11.0f * scale, with_alpha(WL::XENON_CORE, 160));

    // Top metrics row
    const float met_y = rect.y + 24.0f * scale;
    draw_metric({right_x, met_y, 98.0f * scale, 40.0f * scale}, "power p", format_number(ms.p, 2), scale * 0.95f);
    draw_metric({right_x + 104.0f * scale, met_y, 130.0f * scale, 40.0f * scale}, "q0",
                format_number(ms.q0[0], 2) + ", " + format_number(ms.q0[1], 2), scale * 0.95f);
    draw_metric({right_x + 240.0f * scale, met_y, 144.0f * scale, 40.0f * scale}, "qdot0",
                format_number(ms.qdot0[0], 2) + ", " + format_number(ms.qdot0[1], 2), scale * 0.95f);

    const float law_y = met_y + 46.0f * scale;
    draw_metric({right_x, law_y, 118.0f * scale, 36.0f * scale}, "p mode",
                ms.p_dynamic ? "adaptive" : "fixed", scale * 0.82f);
    draw_metric({right_x + 124.0f * scale, law_y, 88.0f * scale, 36.0f * scale}, "beta",
                format_number(ms.p_beta, 2), scale * 0.82f);
    draw_metric({right_x + 218.0f * scale, law_y, 166.0f * scale, 36.0f * scale}, "S (a/b/c)",
                format_number(ms.s_a, 2) + " / " + format_number(ms.s_b, 2) + " / " + format_number(ms.s_c, 2),
                scale * 0.82f);
    draw_metric({right_x + 390.0f * scale, law_y, 122.0f * scale, 36.0f * scale}, "law gain",
                format_number(law.linear_gain, 2), scale * 0.82f);

    // Strength chips
    const auto strengths = stage_strengths(ms);
    static constexpr const char* kShort[] = {"g", "V", "S", "C", "T/G", "L"};
    const float chip_y = law_y + 44.0f * scale;
    const float chip_gap = 4.0f * scale;
    const float chip_w = std::min(60.0f * scale, (right_w - chip_gap * 5.0f) / 6.0f);
    for (int index = 0; index < 6; ++index) {
        const Rectangle chip = {right_x + index * (chip_w + chip_gap), chip_y, chip_w, 30.0f * scale};
        const bool hot = CheckCollisionPointRec(GetMousePosition(), chip);
        const bool selected = seeded.focus_kind == SeededFocusKind::STAGE && clamped_focus_index(seeded, 6) == index;
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            set_focus(seeded, SeededFocusKind::STAGE, index);
        }
        DrawRectangleRounded(chip, 0.10f, 8, selected ? Color{24, 60, 70, 232} : Color{8, 16, 30, 218});
        DrawRectangleRoundedLines(chip, 0.10f, 8, 1.0f,
            with_alpha(WL::CYAN_CORE, selected ? 150 : (hot ? 100 : 55)));
        draw_text(kShort[index], {chip.x + 6.0f * scale, chip.y + 4.0f * scale}, 13.0f * scale, WL::TEXT_PRIMARY);
        draw_text(format_number(strengths[index], 2), {chip.x + 6.0f * scale, chip.y + 16.0f * scale},
                  11.0f * scale, WL::TEXT_SECONDARY);
    }

    // Descriptor text
    const float desc_y = chip_y + 36.0f * scale;
    draw_separator_h(right_x, desc_y, right_w, with_alpha(WL::GLASS_BORDER, 40));
    const Rectangle desc_view = {
        right_x,
        desc_y + 8.0f * scale,
        right_w - 10.0f * scale,
        rect.height - (desc_y + 8.0f * scale - rect.y) - 12.0f * scale
    };
    const float desc_text_size = 15.0f * scale;
    const float desc_line_gap = 4.5f * scale;
    const float desc_content_h = measure_wrapped_ui_text_height(seeded.result.descriptor, desc_view.width, desc_text_size, desc_line_gap);
    const float desc_max_scroll = std::max(0.0f, desc_content_h - desc_view.height);
    seeded.descriptor_scroll = std::clamp(seeded.descriptor_scroll, 0.0f, desc_max_scroll);

    const Vector2 mouse = GetMousePosition();
    seeded.descriptor_hovered = CheckCollisionPointRec(mouse, desc_view);
    if (seeded.descriptor_hovered) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) {
            seeded.descriptor_scroll = std::clamp(
                seeded.descriptor_scroll - wheel * 36.0f * scale,
                0.0f,
                desc_max_scroll);
        }
    }

    BeginScissorMode(static_cast<int>(desc_view.x), static_cast<int>(desc_view.y),
                     static_cast<int>(desc_view.width), static_cast<int>(desc_view.height));
    draw_text_block(seeded.result.descriptor,
                    {desc_view.x, desc_view.y - seeded.descriptor_scroll, desc_view.width, desc_content_h + desc_line_gap},
                    desc_text_size,
                    WL::TEXT_SECONDARY,
                    desc_line_gap);
    EndScissorMode();

    draw_scrollbar(desc_view, seeded.descriptor_scroll, desc_max_scroll);
}

// ── Info Modal ───────────────────────────────────────────────────────────────

inline const char* info_title(SeededInfoTopic topic) {
    switch (topic) {
    case SeededInfoTopic::COMMAND_DECK: return "Seed Drive";
    case SeededInfoTopic::PIPELINE: return "The Pipeline";
    case SeededInfoTopic::EXPANSION: return "Seed Spreading";
    case SeededInfoTopic::MACHINE: return "Machine Edits";
    case SeededInfoTopic::LANES: return "Lane Spectrum";
    case SeededInfoTopic::REGISTERS: return "Register Residue";
    case SeededInfoTopic::ORBIT: return "Assembly Map";
    case SeededInfoTopic::TENSORS: return "Tensor Vault";
    case SeededInfoTopic::LAW_WEAVE: return "Law Weave";
    case SeededInfoTopic::DESCRIPTOR: return "Universe Readout";
    default: return "";
    }
}

inline const char* info_body(SeededInfoTopic topic) {
    switch (topic) {
    case SeededInfoTopic::COMMAND_DECK:
        return "This is your control panel for the entire seed-to-universe process. The seed text is the only input you give. Everything else is generated deterministically from it.\n\nGenerate runs the full pipeline from scratch. Replay resets the animation so you can watch it build step by step. Debug Trace toggles between instant results and the animated stage-by-stage reveal.\n\nThe presets are interesting seeds we picked because they produce visually distinct universes. Try your own name, a word, or anything you like.";
    case SeededInfoTopic::PIPELINE:
        return "The pipeline shows the five stages your seed goes through to become a universe.\n\n1. Fold: Your text gets converted into raw bytes.\n2. Expand: Those bytes are spread across a 512-cell tape using cellular automata, so each byte's influence reaches every part of the tape.\n3. Mutate: A self-modifying machine reads the tape and rewrites its own rules as it goes, creating complex behavior from simple input.\n4. Emit: The machine produces 32 output values between 0 and 1.\n5. Assemble: Those 32 numbers become the matrices and initial conditions that define your universe's physics.";
    case SeededInfoTopic::EXPANSION:
        return "Each row is a snapshot of the 512-byte tape at a different point in time. The initial fold shows your raw seed laid out on the tape. Each generation after that shows the tape after the cellular automaton has mixed it further.\n\nBrighter bars mean more activity at that position. Early snapshots show the seed's fingerprint clearly. Later snapshots should look more uniform as information spreads everywhere.\n\nIf late snapshots still show sharp patterns, the seed has a strong structural signature that survives mixing.";
    case SeededInfoTopic::MACHINE:
        return "The seed machine reads the expanded tape and uses it to process 32 internal registers. The twist: as it runs, it rewrites its own instruction table.\n\nFULL edits are major rewrites that swap what kind of math operation a slot performs. SOFT edits are gentle tweaks to an operation's parameters.\n\nThe mix of FULL and SOFT edits (roughly 55%/45%) is intentional. Too many FULL edits and the machine is chaotic. Too few and every seed produces similar output.\n\nEach card shows which operation was inserted, which slot was changed, and how the parameter shifted.";
    case SeededInfoTopic::LANES:
        return "The lane spectrum is the machine's final output: 32 values, each between 0 and 1. These are the raw building blocks for your universe's physics.\n\nThe bar chart shows each lane's value. The line underneath connects neighboring lanes so you can spot patterns: smooth gradients, sudden jumps, or repeating clusters.\n\nClick any bar to inspect it. The readout panel at the bottom will show you exact values and neighbors.";
    case SeededInfoTopic::REGISTERS:
        return "After the machine finishes processing, its 32 internal registers still hold values. Unlike the lanes (which are normalized to 0-1), registers can be positive or negative.\n\nTeal-colored cells are positive. Violet-colored cells are negative. Brighter colors mean larger values.\n\nLarge positive or negative outliers usually mean the machine developed a strong bias in that register, which often corresponds to a distinctive feature in the final universe.";
    case SeededInfoTopic::ORBIT:
        return "The orbit map arranges the six main components of your universe's physics in a circle. Each node represents one family of physical laws.\n\nMetric: How distances work in this universe.\nPotential: The energy landscape that pulls things around.\nSymmetry: How balanced the physics is between the two pendulum arms.\nCoupling: How strongly the two pendulums influence each other.\nArrow: Whether time has directional effects.\nLaunch: The starting position and velocity.\n\nBigger, brighter nodes have more influence on the final simulation.";
    case SeededInfoTopic::TENSORS:
        return "Each tile shows a 2x2 matrix: four numbers arranged in a grid. These matrices are the actual mathematical objects that define your universe's physics.\n\nTeal cells are positive values. Violet cells are negative values. Brighter = larger magnitude.\n\nThink of each matrix as a set of knobs. The diagonal values (top-left and bottom-right) affect each pendulum arm independently. The off-diagonal values (top-right and bottom-left) create cross-effects between the arms.\n\nClick any tile to inspect it in the readout panel below.";
    case SeededInfoTopic::LAW_WEAVE:
        return "Law Weave is the first place the seed stops looking like ingredients and starts looking like motion. The MetaSpec tensors are passed into LawSpec, which builds a vector field on phase space and then samples it with RK4.\n\nThe phase portrait on the left is not a decorative path. It is a short preview orbit generated directly from the seeded initial condition. The sparkline on the right shows how the nonlinear exponent p behaves across that preview. If p is dynamic, the line flexes with angular momentum but stays clamped near its seed.\n\nThe gain and ceiling metrics tell you how the law engine was stabilized: the linear potential term is spectrally bounded at construction time, and large accelerations trigger runtime step refinement rather than explosive drift. The three S bars show how symmetry affects the law after assembly: additive bias, symmetry filtering, and antisymmetric torque.";
    case SeededInfoTopic::DESCRIPTOR:
        return "The readout panel adapts to whatever you've clicked. It shows detailed information about the selected element: a lane value, a register, a mutation event, a tensor matrix, or an orbit node.\n\nThe left side shows your current selection with a value gauge. The right side shows a summary of the whole universe including the key physics parameters and a natural-language description of what this particular universe is like.\n\nThe strength chips at the bottom right let you quickly jump between the six physics families.";
    default:
        return "";
    }
}

inline void draw_info_modal(SeededUniverseUiState& seeded, Rectangle viewport, float scale) {
    if (seeded.info_topic == SeededInfoTopic::NONE) return;

    DrawRectangleRec(viewport, {2, 8, 14, 200});
    DrawCircleGradient(static_cast<int>(viewport.x + viewport.width * 0.5f),
                       static_cast<int>(viewport.y + viewport.height * 0.5f),
                       std::max(viewport.width, viewport.height) * 0.6f,
                       {0, 0, 0, 0},
                       {2, 6, 10, 140});

    const float modal_w = std::min(760.0f * scale, viewport.width * 0.78f);
    const float modal_h = std::min(620.0f * scale, viewport.height * 0.78f);
    const Rectangle modal = {
        viewport.x + (viewport.width - modal_w) * 0.5f,
        viewport.y + (viewport.height - modal_h) * 0.5f,
        modal_w,
        modal_h
    };

    const Vector2 mouse = GetMousePosition();
    if (seeded.info_ignore_mouse_until_release) {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            seeded.info_ignore_mouse_until_release = false;
        }
    } else if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, modal)) || IsKeyPressed(KEY_ESCAPE)) {
        seeded.info_topic = SeededInfoTopic::NONE;
        seeded.info_ignore_mouse_until_release = false;
    }

    draw_card(modal, {8, 16, 28, 248}, with_alpha(WL::CYAN_CORE, 100));
    DrawRectangleGradientEx(modal,
                            {16, 58, 78, 50},
                            {10, 18, 32, 10},
                            {0, 0, 0, 0},
                            {48, 16, 82, 35});
    draw_corner_brackets({modal.x + 4, modal.y + 4, modal.width - 8, modal.height - 8},
                         14.0f * scale, 1.2f, with_alpha(WL::CYAN_CORE, 60));

    draw_text("INFO", {modal.x + 18.0f * scale, modal.y + 16.0f * scale}, 12.0f * scale, with_alpha(WL::CYAN_CORE, 160));
    draw_text(info_title(seeded.info_topic), {modal.x + 18.0f * scale, modal.y + 32.0f * scale}, 28.0f * scale, WL::TEXT_PRIMARY);
    draw_separator_h(modal.x + 18.0f * scale, modal.y + 64.0f * scale, modal.width - 36.0f * scale, with_alpha(WL::GLASS_BORDER, 60));

    const Rectangle info_view = {
        modal.x + 18.0f * scale,
        modal.y + 72.0f * scale,
        modal.width - 28.0f * scale,
        modal.height - 122.0f * scale
    };
    const float info_text_size = 16.0f * scale;
    const float info_line_gap = 5.5f * scale;
    const float info_content_h =
        measure_wrapped_ui_text_height(info_body(seeded.info_topic), info_view.width - 12.0f * scale, info_text_size, info_line_gap);
    const float info_max_scroll = std::max(0.0f, info_content_h - info_view.height);
    seeded.info_modal_scroll = std::clamp(seeded.info_modal_scroll, 0.0f, info_max_scroll);
    if (CheckCollisionPointRec(mouse, info_view)) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) {
            seeded.info_modal_scroll = std::clamp(
                seeded.info_modal_scroll - wheel * 44.0f * scale,
                0.0f,
                info_max_scroll);
        }
    }

    BeginScissorMode(static_cast<int>(info_view.x), static_cast<int>(info_view.y),
                     static_cast<int>(info_view.width), static_cast<int>(info_view.height));
    draw_text_block(info_body(seeded.info_topic),
                    {info_view.x, info_view.y - seeded.info_modal_scroll, info_view.width - 12.0f * scale, info_content_h + info_line_gap},
                    info_text_size,
                    WL::TEXT_SECONDARY,
                    info_line_gap);
    EndScissorMode();
    draw_scrollbar(info_view, seeded.info_modal_scroll, info_max_scroll);

    draw_text("Click outside or press Escape to close",
              {modal.x + 18.0f * scale, modal.y + modal.height - 28.0f * scale},
              12.0f * scale,
              WL::TEXT_INACTIVE);

    if (draw_button({modal.x + modal.width - 90.0f * scale, modal.y + 14.0f * scale, 72.0f * scale, 26.0f * scale},
                    "CLOSE",
                    {20, 36, 60, 235},
                    {32, 54, 88, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale * 0.85f)) {
        seeded.info_topic = SeededInfoTopic::NONE;
        seeded.info_ignore_mouse_until_release = false;
    }
}

// ── Keyboard Navigation ──────────────────────────────────────────────────────

inline void handle_keyboard_nav(SeededUniverseUiState& seeded) {
    if (seeded.input_active) return;
    if (seeded.info_topic != SeededInfoTopic::NONE) return;

    int max_count = 0;
    switch (seeded.focus_kind) {
    case SeededFocusKind::CHECKPOINT:
        max_count = static_cast<int>(seeded.result.expansion_trace.checkpoints.size()); break;
    case SeededFocusKind::MUTATION:
        max_count = static_cast<int>(seeded.result.machine_trace.mutation_events.size()); break;
    case SeededFocusKind::LANE:
        max_count = static_cast<int>(seeded.result.lanes.size()); break;
    case SeededFocusKind::REGISTER_SLOT:
        max_count = static_cast<int>(seeded.result.machine_trace.final_state.registers.size()); break;
    case SeededFocusKind::STAGE:
    case SeededFocusKind::TENSOR:
        max_count = 6; break;
    default: return;
    }

    if (max_count <= 0) return;

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) {
        seeded.focus_index = (seeded.focus_index + 1) % max_count;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) {
        seeded.focus_index = (seeded.focus_index - 1 + max_count) % max_count;
    }

    // Tab cycles focus kind
    if (IsKeyPressed(KEY_TAB)) {
        static constexpr SeededFocusKind kCycle[] = {
            SeededFocusKind::LANE, SeededFocusKind::REGISTER_SLOT,
            SeededFocusKind::CHECKPOINT, SeededFocusKind::MUTATION,
            SeededFocusKind::STAGE, SeededFocusKind::TENSOR
        };
        constexpr int n = 6;
        int current = 0;
        for (int i = 0; i < n; ++i) {
            if (kCycle[i] == seeded.focus_kind) { current = i; break; }
        }
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const int next = shift ? (current - 1 + n) % n : (current + 1) % n;
        seeded.focus_kind = kCycle[next];
        seeded.focus_index = 0;
    }
}

// ── Main Entry Point ─────────────────────────────────────────────────────────

inline bool draw_seeded_universe_screen(AppState& app, Rectangle viewport) {
    SeededUniverseUiState& seeded = app.ui.seeded;
    if (!seeded.result.ready && seeded.result.error.empty()) {
        run_generation(seeded);
    }
    seeded.descriptor_hovered = false;

    const float scale = std::clamp(std::min(viewport.width / 1540.0f, viewport.height / 940.0f) * 1.10f, 0.82f, 1.18f);
    const bool modal_open = seeded.info_topic != SeededInfoTopic::NONE;

    if (seeded.result.ready && seeded.debug_enabled) {
        seeded.playback_time += GetFrameTime();
    } else if (seeded.result.ready) {
        seeded.playback_time = std::max(seeded.playback_time, 8.0f);
    }

    // Keyboard nav
    if (!modal_open) {
        handle_keyboard_nav(seeded);
    }

    if (modal_open && IsKeyPressed(KEY_ESCAPE)) {
        seeded.info_topic = SeededInfoTopic::NONE;
        seeded.info_ignore_mouse_until_release = false;
    }

    const bool back_to_menu = !modal_open
        && (draw_back_to_menu_button(viewport, scale)
            || (!seeded.input_active && IsKeyPressed(KEY_ESCAPE)));

    // ── Layout ───────────────────────────────────────────────────────────────
    const float margin = 10.0f * scale;
    const float gap = 8.0f * scale;

    // Top strip height adapts to whether results are ready
    const float top_h = seeded.result.ready ? 228.0f * scale : 82.0f * scale;
    const Rectangle top_rect = {
        viewport.x + margin,
        viewport.y + 52.0f * scale,
        viewport.width - margin * 2.0f,
        top_h
    };
    draw_top_strip(top_rect, seeded, scale);

    if (!seeded.result.ready) {
        if (!seeded.result.error.empty()) {
            draw_text("Generation failed: " + seeded.result.error,
                      {viewport.x + margin + 14.0f * scale, top_rect.y + top_rect.height + 14.0f * scale},
                      14.0f * scale, WL::XENON_CORE);
        }
        draw_info_modal(seeded, viewport, scale);
        return back_to_menu;
    }

    // ── Body area with scrolling ─────────────────────────────────────────────
    const Rectangle body_view = {
        viewport.x + margin,
        top_rect.y + top_rect.height + gap,
        viewport.width - margin * 2.0f,
        viewport.y + viewport.height - (top_rect.y + top_rect.height + gap) - margin
    };

    const float col_gap = gap;
    const float left_w = body_view.width * 0.50f - col_gap * 0.5f;
    const float right_w = body_view.width - left_w - col_gap;

    // Panel heights
    const float expansion_h = 220.0f * scale;
    const float machine_h = 252.0f * scale;
    const float lanes_h = 240.0f * scale;
    const float registers_h = 240.0f * scale;
    const float orbit_h = 260.0f * scale;
    const float tensors_h = 260.0f * scale;
    const float law_h = 252.0f * scale;
    const float descriptor_h = 272.0f * scale;

    // Content height: two columns, metaspec assembly row, law weave, then readout
    const float left_col_h = expansion_h + gap + machine_h;
    const float right_col_h = lanes_h + gap + registers_h;
    const float two_col_h = std::max(left_col_h, right_col_h);
    const float orbit_row_h = std::max(orbit_h, tensors_h);
    const float content_height = two_col_h + gap + orbit_row_h + gap + law_h + gap + descriptor_h;
    const float max_scroll = std::max(0.0f, content_height - body_view.height);

    BeginScissorMode(static_cast<int>(body_view.x), static_cast<int>(body_view.y),
                     static_cast<int>(body_view.width), static_cast<int>(body_view.height));

    float y = body_view.y - seeded.body_scroll;

    // Row 1: Expansion (left) + Lanes (right)
    draw_expansion_panel({body_view.x, y, left_w, expansion_h}, seeded, scale);
    draw_lanes_panel({body_view.x + left_w + col_gap, y, right_w, lanes_h}, seeded, scale);

    // Row 2: Machine (left) + Registers (right)
    const float row2_y_left = y + expansion_h + gap;
    const float row2_y_right = y + lanes_h + gap;
    draw_machine_panel({body_view.x, row2_y_left, left_w, machine_h}, seeded, scale);
    draw_registers_panel({body_view.x + left_w + col_gap, row2_y_right, right_w, registers_h}, seeded, scale);

    const float row2_bottom = std::max(row2_y_left + machine_h, row2_y_right + registers_h);

    // Row 3: Orbit (left) + Tensors (right) — full width split
    const float row3_y = row2_bottom + gap;
    const float orbit_w = body_view.width * 0.35f - col_gap * 0.5f;
    const float tensors_w = body_view.width - orbit_w - col_gap;
    draw_orbit_panel({body_view.x, row3_y, orbit_w, orbit_row_h}, seeded, scale);
    draw_tensors_panel({body_view.x + orbit_w + col_gap, row3_y, tensors_w, orbit_row_h}, seeded, scale);

    // Row 4: LawSpec preview — full width bridge between tensors and readout
    const float row4_y = row3_y + orbit_row_h + gap;
    draw_law_panel({body_view.x, row4_y, body_view.width, law_h}, seeded, scale);

    // Row 5: Descriptor — full width
    const float row5_y = row4_y + law_h + gap;
    draw_descriptor_panel({body_view.x, row5_y, body_view.width, descriptor_h}, seeded, scale);

    EndScissorMode();

    if (!modal_open
        && !seeded.descriptor_hovered
        && CheckCollisionPointRec(GetMousePosition(), body_view)) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) {
            seeded.body_scroll = std::clamp(seeded.body_scroll - wheel * 40.0f * scale, 0.0f, max_scroll);
        }
    }

    draw_scrollbar(body_view, seeded.body_scroll, max_scroll);
    draw_info_modal(seeded, viewport, scale);
    return back_to_menu;
}

} // namespace SeededUniverseUi

using SeededUniverseUi::draw_seeded_universe_screen;
