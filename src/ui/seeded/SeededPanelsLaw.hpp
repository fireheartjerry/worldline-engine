#pragma once
// Seeded Universe — law panels (orbit, tensors, law weave, descriptor readout).
#include "SeededCommon.hpp"

namespace SeededUniverseUi {

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


} // namespace SeededUniverseUi
