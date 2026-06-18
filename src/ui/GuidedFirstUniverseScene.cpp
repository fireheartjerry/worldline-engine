#include "ui/GuidedFirstUniverseScene.hpp"

#include "app/SeededUniverseRuntime.hpp"
#include "app/WorldlineCopy.hpp"
#include "app/WorldlineStorage.hpp"
#include "ui/TextInput.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>

namespace {

void draw_stage_card(Rectangle rect, const char* label, const char* body, Color accent, float scale, bool active) {
    draw_card(rect,
              active ? Color{8, 20, 34, 236} : Color{6, 14, 26, 220},
              with_alpha(accent, active ? 120 : 70));
    draw_text(label,
              {rect.x + 14.0f * scale, rect.y + 14.0f * scale},
              12.0f * scale,
              with_alpha(accent, 190));
    draw_text_block(body,
                    {rect.x + 14.0f * scale, rect.y + 34.0f * scale, rect.width - 28.0f * scale, rect.height - 44.0f * scale},
                    14.0f * scale,
                    WL::TEXT_SECONDARY,
                    3.0f * scale);
}

} // namespace

GuidedFirstUniverseSceneResult draw_guided_first_universe_scene(AppState& app,
                                                               Rectangle viewport) {
    GuidedFirstUniverseSceneResult result;
    GuidedFirstUniverseSceneState& guided = app.ui.guided;
    SeededUniverseUiState& seeded = app.ui.seeded;
    guided.reveal_time += GetFrameTime();

    const float scale = std::clamp(std::min(viewport.width / 1500.0f, viewport.height / 920.0f), 0.92f, 1.16f);
    const float margin = 34.0f * scale;
    const float top = viewport.y + 40.0f * scale;
    const float left_w = viewport.width * 0.44f;
    const float right_w = viewport.width - left_w - 20.0f * scale;
    const Rectangle intro = {viewport.x + margin, top, left_w - margin, 250.0f * scale};
    const Rectangle action = {viewport.x + margin, intro.y + intro.height + 18.0f * scale, left_w - margin, 264.0f * scale};
    const Rectangle stages = {viewport.x + left_w, top, right_w - margin, 470.0f * scale};

    DrawCircleGradient(static_cast<int>(viewport.x + viewport.width * 0.16f),
                       static_cast<int>(viewport.y + viewport.height * 0.24f),
                       viewport.height * 0.34f,
                       {18, 78, 92, 58},
                       {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(viewport.x + viewport.width * 0.84f),
                       static_cast<int>(viewport.y + viewport.height * 0.72f),
                       viewport.height * 0.30f,
                       {110, 48, 18, 36},
                       {0, 0, 0, 0});

    draw_text("WORLDLINE",
              {intro.x, intro.y},
              13.0f * scale,
              with_alpha(WL::CYAN_CORE, 200));
    draw_text("Universe Studio",
              {intro.x, intro.y + 24.0f * scale},
              60.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text_block(Copy::kAppSubtitle,
                    {intro.x, intro.y + 98.0f * scale, intro.width - 40.0f * scale, 78.0f * scale},
                    18.0f * scale,
                    WL::TEXT_SECONDARY,
                    4.0f * scale);
    draw_text_block(
        "Worldline is built as a deterministic instrument. Enter one seed, inspect the generator, and watch the assembled law drive a live visual system.",
        {intro.x, intro.y + 182.0f * scale, intro.width - 40.0f * scale, 80.0f * scale},
        14.0f * scale,
        WL::TEXT_TERTIARY,
        3.0f * scale);
    draw_badge({intro.x, intro.y + 252.0f * scale, 112.0f * scale, 24.0f * scale},
               "DETERMINISTIC",
               {16, 36, 62, 210},
               WL::CYAN_CORE,
               scale);
    draw_badge({intro.x + 122.0f * scale, intro.y + 252.0f * scale, 74.0f * scale, 24.0f * scale},
               "LOCAL",
               {18, 30, 50, 210},
               WL::TEXT_SECONDARY,
               scale);
    draw_badge({intro.x + 206.0f * scale, intro.y + 252.0f * scale, 98.0f * scale, 24.0f * scale},
               "REAL-TIME",
               with_alpha(WL::PLASMA_DIM, 210),
               WL::PLASMA_GREEN,
               scale);

    draw_card(action, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 110));
    draw_text("First Universe",
              {action.x + 16.0f * scale, action.y + 14.0f * scale},
              20.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text("Start with a seed",
              {action.x + 16.0f * scale, action.y + 38.0f * scale},
              12.0f * scale,
              with_alpha(WL::CYAN_CORE, 180));

    const Rectangle seed_field = {action.x + 16.0f * scale, action.y + 62.0f * scale, action.width - 32.0f * scale, 38.0f * scale};
    if (draw_text_field(seed_field, seeded.seed_input, "Enter a seed", seeded.input_active, WL::CYAN_CORE, scale)) {
        seeded.input_active = true;
        seeded.input_select_all = true;
        seeded.backspace_repeat_timer = 0.0f;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), seed_field)) {
        seeded.input_active = false;
        seeded.input_select_all = false;
    }
    handle_text_input(seeded.seed_input, seeded.input_active, seeded.input_select_all, seeded.backspace_repeat_timer, 128u);

    const float button_y = seed_field.y + seed_field.height + 14.0f * scale;
    const float button_gap = 10.0f * scale;
    const float button_w = (seed_field.width - button_gap * 2.0f) / 3.0f;
    if (draw_button({seed_field.x, button_y, button_w, 34.0f * scale},
                    "Generate",
                    {10, 84, 98, 240},
                    {18, 126, 140, 255},
                    WL::CYAN_CORE,
                    true,
                    scale)
        || (seeded.input_active && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))) {
        regenerate_seeded_universe(seeded);
        guided.completed_intro = true;
        result.open_workspace = true;
    }
    if (draw_button({seed_field.x + button_w + button_gap, button_y, button_w, 34.0f * scale},
                    Copy::kAtlasLabel,
                    {18, 34, 62, 230},
                    {28, 54, 92, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale)) {
        result.open_atlas = true;
    }
    if (draw_button({seed_field.x + (button_w + button_gap) * 2.0f, button_y, button_w, 34.0f * scale},
                    Copy::kReferenceLabel,
                    {26, 30, 48, 228},
                    {38, 46, 70, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale)) {
        result.open_reference = true;
    }

    if (draw_button({seed_field.x, button_y + 44.0f * scale, seed_field.width, 36.0f * scale},
                    "Explore the Cosmos  -  multi-scale universe",
                    {28, 18, 64, 235},
                    {44, 28, 96, 255},
                    WL::VIOLET_CORE,
                    true,
                    scale)) {
        result.open_cosmos = true;
    }

    draw_text_block(
        "Cosmos opens this seed as a library of objects across scales - quarks to galaxies - with a live N-body sandbox.",
        {action.x + 16.0f * scale, button_y + 86.0f * scale, action.width - 32.0f * scale, 44.0f * scale},
        12.5f * scale,
        WL::TEXT_TERTIARY,
        3.0f * scale);

    draw_card(stages, {6, 14, 24, 220}, with_alpha(WL::GLASS_BORDER, 110));
    draw_text("Guided Flow",
              {stages.x + 18.0f * scale, stages.y + 16.0f * scale},
              18.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text("Seed to live workspace",
              {stages.x + 18.0f * scale, stages.y + 38.0f * scale},
              12.0f * scale,
              with_alpha(WL::CYAN_CORE, 170));

    const float card_gap = 12.0f * scale;
    const float card_w = (stages.width - 18.0f * scale * 2.0f - card_gap) * 0.5f;
    const float card_h = 160.0f * scale;
    const float row_y = stages.y + 68.0f * scale;
    draw_stage_card({stages.x + 18.0f * scale, row_y, card_w, card_h},
                    "01 Seed",
                    "The input string is the only user choice. Everything else is deterministic.",
                    WL::CYAN_CORE,
                    scale,
                    guided.reveal_time >= 0.0f);
    draw_stage_card({stages.x + 18.0f * scale + card_w + card_gap, row_y, card_w, card_h},
                    "02 Generator",
                    "The seed expands through the self-modifying machine and assembles the tensors that define the generated law.",
                    WL::VIOLET_CORE,
                    scale,
                    guided.reveal_time >= 0.3f);
    draw_stage_card({stages.x + 18.0f * scale, row_y + card_h + card_gap, card_w, card_h},
                    "03 Law Assembly",
                    "LawSpec builds the equation of motion and ObservableExtractor maps the generated state into visible pendulum motion.",
                    WL::PLASMA_GREEN,
                    scale,
                    guided.reveal_time >= 0.6f);
    draw_stage_card({stages.x + 18.0f * scale + card_w + card_gap, row_y + card_h + card_gap, card_w, card_h},
                    "04 Workspace",
                    "The live workspace keeps the renderer quality, adds a timeline, and keeps detailed trace information one step away.",
                    WL::XENON_CORE,
                    scale,
                    guided.reveal_time >= 0.9f);

    draw_text(std::to_string(app.catalog.projects.size()) + " saved universe(s)",
              {stages.x + stages.width - 186.0f * scale, stages.y + 18.0f * scale},
              11.0f * scale,
              WL::TEXT_TERTIARY);

    if (!app.catalog.projects.empty()) {
        const UniverseProject& project = app.catalog.projects.front();
        draw_text("Recent project",
                  {stages.x + 18.0f * scale, stages.y + stages.height - 62.0f * scale},
                  11.0f * scale,
                  with_alpha(WL::CYAN_CORE, 165));
        draw_text(project.title.empty() ? project.seed : project.title,
                  {stages.x + 18.0f * scale, stages.y + stages.height - 40.0f * scale},
                  18.0f * scale,
                  WL::TEXT_PRIMARY);
        draw_text(project.dynamic_p ? "Dynamic p" : "Seed-locked p",
                  {stages.x + 18.0f * scale, stages.y + stages.height - 18.0f * scale},
                  13.0f * scale,
                  WL::TEXT_SECONDARY);
    }

    return result;
}
