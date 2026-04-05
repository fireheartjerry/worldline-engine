#pragma once
#include "UiPrimitives.hpp"
#include "../app/AppTypes.hpp"

struct MainMenuResult {
    AppScreen target = AppScreen::MAIN_MENU;
};

// ── Decorative orbit ring (arc segment) ───────────────────────────────────────
inline void draw_menu_orbit(Vector2 center, float radius, float thickness,
                            Color color, float start_deg, float end_deg) {
    DrawRing(center, radius - thickness, radius, start_deg, end_deg, 80, color);
}

// ── Hex-grid dot — decorative alien-tech motif ────────────────────────────────
// Draws a small bright dot at a hex-grid position for atmosphere.
inline void draw_grid_dot(Vector2 pos, float radius, Color color) {
    DrawCircleV(pos, radius, color);
}

// ── Main menu ─────────────────────────────────────────────────────────────────
inline MainMenuResult draw_main_menu_screen(Rectangle viewport) {
    MainMenuResult result;
    const float s = std::clamp(std::min(viewport.width / 1440.0f, viewport.height / 900.0f), 0.90f, 1.18f);
    const Vector2 center = {
        viewport.x + viewport.width  * 0.56f,
        viewport.y + viewport.height * 0.46f
    };

    // ── Background nebula blooms (in addition to draw_background()) ───────────
    // These are foreground glow accents specific to the menu layout.
    DrawCircleGradient(static_cast<int>(viewport.width * 0.56f),
                       static_cast<int>(viewport.height * 0.46f),
                       viewport.height * 0.42f,
                       { 14, 60, 80,  52},
                       {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(viewport.width * 0.10f),
                       static_cast<int>(viewport.height * 0.82f),
                       viewport.height * 0.30f,
                       {100, 38, 10,  42},
                       {0, 0, 0, 0});

    // ── Orbital rings around the visual pendulum centre ───────────────────────
    draw_menu_orbit(center, 118.0f * s, 1.2f * s, { 48, 180, 200,  48}, 8.0f,  336.0f);
    draw_menu_orbit(center, 206.0f * s, 1.0f * s, { 48, 180, 200,  30}, 24.0f, 320.0f);
    draw_menu_orbit(center, 304.0f * s, 0.8f * s, { 48, 180, 200,  18}, 42.0f, 305.0f);

    // Outer diffuse ring
    DrawRing(center, 302.0f * s, 308.0f * s, 0.0f, 360.0f, 80, { 28, 80, 100,  14});

    // Pivot dot with glow
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y),
                       22.0f * s, { 64, 220, 240,  60}, {0, 0, 0, 0});
    DrawCircleV(center, 5.0f * s, WL::CYAN_CORE);

    // ── Decorative hex-grid dots ───────────────────────────────────────────────
    // A sparse grid of very faint dots behind the left panel area — alien terminal feel
    {
        const float grid_step = 42.0f * s;
        const float origin_x  = viewport.x + 30.0f * s;
        const float origin_y  = viewport.y + 80.0f * s;
        for (int row = 0; row < 12; ++row) {
            for (int col = 0; col < 8; ++col) {
                const float ox = (row % 2 == 0) ? 0.0f : grid_step * 0.5f;
                draw_grid_dot(
                    {origin_x + col * grid_step + ox,
                     origin_y + row * grid_step * 0.866f},
                    1.2f * s,
                    { 48, 140, 168,  22});
            }
        }
    }

    // ── Left panel: branding + description ────────────────────────────────────
    const float left_w = std::clamp(viewport.width * 0.36f, 415.0f * s, 555.0f * s);
    const Rectangle intro = {
        viewport.x + 52.0f * s,
        viewport.y + 58.0f * s,
        left_w,
        220.0f * s
    };

    // Super-label
    draw_text("WORLDLINE",
              {intro.x, intro.y},
              13.0f * s,
              with_alpha(WL::CYAN_CORE, 210));

    // Thin separator line under super-label
    DrawLineEx({intro.x, intro.y + 17.0f * s}, {intro.x + 120.0f * s, intro.y + 17.0f * s},
               1.0f, with_alpha(WL::CYAN_DIM, 80));

    // Main title — large, bright
    draw_text("Universe Studio",
              {intro.x, intro.y + 26.0f * s},
              62.0f * s,
              WL::TEXT_PRIMARY);

    draw_text_block(
        "A seed defines everything - space, time, the laws of physics.\nYou can also start with the universe you already live in.",
        {intro.x, intro.y + 106.0f * s, intro.width, 100.0f * s},
        19.0f * s,
        WL::TEXT_SECONDARY,
        5.0f * s);

    // ── Seeded Universe card (primary CTA) ────────────────────────────────────
    const Rectangle seeded = {
        viewport.x + 52.0f * s,
        viewport.y + 292.0f * s,
        std::clamp(viewport.width * 0.45f, 515.0f * s, 720.0f * s),
        280.0f * s
    };

    // Card background with cyan glow border
    draw_card(seeded, {  6, 16, 28, 228}, with_alpha(WL::CYAN_DIM, 130));

    // Interior gradient overlay — top-left tint
    DrawRectangleGradientEx(seeded,
                            { 14, 60, 72,  80},
                            {  8, 18, 30,  12},
                            {  0,  0,  0,   0},
                            {  8, 22, 34,  90});

    // Animated (static) corner accent marks — two L-shaped corner brackets
    auto draw_corner = [&](float cx, float cy, float dx, float dy) {
        const float arm = 12.0f * s;
        DrawLineEx({cx, cy}, {cx + dx * arm, cy},       1.5f, WL::CYAN_CORE);
        DrawLineEx({cx, cy}, {cx, cy + dy * arm},        1.5f, WL::CYAN_CORE);
    };
    draw_corner(seeded.x + 8, seeded.y + 8,   +1, +1);
    draw_corner(seeded.x + seeded.width - 8, seeded.y + 8,   -1, +1);
    draw_corner(seeded.x + 8, seeded.y + seeded.height - 8,  +1, -1);
    draw_corner(seeded.x + seeded.width - 8, seeded.y + seeded.height - 8, -1, -1);

    draw_badge({seeded.x + 20.0f * s, seeded.y + 20.0f * s, 148.0f * s, 26.0f * s},
               "PRIMARY TRACK",
               with_alpha(WL::CYAN_DIM, 200),
               WL::CYAN_CORE,
               s);

    draw_text("Seeded Universe",
              {seeded.x + 20.0f * s, seeded.y + 62.0f * s},
              42.0f * s,
              WL::TEXT_PRIMARY);

    draw_text_block(
        "Generate worlds from deterministic seeds. Explore alternate rule sets and evolve alien physics from a single word.",
        {seeded.x + 20.0f * s, seeded.y + 118.0f * s,
         seeded.width - 40.0f * s, 72.0f * s},
        18.5f * s,
        WL::TEXT_SECONDARY,
        4.0f * s);

    // CTA button — glowing cyan
    if (draw_button(
            {seeded.x + 20.0f * s, seeded.y + seeded.height - 60.0f * s,
             220.0f * s, 40.0f * s},
            "Enter Seeded Universe",
            { 10, 88, 96, 240},
            { 16, 120, 130, 255},
            WL::CYAN_CORE,
            true, s)) {
        result.target = AppScreen::SEEDED_UNIVERSE;
    }

    // ── Default Universe card (secondary) ─────────────────────────────────────
    const float def_w = std::clamp(viewport.width * 0.29f, 330.0f * s, 415.0f * s);
    const Rectangle default_card = {
        viewport.x + viewport.width - def_w - 54.0f * s,
        viewport.y + 234.0f * s,
        def_w,
        338.0f * s
    };

    draw_card(default_card, {  6, 14, 24, 218}, with_alpha(WL::GLASS_BORDER, 100));
    DrawRectangleGradientEx(default_card,
                            { 34, 50, 90,  44},
                            {  6, 12, 22,   8},
                            {  0,  0,  0,   0},
                            {  8, 16, 30,  60});

    draw_text("Default Universe",
              {default_card.x + 20.0f * s, default_card.y + 22.0f * s},
              30.0f * s,
              WL::TEXT_PRIMARY);

    draw_text_block("Standard Newtonian universe.\nKnown laws. Predictable chaos.",
                    {default_card.x + 20.0f * s, default_card.y + 60.0f * s,
                     default_card.width - 40.0f * s, 60.0f * s},
                    17.0f * s,
                    WL::TEXT_SECONDARY,
                    3.5f * s);

    // Inline pendulum preview — ghost rendering
    const Vector2 pp = {
        default_card.x + default_card.width * 0.50f,
        default_card.y + default_card.height * 0.56f
    };
    const Vector2 upper = {pp.x - 52.0f * s, pp.y - 80.0f * s};
    const Vector2 lower = {upper.x + 120.0f * s, upper.y + 18.0f * s};

    // Faint ghost trail
    DrawLineEx(pp, upper,    2.0f * s, { 48, 140, 200,  80});
    DrawLineEx(upper, lower, 2.0f * s, { 48, 140, 200,  80});

    // Pivot
    DrawCircleGradient(static_cast<int>(pp.x), static_cast<int>(pp.y),
                       14.0f * s, { 64, 180, 210, 100}, {0, 0, 0, 0});
    DrawCircleV(pp, 4.0f * s, WL::TEXT_PRIMARY);

    // Bob 1 — cyan
    DrawCircleGradient(static_cast<int>(upper.x), static_cast<int>(upper.y),
                       18.0f * s, {  64, 228, 240, 180}, {0, 0, 0, 0});
    DrawCircleV(upper, 10.0f * s, with_alpha(WL::CYAN_CORE, 220));

    // Bob 2 — xenon orange
    DrawCircleGradient(static_cast<int>(lower.x), static_cast<int>(lower.y),
                       20.0f * s, { 255, 148,  48, 160}, {0, 0, 0, 0});
    DrawCircleV(lower, 11.0f * s, with_alpha(WL::XENON_CORE, 210));

    // Faint sweep ring
    DrawRing(pp, 116.0f * s, 118.0f * s, 0.0f, 360.0f, 72, { 60, 100, 130,  22});

    if (draw_button(
            {default_card.x + 20.0f * s, default_card.y + default_card.height - 58.0f * s,
             default_card.width - 40.0f * s, 38.0f * s},
            "Open Default Universe",
            { 20, 36, 64, 232},
            { 30, 52, 90, 255},
            WL::TEXT_PRIMARY,
            true, s)) {
        result.target = AppScreen::DEFAULT_UNIVERSE;
    }

    // ── Footer credit ──────────────────────────────────────────────────────────
    const std::string credit = "Jerry Li";
    const float credit_size  = 14.0f * s;
    const Vector2 credit_sz  = measure_ui_text(credit, credit_size);
    const Vector2 credit_pos = {
        viewport.x + viewport.width - credit_sz.x - 58.0f * s,
        viewport.y + viewport.height - 54.0f * s
    };
    draw_text("MADE BY", credit_pos, 10.0f * s, WL::TEXT_INACTIVE);
    draw_text(credit, {credit_pos.x, credit_pos.y + 13.0f * s}, credit_size, WL::TEXT_SECONDARY);
    DrawLineEx({credit_pos.x, credit_pos.y + 13.0f * s + credit_sz.y + 4},
               {credit_pos.x + credit_sz.x, credit_pos.y + 13.0f * s + credit_sz.y + 4},
               1.5f * s,
               with_alpha(WL::CYAN_DIM, 140));

    return result;
}

