#include "ui/CosmosExplorerScene.hpp"

#include "app/WorldlineStorage.hpp"
#include "cosmos/Analysis.hpp"
#include "cosmos/Sandbox.hpp"
#include "renderer/Renderer.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace cosmos;

namespace {

constexpr double kWorldHalf = 12.0;

std::string fmt_sci(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.2e", v);
    return buf;
}

std::string fmt_fixed(double v, int precision) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, v);
    return buf;
}

bool clicked(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool right_clicked(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
}

Color to_raylib(Color8 c) {
    return Color{c.r, c.g, c.b, 255};
}

// Jump the library to a specific object by id (used by constituent navigation),
// switching tiers if needed.
void select_object_by_id(CosmosState& cosmos, const std::string& id) {
    const UniverseObject* target = find_object(cosmos.catalog, id);
    if (target == nullptr) {
        return;
    }
    cosmos.set_scale(target->scale);
    const auto objs = objects_for_scale(cosmos.catalog, target->scale);
    for (std::size_t i = 0; i < objs.size(); ++i) {
        if (objs[i]->id == id) {
            cosmos.selected_object = static_cast<int>(i);
            break;
        }
    }
}

void restore_bookmark(AppState& app, CosmosState& cosmos, const CosmosBookmark& b);

void draw_browser_modal(AppState& app, CosmosState& cosmos, Rectangle viewport, float scale) {
    // Dim everything behind the modal.
    DrawRectangle(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                  static_cast<int>(viewport.width), static_cast<int>(viewport.height),
                  {2, 4, 9, 200});

    const float w = std::min(viewport.width * 0.5f, 620.0f * scale);
    const float h = std::min(viewport.height * 0.7f, 520.0f * scale);
    const Rectangle modal = {viewport.x + (viewport.width - w) * 0.5f,
                             viewport.y + (viewport.height - h) * 0.5f, w, h};
    draw_card(modal, {7, 14, 26, 248}, with_alpha(WL::VIOLET_CORE, 150));
    draw_text("SAVED SANDBOXES", {modal.x + 18.0f * scale, modal.y + 16.0f * scale},
              17.0f * scale, WL::TEXT_PRIMARY);

    // Close button.
    const Rectangle close = {modal.x + modal.width - 38.0f * scale, modal.y + 14.0f * scale,
                             24.0f * scale, 24.0f * scale};
    if (draw_button(close, "x", {30, 18, 40, 235}, {60, 30, 70, 255}, WL::TEXT_PRIMARY, true, scale) ||
        IsKeyPressed(KEY_ESCAPE)) {
        cosmos.browser_open = false;
    }

    const std::vector<CosmosBookmark> marks = Storage::load_cosmos_bookmarks();
    if (marks.empty()) {
        draw_text_block("No saved sandboxes yet. Spawn a scale and press Save to bookmark it - it "
                        "reopens to the exact same evolved state.",
                        {modal.x + 18.0f * scale, modal.y + 56.0f * scale, modal.width - 36.0f * scale,
                         80.0f * scale},
                        14.0f * scale, WL::TEXT_TERTIARY, 4.0f * scale);
        return;
    }

    const float list_top = modal.y + 50.0f * scale;
    const float row_h = 54.0f * scale;
    const int max_rows = static_cast<int>((modal.height - 62.0f * scale) / row_h);
    const int shown = std::min(static_cast<int>(marks.size()), max_rows);
    for (int i = 0; i < shown; ++i) {
        const CosmosBookmark& b = marks[static_cast<std::size_t>(i)];
        const Rectangle row = {modal.x + 14.0f * scale, list_top + row_h * i,
                               modal.width - 28.0f * scale, row_h - 8.0f * scale};
        const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRounded(row, 0.12f, 6, hot ? Color{14, 30, 50, 240} : Color{8, 18, 32, 210});
        DrawRectangleRoundedLines(row, 0.12f, 6, 1.0f, with_alpha(WL::GLASS_BORDER, 130));

        const int idx = std::clamp(b.scale_index, 0, static_cast<int>(kScaleCount) - 1);
        draw_text(b.title.empty() ? b.seed : b.title,
                  {row.x + 12.0f * scale, row.y + 7.0f * scale}, 15.0f * scale, WL::TEXT_PRIMARY);
        const std::string sub = std::string(scale_ladder()[idx].name) + "  -  seed '" + b.seed +
                                "'  -  " + std::to_string(b.steps) + " steps  -  " + b.created_at;
        draw_text(sub, {row.x + 12.0f * scale, row.y + 28.0f * scale}, 12.0f * scale,
                  WL::TEXT_TERTIARY);

        // Delete button on the right.
        const Rectangle del = {row.x + row.width - 30.0f * scale, row.y + 8.0f * scale,
                               22.0f * scale, 22.0f * scale};
        if (draw_button(del, "x", {44, 16, 20, 235}, {90, 26, 30, 255}, WL::TEXT_SECONDARY, true,
                        scale)) {
            Storage::delete_cosmos_bookmark(b.id);
            continue;
        }
        // Clicking the row (but not the delete button) restores the sandbox.
        if (CheckCollisionPointRec(GetMousePosition(), row) &&
            !CheckCollisionPointRec(GetMousePosition(), del) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            restore_bookmark(app, cosmos, b);
            cosmos.browser_open = false;
        }
    }
}

void save_current_sandbox(const CosmosState& cosmos) {
    CosmosBookmark b;
    b.seed = cosmos.seed;
    b.scale_index = static_cast<int>(scale_index(cosmos.scale));
    b.steps = cosmos.step_count;
    b.id = Storage::make_project_id(cosmos.seed) + "-s" + std::to_string(b.scale_index);
    b.title = std::string(tier_for(cosmos.scale).name) + " @ " + cosmos.seed;
    b.created_at = Storage::now_timestamp();
    Storage::save_cosmos_bookmark(b);
}

// Reproduce a saved sandbox exactly: re-spawn deterministically and replay the
// recorded number of fixed steps.
void restore_bookmark(AppState& app, CosmosState& cosmos, const CosmosBookmark& b) {
    app.ui.seeded.seed_input = b.seed;
    cosmos.configure(b.seed);
    const int idx = std::clamp(b.scale_index, 0, static_cast<int>(kScaleCount) - 1);
    cosmos.set_scale(static_cast<Scale>(idx));
    populate_sandbox(cosmos.system, cosmos.catalog, cosmos.genome, cosmos.scale);
    advance_sandbox(cosmos.system, std::max(0, b.steps));
    cosmos.has_sim = !cosmos.system.bodies.empty();
    cosmos.running = false;
    cosmos.step_count = std::max(0, b.steps);
    cosmos.accumulator = 0.0;
    cosmos.elapsed = cosmos.step_count * kSandboxDt;
}

} // namespace

