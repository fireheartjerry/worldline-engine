#pragma once
// Seeded Universe — generation-pipeline panels (seed input, machine, lanes, registers).
#include "SeededCommon.hpp"

namespace SeededUniverseUi {

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


} // namespace SeededUniverseUi
