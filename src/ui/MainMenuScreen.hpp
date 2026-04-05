#pragma once
#include "UiPrimitives.hpp"
#include "../app/AppTypes.hpp"

struct MainMenuResult {
    AppScreen target = AppScreen::MAIN_MENU;
};

inline void draw_menu_orbit(Vector2 center,
                            float radius,
                            float thickness,
                            Color color,
                            float start_deg,
                            float end_deg) {
    DrawRing(center, radius - thickness, radius, start_deg, end_deg, 64, color);
}

inline MainMenuResult draw_main_menu_screen(Rectangle viewport) {
    MainMenuResult result;
    const float scale = std::clamp(std::min(viewport.width / 1440.0f, viewport.height / 900.0f), 0.92f, 1.18f);
    const Vector2 center = {
        viewport.x + viewport.width * 0.54f,
        viewport.y + viewport.height * 0.47f
    };

    DrawCircleGradient(static_cast<int>(viewport.width * 0.74f),
                       static_cast<int>(viewport.height * 0.20f),
                       viewport.height * 0.30f,
                       {44, 122, 126, 70},
                       {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(viewport.width * 0.22f),
                       static_cast<int>(viewport.height * 0.78f),
                       viewport.height * 0.34f,
                       {188, 112, 44, 54},
                       {0, 0, 0, 0});

    draw_menu_orbit(center, 122.0f * scale, 2.5f * scale, {78, 111, 129, 55}, 12.0f, 340.0f);
    draw_menu_orbit(center, 214.0f * scale, 2.5f * scale, {78, 111, 129, 42}, 30.0f, 322.0f);
    draw_menu_orbit(center, 312.0f * scale, 2.0f * scale, {78, 111, 129, 28}, 48.0f, 304.0f);
    DrawCircleV(center, 6.5f * scale, {232, 244, 248, 255});
    DrawCircleGradient(static_cast<int>(center.x),
                       static_cast<int>(center.y),
                       178.0f * scale,
                       {28, 74, 90, 36},
                       {0, 0, 0, 0});

    const float left_w = std::clamp(viewport.width * 0.36f, 420.0f * scale, 560.0f * scale);
    const Rectangle intro = {
        viewport.x + 56.0f * scale,
        viewport.y + 64.0f * scale,
        left_w,
        220.0f * scale
    };
    draw_text("Worldline", {intro.x, intro.y}, 30.0f * scale, {125, 209, 218, 255});
    draw_text("Universe Studio", {intro.x, intro.y + 36.0f * scale}, 66.0f * scale, {238, 244, 248, 255});
    draw_text_block("A seed defines everything: space, time, the laws of physics. Or start with the universe you already live in.",
                    {intro.x, intro.y + 120.0f * scale, intro.width, 100.0f * scale},
                    21.0f * scale,
                    {166, 198, 213, 230},
                    5.0f * scale);

    const Rectangle seeded = {
        viewport.x + 56.0f * scale,
        viewport.y + 296.0f * scale,
        std::clamp(viewport.width * 0.46f, 520.0f * scale, 730.0f * scale),
        276.0f * scale
    };
    draw_card(seeded, {10, 21, 31, 220}, {80, 182, 185, 150});
    DrawRectangleGradientEx({seeded.x, seeded.y, seeded.width, seeded.height},
                            {18, 66, 72, 86},
                            {12, 22, 30, 18},
                            {0, 0, 0, 0},
                            {10, 24, 34, 126});
    draw_badge({seeded.x + 22.0f * scale, seeded.y + 22.0f * scale, 148.0f * scale, 30.0f * scale},
               "PRIMARY FOCUS",
               {22, 96, 100, 230},
               {232, 247, 248, 255},
               scale);
    draw_text("Seeded Universe",
              {seeded.x + 22.0f * scale, seeded.y + 72.0f * scale},
              44.0f * scale,
              {241, 247, 249, 255});
    draw_text_block("Generate worlds from deterministic seeds, explore alternate rule sets, and evolve alien physics from a mere word.",
                    {seeded.x + 22.0f * scale, seeded.y + 128.0f * scale, seeded.width - 44.0f * scale, 74.0f * scale},
                    20.0f * scale,
                    {178, 209, 221, 228},
                    4.0f * scale);
    if (draw_button({seeded.x + 22.0f * scale, seeded.y + seeded.height - 66.0f * scale, 230.0f * scale, 42.0f * scale},
                    "Enter Seeded Universe",
                    {20, 114, 112, 245},
                    {28, 142, 140, 255},
                    {236, 248, 248, 255},
                    true,
                    scale)) {
        result.target = AppScreen::SEEDED_UNIVERSE;
    }

    const Rectangle default_card = {
        viewport.x + viewport.width - std::clamp(viewport.width * 0.30f, 336.0f * scale, 420.0f * scale) - 58.0f * scale,
        viewport.y + 238.0f * scale,
        std::clamp(viewport.width * 0.30f, 336.0f * scale, 420.0f * scale),
        334.0f * scale
    };
    draw_card(default_card, {8, 16, 24, 214}, {93, 118, 155, 110});
    draw_text("Default Universe",
              {default_card.x + 22.0f * scale, default_card.y + 24.0f * scale},
              33.0f * scale,
              {239, 244, 248, 255});
    draw_text_block("The standard Newtonian universe with regular laws.",
                    {default_card.x + 22.0f * scale, default_card.y + 68.0f * scale, default_card.width - 44.0f * scale, 58.0f * scale},
                    18.0f * scale,
                    {171, 194, 208, 230},
                    3.0f * scale);

    const Vector2 preview_pivot = {
        default_card.x + default_card.width * 0.50f,
        default_card.y + default_card.height * 0.56f
    };
    const Vector2 upper = {preview_pivot.x - 54.0f * scale, preview_pivot.y - 82.0f * scale};
    const Vector2 lower = {upper.x + 124.0f * scale, upper.y + 20.0f * scale};
    DrawLineEx(preview_pivot, upper, 4.0f * scale, {196, 216, 236, 230});
    DrawLineEx(upper, lower, 4.0f * scale, {196, 216, 236, 230});
    DrawCircleGradient(static_cast<int>(upper.x), static_cast<int>(upper.y), 14.0f * scale, {120, 228, 255, 255}, {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(lower.x), static_cast<int>(lower.y), 15.0f * scale, {255, 190, 98, 255}, {0, 0, 0, 0});
    DrawCircleV(preview_pivot, 4.5f * scale, {236, 244, 248, 255});
    DrawRing(preview_pivot, 118.0f * scale, 120.0f * scale, 0.0f, 360.0f, 72, {88, 108, 126, 34});
    if (draw_button({default_card.x + 22.0f * scale, default_card.y + default_card.height - 62.0f * scale, default_card.width - 44.0f * scale, 40.0f * scale},
                    "Open Default Universe",
                    {30, 48, 72, 235},
                    {40, 63, 93, 255},
                    {231, 239, 247, 255},
                    true,
                    scale)) {
        result.target = AppScreen::DEFAULT_UNIVERSE;
    }

    const std::string credit = "Made by Jerry Li";
    const float credit_size = 28.0f * scale;
    const Vector2 credit_dims = measure_ui_text(credit, credit_size);
    const Vector2 credit_pos = {
        viewport.x + viewport.width - credit_dims.x - 62.0f * scale,
        viewport.y + viewport.height - 60.0f * scale
    };
    draw_text(credit, credit_pos, credit_size, {236, 241, 247, 230});
    DrawLineEx({credit_pos.x, credit_pos.y + credit_dims.y + 8.0f * scale},
               {credit_pos.x + credit_dims.x, credit_pos.y + credit_dims.y + 8.0f * scale},
               2.5f * scale,
               {109, 212, 218, 180});

    return result;
}

inline bool draw_back_to_menu_button(Rectangle viewport,
                                     float scale = 1.0f) {
    return draw_button({viewport.x + 24.0f * scale, viewport.y + 24.0f * scale, 170.0f * scale, 38.0f * scale},
                       "Back to Menu",
                       {24, 40, 58, 226},
                       {35, 57, 80, 255},
                       {233, 240, 247, 255},
                       true,
                       scale);
}

inline bool draw_seeded_universe_screen(Rectangle viewport) {
    const float scale = std::clamp(std::min(viewport.width / 1440.0f, viewport.height / 900.0f), 0.92f, 1.18f);
    const Rectangle card = {
        viewport.x + viewport.width * 0.14f,
        viewport.y + viewport.height * 0.18f,
        viewport.width * 0.72f,
        viewport.height * 0.56f
    };
    draw_card(card, {9, 19, 28, 220}, {82, 182, 186, 120});
    DrawRectangleGradientEx(card,
                            {18, 86, 88, 70},
                            {10, 18, 26, 10},
                            {0, 0, 0, 0},
                            {14, 34, 44, 110});
    draw_badge({card.x + 24.0f * scale, card.y + 24.0f * scale, 132.0f * scale, 28.0f * scale},
               "SEED CORE",
               {18, 104, 108, 228},
               {235, 247, 248, 255},
               scale);
    draw_text("Seeded Universe",
              {card.x + 24.0f * scale, card.y + 78.0f * scale},
              52.0f * scale,
              {240, 246, 249, 255});
    draw_text_block("This is the primary universe track. Use it as the future home for seed-driven world generation, custom law stacks, and alternate physical regimes.",
                    {card.x + 24.0f * scale, card.y + 146.0f * scale, card.width - 48.0f * scale, 116.0f * scale},
                    22.0f * scale,
                    {179, 210, 221, 228},
                    5.0f * scale);
    draw_text_block("The Default Universe remains available as the Newtonian reference lab.",
                    {card.x + 24.0f * scale, card.y + card.height - 96.0f * scale, card.width - 48.0f * scale, 54.0f * scale},
                    18.0f * scale,
                    {153, 187, 201, 218},
                    4.0f * scale);
    return draw_back_to_menu_button(viewport, scale);
}