void CosmosState::configure(const std::string& seed_text) {
    seed = seed_text.empty() ? std::string("worldline") : seed_text;
    genome = generate_law_genome(seed);
    catalog = build_object_catalog();
    apply_law_genome(catalog, genome);
    initialized = true;
    selected_object = 0;
    compare_object = -1;
    clear_sim();
}

void CosmosState::set_scale(Scale next) {
    scale = next;
    selected_object = 0;
    compare_object = -1;
}

void CosmosState::clear_sim() {
    system.bodies.clear();
    has_sim = false;
    running = false;
    step_count = 0;
    accumulator = 0.0;
    elapsed = 0.0;
}

void CosmosState::spawn() {
    clear_sim();
    populate_sandbox(system, catalog, genome, scale);
    has_sim = !system.bodies.empty();
    running = has_sim;
}

void step_cosmos(CosmosState& cosmos, float frame_time) {
    if (!cosmos.has_sim || !cosmos.running) {
        return;
    }
    // Fixed-step accumulator: deterministic and frame-rate independent, so the
    // sandbox can be reproduced exactly from its step count.
    cosmos.accumulator += std::min(static_cast<double>(frame_time), 0.05);
    int steps = 0;
    while (cosmos.accumulator >= kSandboxDt && steps < 8) {
        advance_sandbox(cosmos.system, 1);
        cosmos.accumulator -= kSandboxDt;
        ++cosmos.step_count;
        ++steps;
    }
    cosmos.elapsed = cosmos.step_count * kSandboxDt;
}

