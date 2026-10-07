#include "ui/GuidedFirstUniverseScene.hpp"

#include "app/SeededUniverseRuntime.hpp"
#include "app/UniverseProject.hpp"
#include "app/WorldlineCopy.hpp"
#include "app/WorldlineStorage.hpp"
#include "ui/TextInput.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <string>

namespace {

// One step of the guided flow: numbered label + short explanation.  Steps light
// up in sequence (`active`) the first time the screen is shown.
void draw_stage_card(Rectangle rect, const char* number, const char* label, const char* body,
                     Color accent, float scale, bool active) {
    draw_card(rect,
              active ? Color{8, 20, 34, 236} : Color{6, 14, 26, 220},
              with_alpha(accent, active ? 120 : 60));
    const float pad = 16.0f * scale;
    // Faint step-number watermark anchors the lower half of tall cards.
    const float mark = std::min(64.0f * scale, rect.height * 0.42f);
    const Vector2 mark_size = measure_ui_text(number, mark);
    draw_text(number, {rect.x + rect.width - mark_size.x - pad, rect.y + rect.height - mark - 8.0f * scale},
              mark, with_alpha(accent, active ? 34 : 18));
    draw_text_fit(label, {rect.x + pad, rect.y + 14.0f * scale}, rect.width - pad * 2.0f,
                  13.0f * scale, 11.0f * scale, with_alpha(accent, active ? 220 : 150));
    DrawLineEx({rect.x + pad, rect.y + 34.0f * scale}, {rect.x + pad + 28.0f * scale, rect.y + 34.0f * scale},
               1.5f, with_alpha(accent, active ? 150 : 70));
    draw_text_block_fit(body,
                        {rect.x + pad, rect.y + 44.0f * scale, rect.width - pad * 2.0f, rect.height - 54.0f * scale},
                        14.5f * scale,
                        active ? WL::TEXT_SECONDARY : with_alpha(WL::TEXT_SECONDARY, 170),
                        4.0f * scale);
}

// Compact fingerprint of a saved universe (thumbnail points are normalised 0..1).
void draw_mini_fingerprint(Rectangle rect, const std::vector<Vec2>& points, Color accent) {
    DrawRectangleRounded(rect, 0.12f, 6, {8, 18, 30, 236});
    DrawRectangleRoundedLines(rect, 0.12f, 6, 1.0f, with_alpha(accent, 70));
    const float inset = std::min(rect.width, rect.height) * 0.14f;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const Vector2 a = {rect.x + inset + static_cast<float>(points[i].x) * (rect.width - inset * 2.0f),
                           rect.y + inset + static_cast<float>(points[i].y) * (rect.height - inset * 2.0f)};
        const Vector2 b = {rect.x + inset + static_cast<float>(points[i + 1].x) * (rect.width - inset * 2.0f),
                           rect.y + inset + static_cast<float>(points[i + 1].y) * (rect.height - inset * 2.0f)};
        DrawLineEx(a, b, 1.4f, with_alpha(accent, 190));
    }
}

// Pill badge sized to its label.
float draw_auto_badge(Vector2 pos, const char* label, Color fill, Color text, float scale) {
    const float h = 24.0f * scale;
    const float w = measure_ui_text(label, 12.5f * scale).x + 26.0f * scale;
    draw_badge({pos.x, pos.y, w, h}, label, fill, text, scale);
    return w;
}

bool is_typing_key(int key) {
    return (key >= KEY_A && key <= KEY_Z) || (key >= KEY_ZERO && key <= KEY_NINE)
        || (key >= KEY_KP_0 && key <= KEY_KP_9) || key == KEY_MINUS || key == KEY_PERIOD;
}

} // namespace

