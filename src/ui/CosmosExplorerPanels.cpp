#include "ui/CosmosExplorerInternal.hpp"

#include "app/WorldlineStorage.hpp"
#include "cosmos/Analysis.hpp"
#include "cosmos/Sandbox.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace cosmos;

namespace cosmos_ui {

// A deterministic, animated nebula + starfield unique to each universe. Drawn
// behind the sandbox so motion trails glow over it.
void draw_universe_backdrop(const UniversePalette& pal, std::uint64_t sig,
                            Rectangle rect, float t) {
    BeginScissorMode(static_cast<int>(rect.x), static_cast<int>(rect.y),
                     static_cast<int>(rect.width), static_cast<int>(rect.height));

    DrawRectangleGradientEx(rect, palette_color(pal.void_top), palette_color(pal.void_bottom),
                            palette_color(pal.void_bottom), palette_color(pal.void_top));

    // Slow-drifting nebula blooms.
    const float drift = std::sin(t * 0.05f) * rect.width * 0.02f;
    DrawCircleGradient(static_cast<int>(rect.x + rect.width * 0.30f + drift),
                       static_cast<int>(rect.y + rect.height * 0.34f), rect.height * 0.60f,
                       palette_color(pal.nebula_a, 60), {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(rect.x + rect.width * 0.72f - drift),
                       static_cast<int>(rect.y + rect.height * 0.66f), rect.height * 0.48f,
                       palette_color(pal.nebula_b, 50), {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(rect.x + rect.width * 0.55f),
                       static_cast<int>(rect.y + rect.height * 0.12f), rect.height * 0.34f,
                       palette_color(pal.nebula_a, 26), {0, 0, 0, 0});

    // Twinkling starfield seeded by the universe signature.
    std::uint64_t rng = sig ? sig : 0x1234567ull;
    auto nxt = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    auto fr = [&]() { return static_cast<double>(nxt() % 100000ull) / 100000.0; };
    const int stars = 150 + static_cast<int>(pal.star_density * 340.0);
    for (int i = 0; i < stars; ++i) {
        const float sx = rect.x + static_cast<float>(fr()) * rect.width;
        const float sy = rect.y + static_cast<float>(fr()) * rect.height;
        const double phase = fr() * 6.283;
        const double speed = 1.2 + fr() * 3.2;
        const bool bright = fr() < 0.06;
        const float twinkle = 0.35f + 0.65f * static_cast<float>(0.5 + 0.5 * std::sin(t * speed + phase));
        const float sz = bright ? 1.6f + 1.8f * static_cast<float>(fr())
                                : 0.6f + 0.9f * static_cast<float>(fr());
        Color sc = palette_color(pal.star);
        sc.a = static_cast<unsigned char>(std::clamp(twinkle * (bright ? 235.0f : 150.0f), 0.0f, 255.0f));
        if (bright) {
            Color halo = sc;
            halo.a = static_cast<unsigned char>(sc.a * 0.4f);
            DrawCircleGradient(static_cast<int>(sx), static_cast<int>(sy), sz * 3.2f, halo, {0, 0, 0, 0});
        }
        DrawCircleV({sx, sy}, sz, sc);
    }
    EndScissorMode();
}

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
        const UniverseClassification bcls = classify_universe(generate_law_genome(b.seed));
        draw_text(b.title.empty() ? b.seed : b.title,
                  {row.x + 12.0f * scale, row.y + 7.0f * scale}, 15.0f * scale, WL::TEXT_PRIMARY);
        draw_text(bcls.codename + "  " + bcls.class_name,
                  {row.x + 12.0f * scale, row.y + 26.0f * scale}, 11.5f * scale,
                  with_alpha(WL::VIOLET_CORE, 200));
        const std::string sub = std::string(scale_ladder()[idx].name) + "  -  seed '" + b.seed +
                                "'  -  " + std::to_string(b.steps) + " steps";
        draw_text(sub, {row.x + 12.0f * scale, row.y + 40.0f * scale}, 11.0f * scale,
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
    const float row_h = 22.0f * scale;
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
                    {rect.x + 14.0f * scale, insp_y + 28.0f * scale, rect.width - 28.0f * scale,
                     30.0f * scale},
                    12.0f * scale, WL::TEXT_TERTIARY, 2.0f * scale);
    draw_text(o.epoch + " epoch   -   " + fmt_sci(o.temperature) + " K   -   abundance " +
                  std::to_string(static_cast<int>(o.abundance * 100.0 + 0.5)) + "%",
              {rect.x + 14.0f * scale, insp_y + 56.0f * scale}, 11.5f * scale,
              with_alpha(WL::CYAN_CORE, 185));

    const float grid_y = insp_y + 76.0f * scale;
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

    // Derived physics — computed live from the SI mass/radius anchors
    // (display only; the sandbox dynamics are dimensionless and unaffected).
    const DerivedQuantities dq = derive_quantities(o.rest_mass_kg, o.radius_m);
    tile(0, 3, "DENSITY (kg/m3)", fmt_sci(dq.density_kg_m3));
    tile(1, 3, "SCHWARZSCHILD (m)", fmt_sci(dq.schwarzschild_m));
    tile(0, 4, "ESCAPE V (m/s)", fmt_sci(dq.escape_velocity_ms));
    tile(1, 4, "COMPACTNESS rs/r", fmt_fixed(dq.compactness, 3));

    float y = grid_y + 5.0f * (tile_h + 6.0f * scale) + 4.0f * scale;

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

// The generation report: the universe's full dossier — classification, palette,
// fundamental constants, traits and catalog census.
void draw_dossier_modal(CosmosState& cosmos, Rectangle viewport, float scale) {
    DrawRectangle(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                  static_cast<int>(viewport.width), static_cast<int>(viewport.height),
                  {2, 4, 9, 205});
    const float w = std::min(viewport.width * 0.56f, 700.0f * scale);
    const float h = std::min(viewport.height * 0.8f, 600.0f * scale);
    const Rectangle m = {viewport.x + (viewport.width - w) * 0.5f,
                         viewport.y + (viewport.height - h) * 0.5f, w, h};
    draw_card(m, {7, 14, 26, 250}, palette_color(cosmos.palette.accent, 160));

    const UniverseClassification& c = cosmos.classification;
    draw_text(c.codename, {m.x + 20.0f * scale, m.y + 16.0f * scale}, 15.0f * scale,
              palette_color(cosmos.palette.accent, 230));
    draw_text(c.class_name, {m.x + 20.0f * scale, m.y + 34.0f * scale}, 24.0f * scale,
              WL::TEXT_PRIMARY);
    draw_text("seed '" + cosmos.seed + "'   -   generation v" +
                  std::to_string(kGenerationVersion),
              {m.x + 20.0f * scale, m.y + 64.0f * scale}, 12.0f * scale, WL::TEXT_TERTIARY);

    const Rectangle close = {m.x + m.width - 38.0f * scale, m.y + 16.0f * scale, 24.0f * scale,
                             24.0f * scale};
    if (draw_button(close, "x", {30, 18, 40, 235}, {60, 30, 70, 255}, WL::TEXT_PRIMARY, true, scale) ||
        IsKeyPressed(KEY_ESCAPE)) {
        cosmos.dossier_open = false;
    }

    // Palette swatches.
    const Color8 sw[5] = {cosmos.palette.accent, cosmos.palette.nebula_a, cosmos.palette.nebula_b,
                          cosmos.palette.star, cosmos.palette.void_bottom};
    for (int i = 0; i < 5; ++i) {
        const Rectangle r = {m.x + 20.0f * scale + i * 30.0f * scale, m.y + 84.0f * scale,
                             24.0f * scale, 14.0f * scale};
        DrawRectangleRounded(r, 0.3f, 4, palette_color(sw[i]));
        DrawRectangleRoundedLines(r, 0.3f, 4, 1.0f, with_alpha(WL::TEXT_TERTIARY, 120));
    }
    draw_text("complexity " + std::to_string(static_cast<int>(c.complexity * 100.0 + 0.5)) + "%",
              {m.x + 190.0f * scale, m.y + 84.0f * scale}, 13.0f * scale,
              palette_color(cosmos.palette.accent, 220));

    // Fundamental constants grid.
    const LawGenome& g = cosmos.genome;
    const float gy = m.y + 112.0f * scale;
    const float tw = (m.width - 50.0f * scale) / 4.0f;
    const float th = 40.0f * scale;
    auto tile = [&](int col, int row, const char* label, const std::string& value) {
        draw_metric({m.x + 20.0f * scale + col * (tw + 6.0f * scale), gy + row * (th + 6.0f * scale),
                     tw, th},
                    label, value, scale);
    };
    tile(0, 0, "STRONG", fmt_fixed(g.coupling_strong, 3));
    tile(1, 0, "EM", fmt_fixed(g.coupling_em, 3));
    tile(2, 0, "GRAVITY", fmt_fixed(g.coupling_gravity, 3));
    tile(3, 0, "GRAV EXP", fmt_fixed(g.gravity_exponent, 3));
    tile(0, 1, "MASS SCALE", fmt_fixed(g.mass_scale, 3));
    tile(1, 1, "EXPANSION", fmt_fixed(g.cosmological_drift, 3));
    tile(2, 1, "STABILITY", fmt_fixed(g.stability_bias, 3));
    tile(3, 1, "OBJECTS", std::to_string(cosmos.catalog.size()));

    // Traits.
    float ty = gy + 2.0f * (th + 6.0f * scale) + 8.0f * scale;
    draw_text("DISTINGUISHING TRAITS", {m.x + 20.0f * scale, ty}, 12.0f * scale,
              with_alpha(WL::CYAN_CORE, 190));
    ty += 20.0f * scale;
    for (const std::string& tr : c.traits) {
        draw_text("-  " + tr, {m.x + 24.0f * scale, ty}, 13.0f * scale, WL::TEXT_SECONDARY);
        ty += 19.0f * scale;
        if (ty > m.y + m.height - 60.0f * scale) break;
    }

    // Catalog census by tier.
    std::string census;
    for (std::size_t s = 0; s < kScaleCount; ++s) {
        const auto objs = objects_for_scale(cosmos.catalog, static_cast<Scale>(s));
        census += std::to_string(objs.size());
        if (s + 1 < kScaleCount) census += " / ";
    }
    draw_text("census (subatomic .. cosmic):  " + census,
              {m.x + 20.0f * scale, m.y + m.height - 30.0f * scale}, 11.5f * scale,
              WL::TEXT_TERTIARY);
}

} // namespace cosmos_ui
