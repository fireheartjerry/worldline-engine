#pragma once
// Seeded Universe screen — thin aggregator over focused seeded/* modules.
// The screen was a single 1844-line header; it is now composed from one
// module per concern so each system is small and easy to follow.
#include "seeded/SeededCommon.hpp"
#include "seeded/SeededFormat.hpp"
#include "seeded/SeededWidgets.hpp"
#include "seeded/SeededPanelsPipeline.hpp"
#include "seeded/SeededPanelsLaw.hpp"
#include "seeded/SeededInfoModal.hpp"
#include "seeded/SeededNav.hpp"

namespace SeededUniverseUi {

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

inline bool draw_seeded_universe_debug_screen(AppState& app, Rectangle viewport) {
    return draw_seeded_universe_screen(app, viewport);
}