namespace {

void draw_ladder(CosmosState& cosmos, Rectangle rect, float scale) {
    draw_card(rect, {6, 13, 24, 224}, with_alpha(WL::GLASS_BORDER, 120));
    draw_text("SCALE LADDER", {rect.x + 14.0f * scale, rect.y + 12.0f * scale},
              13.0f * scale, with_alpha(WL::CYAN_CORE, 200));

    const float top = rect.y + 38.0f * scale;
    const float row_h = (rect.height - 48.0f * scale) / static_cast<float>(kScaleCount);
    for (std::size_t i = 0; i < kScaleCount; ++i) {
        const ScaleTier& tier = scale_ladder()[i];
        const Rectangle row = {rect.x + 8.0f * scale, top + row_h * i,
                               rect.width - 16.0f * scale, row_h - 4.0f * scale};
        const bool active = (cosmos.scale == tier.scale);
        const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        const Color fill = active ? Color{12, 30, 50, 240}
                                  : (hot ? Color{9, 20, 36, 220} : Color{6, 13, 24, 180});
        DrawRectangleRounded(row, 0.16f, 6, fill);
        if (active) {
            DrawRectangleRoundedLines(row, 0.16f, 6, 1.2f, with_alpha(WL::CYAN_CORE, 160));
            DrawRectangle(static_cast<int>(row.x + 2), static_cast<int>(row.y + 4), 3,
                          static_cast<int>(row.height - 8), with_alpha(WL::CYAN_CORE, 220));
        }
        draw_text(tier.name, {row.x + 12.0f * scale, row.y + 6.0f * scale}, 15.0f * scale,
                  active ? WL::TEXT_PRIMARY : WL::TEXT_SECONDARY);
        draw_text(fmt_sci(tier.length_m) + " m",
                  {row.x + 12.0f * scale, row.y + 24.0f * scale}, 11.5f * scale,
                  with_alpha(WL::TEXT_TERTIARY, 220));
        if (clicked(row)) {
            cosmos.set_scale(tier.scale);
        }
    }
}

void draw_inspector(const CosmosState& cosmos, Rectangle rect, float scale) {
    const auto objs = objects_for_scale(cosmos.catalog, cosmos.scale);
    const ScaleTier& tier = tier_for(cosmos.scale);

    draw_card(rect, {6, 13, 24, 224}, with_alpha(WL::GLASS_BORDER, 120));
    draw_text("OBJECT LIBRARY", {rect.x + 14.0f * scale, rect.y + 12.0f * scale},
              13.0f * scale, with_alpha(WL::CYAN_CORE, 200));
    draw_text(std::string(tier.name) + " - " + tier.subtitle,
              {rect.x + 14.0f * scale, rect.y + 30.0f * scale}, 12.0f * scale,
              WL::TEXT_TERTIARY);

    // Object list.
    const float list_top = rect.y + 50.0f * scale;
    const float row_h = 30.0f * scale;
    for (std::size_t i = 0; i < objs.size(); ++i) {
        const UniverseObject& o = *objs[i];
        const Rectangle row = {rect.x + 10.0f * scale, list_top + row_h * i,
                               rect.width - 20.0f * scale, row_h - 4.0f * scale};
        const bool active = (static_cast<int>(i) == cosmos.selected_object);
        const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRounded(row, 0.2f, 6,
                             active ? Color{12, 30, 50, 235}
                                    : (hot ? Color{9, 20, 36, 210} : Color{6, 13, 24, 170}));
        DrawCircleGradient(static_cast<int>(row.x + 14.0f * scale),
                           static_cast<int>(row.y + row.height * 0.5f), 7.0f * scale,
                           to_raylib(o.color), {0, 0, 0, 0});
        draw_text(o.name, {row.x + 28.0f * scale, row.y + 4.0f * scale}, 14.0f * scale,
                  active ? WL::TEXT_PRIMARY : WL::TEXT_SECONDARY);
        // Abundance bar.
        const float bar_w = (rect.width - 38.0f * scale) * static_cast<float>(o.abundance);
        DrawRectangleRounded({row.x + 28.0f * scale, row.y + row.height - 6.0f * scale,
                              std::max(2.0f, bar_w), 2.5f * scale},
                             0.5f, 4, with_alpha(WL::PLASMA_GREEN, 150));
        if (clicked(row)) {
            const_cast<CosmosState&>(cosmos).selected_object = static_cast<int>(i);
        } else if (right_clicked(row)) {
            const_cast<CosmosState&>(cosmos).compare_object = static_cast<int>(i);
        }
    }

    // Inspector for the selected object.
    if (objs.empty()) {
        return;
    }
    const int sel = std::clamp(cosmos.selected_object, 0, static_cast<int>(objs.size()) - 1);
    const UniverseObject& o = *objs[static_cast<std::size_t>(sel)];

    const float insp_y = list_top + row_h * objs.size() + 10.0f * scale;
    DrawLineEx({rect.x + 12.0f * scale, insp_y}, {rect.x + rect.width - 12.0f * scale, insp_y},
               1.0f, with_alpha(WL::GLASS_BORDER, 160));
    draw_text(o.name + "  (" + o.symbol + ")", {rect.x + 14.0f * scale, insp_y + 8.0f * scale},
              17.0f * scale, WL::TEXT_PRIMARY);
    draw_text_block(o.description,
                    {rect.x + 14.0f * scale, insp_y + 30.0f * scale, rect.width - 28.0f * scale,
                     40.0f * scale},
                    12.5f * scale, WL::TEXT_TERTIARY, 2.0f * scale);

    const float grid_y = insp_y + 70.0f * scale;
    const float tile_w = (rect.width - 30.0f * scale) * 0.5f;
    const float tile_h = 40.0f * scale;
    const float gx = rect.x + 12.0f * scale;
    auto tile = [&](int col, int rowi, const char* label, const std::string& value) {
        const Rectangle t = {gx + col * (tile_w + 6.0f * scale),
                             grid_y + rowi * (tile_h + 6.0f * scale), tile_w, tile_h};
        draw_metric(t, label, value, scale);
    };
    tile(0, 0, "REST MASS (kg)", fmt_sci(o.rest_mass_kg));
    tile(1, 0, "RADIUS (m)", fmt_sci(o.radius_m));
    tile(0, 1, "MASS FACTOR", fmt_fixed(o.mass_factor, 3) + "x");
    tile(1, 1, "CHARGE (e)", fmt_fixed(o.charge_e, 2));
    tile(0, 2, "STABILITY", fmt_fixed(o.stability, 2));
    tile(1, 2, "BINDING", fmt_fixed(o.binding, 2));

    float y = grid_y + 3.0f * (tile_h + 6.0f * scale) + 4.0f * scale;

    // Constituents — what this object is built from (clickable to navigate).
    const auto parts = resolve_constituents(cosmos.catalog, o);
    if (!parts.empty()) {
        draw_text("BUILT FROM", {rect.x + 14.0f * scale, y}, 11.5f * scale,
                  with_alpha(WL::CYAN_CORE, 180));
        y += 18.0f * scale;
        float chip_x = rect.x + 14.0f * scale;
        for (const ConstituentRef& part : parts) {
            const float w = measure_ui_text(part.name, 12.0f * scale).x + 16.0f * scale;
            if (chip_x + w > rect.x + rect.width - 14.0f * scale) {
                chip_x = rect.x + 14.0f * scale;
                y += 24.0f * scale;
            }
            const Rectangle chip = {chip_x, y, w, 20.0f * scale};
            const bool hot = part.object && CheckCollisionPointRec(GetMousePosition(), chip);
            DrawRectangleRounded(chip, 0.5f, 6,
                                 hot ? Color{16, 38, 60, 235} : Color{8, 18, 32, 200});
            draw_text(part.name, {chip_x + 8.0f * scale, y + 3.0f * scale}, 12.0f * scale,
                      part.object ? WL::TEXT_SECONDARY : WL::TEXT_TERTIARY);
            if (part.object && clicked(chip)) {
                select_object_by_id(const_cast<CosmosState&>(cosmos), part.id);
            }
            chip_x += w + 6.0f * scale;
        }
        y += 28.0f * scale;
    }

    // Comparison — right-click any object in the list to compare against it.
    const auto all = objects_for_scale(cosmos.catalog, cosmos.scale);
    if (cosmos.compare_object >= 0 && cosmos.compare_object < static_cast<int>(all.size()) &&
        cosmos.compare_object != sel) {
        const UniverseObject& other = *all[static_cast<std::size_t>(cosmos.compare_object)];
        const ObjectComparison cmp = compare_objects(o, other);
        draw_text("COMPARED TO " + other.name, {rect.x + 14.0f * scale, y}, 11.5f * scale,
                  with_alpha(WL::XENON_CORE, 200));
        y += 18.0f * scale;
        const std::string line1 =
            "mass x" + fmt_fixed(cmp.mass_ratio, 2) + "  (" +
            (cmp.mass_orders >= 0 ? "+" : "") + std::to_string(cmp.mass_orders) + " orders)";
        const std::string line2 =
            "radius x" + fmt_fixed(cmp.radius_ratio, 2) +
            "   stability " + (cmp.stability_delta >= 0 ? "+" : "") +
            fmt_fixed(cmp.stability_delta, 2);
        draw_text(line1, {rect.x + 14.0f * scale, y}, 12.5f * scale, WL::TEXT_SECONDARY);
        draw_text(line2, {rect.x + 14.0f * scale, y + 16.0f * scale}, 12.5f * scale,
                  WL::TEXT_SECONDARY);
    } else {
        draw_text("right-click an object to compare", {rect.x + 14.0f * scale, y},
                  11.5f * scale, with_alpha(WL::TEXT_TERTIARY, 180));
    }
}

void draw_observables(const CosmosState& cosmos, Rectangle rect, float scale) {
    draw_card(rect, {6, 13, 24, 224}, with_alpha(WL::GLASS_BORDER, 120));
    draw_text("LIVE OBSERVABLES", {rect.x + 14.0f * scale, rect.y + 10.0f * scale},
              13.0f * scale, with_alpha(WL::PLASMA_GREEN, 210));

    if (!cosmos.has_sim) {
        draw_text_block("Spawn this scale to populate a live sandbox and read its emergent observables.",
                        {rect.x + 14.0f * scale, rect.y + 32.0f * scale, rect.width - 28.0f * scale,
                         rect.height - 40.0f * scale},
                        13.0f * scale, WL::TEXT_TERTIARY, 3.0f * scale);
        return;
    }

    const NBodySystem& sys = cosmos.system;
    const float grid_y = rect.y + 32.0f * scale;
    const float tile_w = (rect.width - 30.0f * scale) / 3.0f;
    const float tile_h = 40.0f * scale;
    const float gx = rect.x + 12.0f * scale;
    auto tile = [&](int col, int rowi, const char* label, const std::string& value) {
        const Rectangle t = {gx + col * (tile_w + 6.0f * scale),
                             grid_y + rowi * (tile_h + 6.0f * scale), tile_w, tile_h};
        draw_metric(t, label, value, scale);
    };
    tile(0, 0, "BODIES", std::to_string(sys.bodies.size()));
    tile(1, 0, "BOUND", std::to_string(sys.bound_pair_count()));
    tile(2, 0, "TIME", fmt_fixed(cosmos.elapsed, 1) + "s");
    tile(0, 1, "ENERGY", fmt_fixed(sys.total_energy(), 1));
    tile(1, 1, "VIRIAL", fmt_fixed(sys.virial_ratio(), 2));
    tile(2, 1, "SPREAD", fmt_fixed(sys.rms_radius(), 2));

    // Tier-specific signature metric — the headline reading for this scale.
    const SignatureMetric sig = tier_signature_metric(cosmos.scale, sys);
    const float sig_y = grid_y + 2.0f * (tile_h + 6.0f * scale) + 4.0f * scale;
    const Rectangle bar = {rect.x + 12.0f * scale, sig_y, rect.width - 24.0f * scale, 30.0f * scale};
    DrawRectangleRounded(bar, 0.18f, 6, {14, 28, 46, 235});
    DrawRectangleRoundedLines(bar, 0.18f, 6, 1.1f, with_alpha(WL::XENON_CORE, 150));
    DrawRectangle(static_cast<int>(bar.x + 2), static_cast<int>(bar.y + 4), 3,
                  static_cast<int>(bar.height - 8), with_alpha(WL::XENON_CORE, 220));
    draw_text(sig.label, {bar.x + 12.0f * scale, bar.y + 8.0f * scale}, 13.0f * scale,
              with_alpha(WL::XENON_CORE, 220));
    const std::string sig_val =
        (std::abs(sig.value) >= 10.0) ? fmt_fixed(sig.value, 1) : fmt_fixed(sig.value, 2);
    const Vector2 vs = measure_ui_text(sig_val, 17.0f * scale);
    draw_text(sig_val, {bar.x + bar.width - vs.x - 12.0f * scale, bar.y + 6.0f * scale},
              17.0f * scale, WL::TEXT_PRIMARY);
}

} // namespace