// ── Back-to-menu button ────────────────────────────────────────────────────────
inline bool draw_back_to_menu_button(Rectangle viewport, float scale = 1.0f) {
    return draw_button(
        {viewport.x + 20.0f * scale, viewport.y + 20.0f * scale, 160.0f * scale, 36.0f * scale},
        "< Back",
        { 14, 28, 46, 228},
        { 22, 44, 70, 255},
        WL::TEXT_PRIMARY,
        true, scale);
}

// ── Seeded universe placeholder screen ────────────────────────────────────────
inline bool draw_seeded_universe_screen(Rectangle viewport) {
    const float s = std::clamp(std::min(viewport.width / 1440.0f, viewport.height / 900.0f), 0.90f, 1.18f);
    const Rectangle card = {
        viewport.x + viewport.width  * 0.14f,
        viewport.y + viewport.height * 0.17f,
        viewport.width  * 0.72f,
        viewport.height * 0.58f
    };

    draw_card(card, {  6, 14, 26, 225}, with_alpha(WL::CYAN_DIM, 120));
    DrawRectangleGradientEx(card,
                            { 14, 72, 84,  72},
                            {  8, 16, 26,   8},
                            {  0,  0,  0,   0},
                            { 10, 28, 42, 100});

    // Corner brackets
    auto bracket = [&](float cx, float cy, float dx, float dy) {
        const float arm = 14.0f * s;
        DrawLineEx({cx, cy}, {cx + dx * arm, cy},  1.8f, WL::CYAN_CORE);
        DrawLineEx({cx, cy}, {cx, cy + dy * arm},  1.8f, WL::CYAN_CORE);
    };
    bracket(card.x + 8, card.y + 8,   +1, +1);
    bracket(card.x + card.width - 8, card.y + 8,   -1, +1);
    bracket(card.x + 8, card.y + card.height - 8,  +1, -1);
    bracket(card.x + card.width - 8, card.y + card.height - 8, -1, -1);

    draw_badge({card.x + 22.0f * s, card.y + 22.0f * s, 120.0f * s, 26.0f * s},
               "SEED CORE",
               with_alpha(WL::CYAN_DIM, 200),
               WL::CYAN_CORE, s);

    draw_text("WORLDLINE",
              {card.x + 22.0f * s, card.y + 62.0f * s},
              12.0f * s,
              with_alpha(WL::CYAN_CORE, 170));

    draw_text("Seeded Universe",
              {card.x + 22.0f * s, card.y + 80.0f * s},
              48.0f * s,
              WL::TEXT_PRIMARY);

    // Divider
    DrawLineEx({card.x + 22, card.y + 138.0f * s},
               {card.x + card.width - 22, card.y + 138.0f * s},
               1.0f, with_alpha(WL::CYAN_DIM, 55));

    draw_text_block(
        "This is the primary universe track.  Use it as the home for seed-driven world generation, custom law stacks, and alternate physical regimes.\n\nThe Default Universe remains available as the Newtonian reference lab.",
        {card.x + 22.0f * s, card.y + 150.0f * s,
         card.width - 44.0f * s, card.height - 220.0f * s},
        20.0f * s,
        WL::TEXT_SECONDARY,
        5.0f * s);

    return draw_back_to_menu_button(viewport, s);
}