GuidedFirstUniverseSceneResult draw_guided_first_universe_scene(AppState& app,
                                                               Rectangle viewport) {
    GuidedFirstUniverseSceneResult result;
    GuidedFirstUniverseSceneState& guided = app.ui.guided;
    SeededUniverseUiState& seeded = app.ui.seeded;
    guided.reveal_time += GetFrameTime();

    const float s = std::clamp(std::min(viewport.width / 1500.0f, viewport.height / 900.0f), 0.90f, 1.20f);
    const float margin  = 34.0f * s;
    const float col_gap = 30.0f * s;
    const float content_w = std::min(viewport.width - margin * 2.0f, 1520.0f * s);
    const float left_w  = std::floor(content_w * 0.45f);
    const float right_w = content_w - left_w - col_gap;

    // ── Measure the left column so the whole composition can be centred ──────
    const float title_size    = fit_ui_text_size("Universe Studio", left_w, 60.0f * s, 38.0f * s);
    const float subtitle_size = 18.0f * s;
    const float body_size     = 14.5f * s;
    const char* body_copy =
        "Worldline is a deterministic instrument: enter one seed, inspect the generator, "
        "and watch the assembled law drive a live visual system.";
    const float subtitle_h = measure_wrapped_ui_text_height(Copy::kAppSubtitle, left_w, subtitle_size, 5.0f * s);
    const float body_h     = measure_wrapped_ui_text_height(body_copy, left_w, body_size, 4.0f * s);

    const float y_title    = 22.0f * s;
    const float y_subtitle = y_title + title_size + 10.0f * s;
    const float y_body     = y_subtitle + subtitle_h + 14.0f * s;
    const float y_badges   = y_body + body_h + 18.0f * s;
    const float y_action   = y_badges + 24.0f * s + 26.0f * s;

    // Action card contents (relative to the card).
    const float pad      = 20.0f * s;
    const float field_y  = 66.0f * s;
    const float field_h  = 42.0f * s;
    const float hint_y   = field_y + field_h + 8.0f * s;
    const float row_y    = hint_y + 13.0f * s + 14.0f * s;
    const float row_h    = 38.0f * s;
    const float cosmos_y = row_y + row_h + 10.0f * s;
    const float cosmos_h = 40.0f * s;
    const float cosmos_note_size = 13.0f * s;
    const float cosmos_note_h = measure_wrapped_ui_text_height(
        "Opens this seed as a library of objects across scales, from quarks to galaxies, with a live N-body sandbox.",
        left_w - pad * 2.0f, cosmos_note_size, 3.0f * s);
    const float action_h = cosmos_y + cosmos_h + 10.0f * s + cosmos_note_h + pad;

    const float content_h = y_action + action_h;
    const float origin_x = viewport.x + (viewport.width - content_w) * 0.5f;
    const float origin_y = viewport.y + std::max(margin * 0.6f, (viewport.height - content_h) * 0.44f);

    const Rectangle action = {origin_x, origin_y + y_action, left_w, action_h};
    const Rectangle stages = {origin_x + left_w + col_gap, origin_y, right_w, content_h};

    // ── Ambient glow ──────────────────────────────────────────────────────────
    DrawCircleGradient(static_cast<int>(origin_x + left_w * 0.35f),
                       static_cast<int>(origin_y + content_h * 0.30f),
                       viewport.height * 0.36f, {18, 78, 92, 58}, {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(stages.x + stages.width * 0.75f),
                       static_cast<int>(stages.y + stages.height * 0.85f),
                       viewport.height * 0.30f, {110, 48, 18, 36}, {0, 0, 0, 0});

    // ── Brand + intro ─────────────────────────────────────────────────────────
    draw_text("WORLDLINE", {origin_x, origin_y}, 13.5f * s, with_alpha(WL::CYAN_CORE, 210));
    DrawLineEx({origin_x, origin_y + 18.0f * s}, {origin_x + 108.0f * s, origin_y + 18.0f * s},
               1.0f, with_alpha(WL::CYAN_DIM, 90));
    draw_text("Universe Studio", {origin_x, origin_y + y_title}, title_size, WL::TEXT_PRIMARY);
    draw_text_block(Copy::kAppSubtitle, {origin_x, origin_y + y_subtitle, left_w, subtitle_h + 2.0f},
                    subtitle_size, WL::TEXT_SECONDARY, 5.0f * s);
    draw_text_block(body_copy, {origin_x, origin_y + y_body, left_w, body_h + 2.0f},
                    body_size, with_alpha(WL::TEXT_SECONDARY, 175), 4.0f * s);

    float bx = origin_x;
    bx += draw_auto_badge({bx, origin_y + y_badges}, "DETERMINISTIC", {16, 36, 62, 210}, WL::CYAN_CORE, s) + 10.0f * s;
    bx += draw_auto_badge({bx, origin_y + y_badges}, "LOCAL", {18, 30, 50, 210}, WL::TEXT_SECONDARY, s) + 10.0f * s;
    draw_auto_badge({bx, origin_y + y_badges}, "REAL-TIME", with_alpha(WL::PLASMA_DIM, 210), WL::PLASMA_GREEN, s);

    // ── First Universe action card ────────────────────────────────────────────
    draw_card(action, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 120));
    draw_corner_brackets(action, with_alpha(WL::CYAN_CORE, 120), 11.0f * s, 1.4f, 5.0f * s);
    draw_text("First Universe", {action.x + pad, action.y + 18.0f * s}, 22.0f * s, WL::TEXT_PRIMARY);
    draw_text("Start with a seed. Any text works.", {action.x + pad, action.y + 44.0f * s},
              13.0f * s, with_alpha(WL::CYAN_CORE, 190));

    const Rectangle seed_field = {action.x + pad, action.y + field_y, action.width - pad * 2.0f, field_h};

    // Type-to-focus: starting to type anywhere on this screen edits the seed
    // (replacing it, like clicking the field does).  The key queue is separate
    // from the character queue, so the first character still reaches the field.
    if (!seeded.input_active) {
        for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
            if (is_typing_key(key)) {
                seeded.input_active = true;
                seeded.input_select_all = true;
                seeded.backspace_repeat_timer = 0.0f;
                break;
            }
        }
    }
    if (draw_text_field(seed_field, seeded.seed_input, "Type a seed, e.g. aurora",
                        seeded.input_active, seeded.input_select_all, WL::CYAN_CORE, s)) {
        seeded.input_active = true;
        seeded.input_select_all = true;
        seeded.backspace_repeat_timer = 0.0f;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), seed_field)) {
        seeded.input_active = false;
        seeded.input_select_all = false;
    }
    if (seeded.input_active && IsKeyPressed(KEY_ESCAPE)) {
        seeded.input_active = false;
        seeded.input_select_all = false;
    }
    handle_text_input(seeded.seed_input, seeded.input_active, seeded.input_select_all, seeded.backspace_repeat_timer, 128u);

    const bool has_seed = !seeded.seed_input.empty();
    draw_text_fit(has_seed ? "Enter generates  |  the same seed always builds the same universe"
                           : "Type a seed to generate a universe",
                  {seed_field.x + 2.0f * s, action.y + hint_y}, seed_field.width,
                  12.5f * s, 10.5f * s, with_alpha(WL::TEXT_SECONDARY, 150));

    const float button_gap = 10.0f * s;
    const float generate_w = std::floor((seed_field.width - button_gap * 2.0f) * 0.38f);
    const float other_w    = (seed_field.width - button_gap * 2.0f - generate_w) * 0.5f;
    const bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    if ((draw_button({seed_field.x, action.y + row_y, generate_w, row_h},
                     "Generate",
                     {10, 84, 98, 240},
                     {18, 126, 140, 255},
                     WL::CYAN_CORE,
                     has_seed,
                     s)
         || (enter && has_seed))) {
        regenerate_seeded_universe(seeded);
        seeded.input_active = false;
        seeded.input_select_all = false;
        guided.completed_intro = true;
        result.open_workspace = true;
    }
    if (draw_button({seed_field.x + generate_w + button_gap, action.y + row_y, other_w, row_h},
                    Copy::kAtlasLabel,
                    {18, 34, 62, 230},
                    {28, 54, 92, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    s)) {
        result.open_atlas = true;
    }
    if (draw_button({seed_field.x + generate_w + other_w + button_gap * 2.0f, action.y + row_y, other_w, row_h},
                    Copy::kReferenceLabel,
                    {26, 30, 48, 228},
                    {38, 46, 70, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    s)) {
        result.open_reference = true;
    }

    if (draw_button({seed_field.x, action.y + cosmos_y, seed_field.width, cosmos_h},
                    "Explore the Cosmos \xE2\x80\x94 multi-scale universe",
                    {28, 18, 64, 235},
                    {44, 28, 96, 255},
                    WL::VIOLET_CORE,
                    true,
                    s)) {
        result.open_cosmos = true;
    }
    draw_text_block("Opens this seed as a library of objects across scales, from quarks to galaxies, with a live N-body sandbox.",
                    {seed_field.x, action.y + cosmos_y + cosmos_h + 10.0f * s, seed_field.width, cosmos_note_h + 2.0f},
                    cosmos_note_size, WL::TEXT_TERTIARY, 3.0f * s);

    // ── Guided flow card ──────────────────────────────────────────────────────
    draw_card(stages, {6, 14, 24, 220}, with_alpha(WL::GLASS_BORDER, 110));
    const float spad = 20.0f * s;
    draw_text("Guided Flow", {stages.x + spad, stages.y + 18.0f * s}, 20.0f * s, WL::TEXT_PRIMARY);
    draw_text("From seed to live workspace", {stages.x + spad, stages.y + 44.0f * s},
              13.0f * s, with_alpha(WL::CYAN_CORE, 180));

    // Saved-universe count doubles as a shortcut into the Atlas.
    const std::size_t saved = app.catalog.projects.size();
    if (saved > 0) {
        const std::string label = std::to_string(saved) + (saved == 1 ? " saved universe  >" : " saved universes  >");
        const float lw = measure_ui_text(label, 13.0f * s).x + 24.0f * s;
        if (draw_button({stages.x + stages.width - spad - lw, stages.y + 18.0f * s, lw, 30.0f * s},
                        label.c_str(), {14, 30, 50, 200}, {22, 48, 76, 255}, WL::TEXT_SECONDARY, true, s * 0.72f)) {
            result.open_atlas = true;
        }
    } else {
        const char* none = "No saved universes yet";
        draw_text(none, {stages.x + stages.width - spad - measure_ui_text(none, 12.5f * s).x, stages.y + 26.0f * s},
                  12.5f * s, WL::TEXT_TERTIARY);
    }

    const bool has_recent = saved > 0;
    const float recent_h  = 64.0f * s;
    const float empty_row_h = 58.0f * s;
    const float recent_block = has_recent ? recent_h + 28.0f * s : empty_row_h + 14.0f * s;
    const float grid_top  = stages.y + 76.0f * s;
    const float grid_bottom = stages.y + stages.height - spad - recent_block;
    const float card_gap  = 12.0f * s;
    const float card_w    = (stages.width - spad * 2.0f - card_gap) * 0.5f;
    const float card_h    = std::max(96.0f * s, (grid_bottom - grid_top - card_gap) * 0.5f);
    const float gx0 = stages.x + spad;
    const float gx1 = gx0 + card_w + card_gap;
    draw_stage_card({gx0, grid_top, card_w, card_h}, "01",
                    "01  SEED",
                    "The input string is the only choice you make. Everything after it is deterministic.",
                    WL::CYAN_CORE, s, guided.reveal_time >= 0.0f);
    draw_stage_card({gx1, grid_top, card_w, card_h}, "02",
                    "02  GENERATOR",
                    "The seed expands through a self-modifying machine that assembles the tensors defining the law.",
                    WL::VIOLET_CORE, s, guided.reveal_time >= 0.3f);
    draw_stage_card({gx0, grid_top + card_h + card_gap, card_w, card_h}, "03",
                    "03  LAW ASSEMBLY",
                    "LawSpec builds the equation of motion; ObservableExtractor maps the hidden state onto visible pendulum motion.",
                    WL::PLASMA_GREEN, s, guided.reveal_time >= 0.6f);
    draw_stage_card({gx1, grid_top + card_h + card_gap, card_w, card_h}, "04",
                    "04  WORKSPACE",
                    "The live workspace renders the law in real time, adds a timeline, and keeps the full trace one click away.",
                    WL::XENON_CORE, s, guided.reveal_time >= 0.9f);

    if (!has_recent) {
        // Empty state in the same slot the resume row will occupy, so the
        // layout does not jump once the first universe is saved.
        const float ry = stages.y + stages.height - spad - empty_row_h;
        const Rectangle row = {gx0, ry, stages.width - spad * 2.0f, empty_row_h};
        DrawRectangleRounded(row, 0.12f, 8, {8, 16, 28, 150});
        DrawRectangleRoundedLines(row, 0.12f, 8, 1.0f, with_alpha(WL::GLASS_BORDER, 70));
        draw_text_fit("Saved universes appear here.", {row.x + 16.0f * s, row.y + 12.0f * s},
                      row.width - 32.0f * s, 15.0f * s, 12.0f * s, WL::TEXT_SECONDARY);
        draw_text_fit("Generate one, then press Save in the workspace to keep it in the Atlas.",
                      {row.x + 16.0f * s, row.y + 34.0f * s}, row.width - 32.0f * s,
                      12.5f * s, 10.5f * s, WL::TEXT_TERTIARY);
    } else {
        const UniverseProject& project = app.catalog.projects.front();
        const float ry = stages.y + stages.height - spad - recent_h;
        draw_text("CONTINUE WHERE YOU LEFT OFF", {gx0, ry - 22.0f * s}, 11.5f * s, with_alpha(WL::CYAN_CORE, 175));

        const Rectangle row = {gx0, ry, stages.width - spad * 2.0f, recent_h};
        const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        if (hot) ui_request_cursor(MOUSE_CURSOR_POINTING_HAND);
        const Color accent = project.dynamic_p ? WL::VIOLET_CORE : WL::CYAN_CORE;
        if (hot) {
            draw_card_accented(row, {14, 32, 52, 244}, with_alpha(accent, 170), accent);
        } else {
            draw_card(row, {8, 18, 30, 232}, with_alpha(accent, 80));
        }
        draw_mini_fingerprint({row.x + 10.0f * s, row.y + 9.0f * s, 64.0f * s, row.height - 18.0f * s},
                              project.thumbnail_points, accent);

        const float tx = row.x + 88.0f * s;
        const float open_w = 92.0f * s;
        const float text_w = row.x + row.width - open_w - 22.0f * s - tx;
        draw_text_fit(project.title.empty() ? project.seed : project.title, {tx, row.y + 12.0f * s}, text_w,
                      18.0f * s, 14.0f * s, WL::TEXT_PRIMARY);
        const std::string meta = "seed \"" + project.seed + "\"  |  " + (project.dynamic_p ? "dynamic p" : "seed-locked p");
        draw_text_fit(meta, {tx, row.y + 37.0f * s}, text_w, 12.5f * s, 11.0f * s, WL::TEXT_TERTIARY);

        const Rectangle open = {row.x + row.width - open_w - 12.0f * s, row.y + (row.height - 34.0f * s) * 0.5f,
                                open_w, 34.0f * s};
        const bool open_clicked = draw_button(open, "Open", {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, s);
        if (open_clicked || (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
            apply_universe_project(seeded, project);
            app.settings.last_project_id = project.id;
            seeded.input_active = false;
            result.open_workspace = true;
        }
    }

    // ── Footer: keyboard affordances ──────────────────────────────────────────
    const char* footer = "Type to enter a seed   |   Enter: generate   |   Esc: leave a field";
    const float fw = measure_ui_text(footer, 12.0f * s).x;
    draw_text(footer, {viewport.x + (viewport.width - fw) * 0.5f, viewport.y + viewport.height - 26.0f * s},
              12.0f * s, with_alpha(WL::TEXT_TERTIARY, 150));

    return result;
}