CosmosExplorerResult draw_cosmos_explorer_scene(AppState& app,
                                                CosmosState& cosmos,
                                                Renderer& renderer,
                                                Rectangle viewport) {
    CosmosExplorerResult result;

    if (!cosmos.initialized || cosmos.seed != app.ui.seeded.seed_input) {
        cosmos.configure(app.ui.seeded.seed_input);
    }

    const float scale =
        std::clamp(std::min(viewport.width / 1500.0f, viewport.height / 920.0f), 0.85f, 1.2f);
    const float margin = 26.0f * scale;
    const float top = viewport.y + 76.0f * scale;
    const float gap = 16.0f * scale;
    const float ladder_w = 210.0f * scale;
    const float panel_w = 360.0f * scale;
    const float controls_h = 46.0f * scale;

    const float column_h = viewport.y + viewport.height - margin - top;
    const Rectangle ladder = {viewport.x + margin, top, ladder_w, column_h};
    const Rectangle panel = {viewport.x + viewport.width - margin - panel_w, top, panel_w, column_h};
    const float stage_x = ladder.x + ladder_w + gap;
    const float stage_w = panel.x - gap - stage_x;
    const Rectangle stage = {stage_x, top, stage_w, column_h - controls_h - 10.0f * scale};
    const Rectangle controls = {stage_x, stage.y + stage.height + 10.0f * scale, stage_w, controls_h};

    // ── Header (offset to clear the top-left back button) ────────────────────
    const float header_x = viewport.x + 200.0f * scale;
    draw_text("COSMOS EXPLORER", {header_x, viewport.y + 24.0f * scale},
              24.0f * scale, WL::TEXT_PRIMARY);
    draw_text("seed '" + cosmos.seed + "'  -  law genome: " + describe_genome(cosmos.genome),
              {header_x, viewport.y + 52.0f * scale}, 13.0f * scale,
              with_alpha(WL::VIOLET_CORE, 210));

    // Static UI is drawn first. The N-body stage uses a scissor + render-texture
    // passes, so it is rendered LAST to guarantee it cannot clip the panels.

    // ── Controls ─────────────────────────────────────────────────────────────
    const float btn_gap = 8.0f * scale;
    const float btn_w = (controls.width - btn_gap * 4.0f) / 5.0f;
    auto control_rect = [&](int i) {
        return Rectangle{controls.x + i * (btn_w + btn_gap), controls.y, btn_w, controls.height};
    };
    if (draw_button(control_rect(0), cosmos.has_sim ? "Re-spawn" : "Spawn",
                    {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, scale)) {
        cosmos.spawn();
    }
    if (draw_button(control_rect(1), cosmos.running ? "Pause" : "Resume",
                    {18, 40, 36, 235}, {26, 70, 56, 255}, WL::PLASMA_GREEN, cosmos.has_sim, scale)) {
        cosmos.running = !cosmos.running;
    }
    if (draw_button(control_rect(2), "Clear", {26, 30, 48, 228}, {38, 46, 70, 255},
                    WL::TEXT_PRIMARY, cosmos.has_sim, scale)) {
        cosmos.clear_sim();
    }
    if (draw_button(control_rect(3), "Save", {30, 22, 60, 232}, {46, 32, 92, 255},
                    WL::VIOLET_CORE, cosmos.has_sim, scale)) {
        save_current_sandbox(cosmos);
    }
    if (draw_button(control_rect(4), "Saved...", {22, 34, 58, 230}, {32, 50, 86, 255},
                    WL::TEXT_PRIMARY, true, scale)) {
        cosmos.browser_open = !cosmos.browser_open;
    }

    // ── Side columns ─────────────────────────────────────────────────────────
    draw_ladder(cosmos, ladder, scale);

    const float obs_h = 158.0f * scale;
    const Rectangle library = {panel.x, panel.y, panel.width, panel.height - obs_h - 10.0f * scale};
    const Rectangle observables = {panel.x, library.y + library.height + 10.0f * scale, panel.width,
                                   obs_h};
    draw_inspector(cosmos, library, scale);
    draw_observables(cosmos, observables, scale);

    if (draw_back_to_menu_button(viewport, scale)) {
        result.back_requested = true;
    }

    // ── Stage (the live N-body sandbox) — rendered last ──────────────────────
    draw_card(stage, {3, 7, 14, 255}, with_alpha(WL::GLASS_BORDER, 130));

    const float cx = stage.x + stage.width * 0.5f;
    const float cy = stage.y + stage.height * 0.5f;
    const float sc = std::min(stage.width, stage.height) * 0.5f / static_cast<float>(kWorldHalf);

    std::vector<Renderer::FieldSprite> sprites;
    sprites.reserve(cosmos.system.bodies.size());
    for (const Body& b : cosmos.system.bodies) {
        Renderer::FieldSprite s;
        s.pos = {cx + static_cast<float>(b.pos.x) * sc, cy + static_cast<float>(b.pos.y) * sc};
        s.radius = std::clamp(static_cast<float>(2.5 + b.radius * 3.0), 2.5f, 10.0f);
        s.color = to_raylib(b.color);
        sprites.push_back(s);
    }

    if (cosmos.has_sim && !cosmos.browser_open) {
        if (cosmos.running) {
            renderer.accumulate_field(sprites, stage, 24);
        }
        renderer.draw_field(sprites, stage);
        draw_text(std::string(tier_for(cosmos.scale).name) + " sandbox  -  " +
                      std::to_string(cosmos.system.bodies.size()) + " bodies" +
                      (cosmos.running ? "  [running]" : "  [paused]"),
                  {stage.x + 14.0f * scale, stage.y + 12.0f * scale}, 13.0f * scale,
                  with_alpha(WL::TEXT_SECONDARY, 220));
    } else {
        draw_text("Empty stage", {stage.x + 14.0f * scale, stage.y + 12.0f * scale},
                  13.0f * scale, with_alpha(WL::TEXT_SECONDARY, 200));
        const char* prompt = "Select a scale, then Spawn to drop this tier's objects into a live sandbox.";
        draw_text_block(prompt,
                        {cx - 220.0f * scale, cy - 20.0f * scale, 440.0f * scale, 60.0f * scale},
                        15.0f * scale, WL::TEXT_TERTIARY, 4.0f * scale);
    }
    DrawRectangleRoundedLines(stage, 0.02f, 12, 1.4f, with_alpha(WL::GLASS_BORDER, 150));

    // Saved-sandbox browser overlay (drawn last; the field pass above is skipped
    // while it is open, so nothing clips it).
    if (cosmos.browser_open) {
        draw_browser_modal(app, cosmos, viewport, scale);
    }

    return result;
}
