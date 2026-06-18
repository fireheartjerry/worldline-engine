#include "ui/CosmosExplorerScene.hpp"

#include "renderer/Renderer.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace cosmos;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kWorldHalf = 9.0;

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

Color to_raylib(Color8 c) {
    return Color{c.r, c.g, c.b, 255};
}

} // namespace

void CosmosState::configure(const std::string& seed_text) {
    seed = seed_text.empty() ? std::string("worldline") : seed_text;
    genome = generate_law_genome(seed);
    catalog = build_object_catalog();
    apply_law_genome(catalog, genome);
    initialized = true;
    selected_object = 0;
    clear_sim();
}

void CosmosState::set_scale(Scale next) {
    scale = next;
    selected_object = 0;
}

void CosmosState::clear_sim() {
    system.bodies.clear();
    has_sim = false;
    running = false;
    elapsed = 0.0;
}

void CosmosState::spawn() {
    clear_sim();
    const ScaleTier& tier = tier_for(scale);
    system.params = make_force_params(tier, genome);

    const auto objs = objects_for_scale(catalog, scale);
    if (objs.empty()) {
        return;
    }

    // Deterministic xorshift seeded by the universe + tier.
    std::uint64_t rng = genome.signature ^ (0x9E3779B97F4A7C15ull * (scale_index(scale) + 1));
    if (rng == 0) {
        rng = 0x1234567811111111ull;
    }
    auto next = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    auto frand = [&]() { return static_cast<double>(next() % 1000000ull) / 1000000.0; };

    const bool gravity_tier = tier.gravity_weight > 0.5;
    const int count = 90;

    double total_ab = 0.0;
    for (const UniverseObject* o : objs) {
        total_ab += std::max(0.05, o->abundance);
    }
    auto pick = [&]() -> const UniverseObject* {
        double r = frand() * total_ab;
        for (const UniverseObject* o : objs) {
            r -= std::max(0.05, o->abundance);
            if (r <= 0.0) {
                return o;
            }
        }
        return objs.back();
    };

    double total_mass = 0.0;
    for (int i = 0; i < count; ++i) {
        const UniverseObject* o = pick();
        Body b;
        b.mass = o->sim_mass;
        b.radius = o->sim_radius;
        b.charge = o->sim_charge;
        b.color = o->color;
        b.type = static_cast<int>(static_cast<std::size_t>(
            std::find(objs.begin(), objs.end(), o) - objs.begin()));
        if (gravity_tier) {
            const double ang = frand() * 2.0 * kPi;
            const double rad = std::sqrt(frand()) * 5.5;
            b.pos = {rad * std::cos(ang), rad * std::sin(ang)};
        } else {
            b.pos = {(frand() * 2.0 - 1.0) * 4.5, (frand() * 2.0 - 1.0) * 4.5};
        }
        b.vel = {0.0, 0.0};
        system.bodies.push_back(b);
        total_mass += b.mass;
    }

    if (gravity_tier && total_mass > 0.0) {
        const double sign = (genome.cosmological_drift > 1.0) ? 1.0 : -1.0;
        for (Body& b : system.bodies) {
            const double r = b.pos.length();
            if (r > 1.0e-3) {
                const Vec2 tang = {-b.pos.y / r, b.pos.x / r};
                const double speed =
                    0.30 * std::sqrt(system.params.gravity * total_mass / std::max(r, 1.0));
                b.vel = tang * (speed * sign);
            }
        }
        // Remove net drift so the cluster stays centered on the stage.
        const Vec2 com_vel = system.total_momentum() / total_mass;
        for (Body& b : system.bodies) {
            b.vel -= com_vel;
        }
    }

    has_sim = true;
    running = true;
    elapsed = 0.0;
}

void step_cosmos(CosmosState& cosmos, float frame_time) {
    if (!cosmos.has_sim || !cosmos.running) {
        return;
    }
    const double dt = std::min(static_cast<double>(frame_time), 1.0 / 30.0);
    cosmos.system.step(dt, 4);
    cosmos.elapsed += dt;
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
    const float btn_w = (controls.width - btn_gap * 2.0f) / 3.0f;
    if (draw_button({controls.x, controls.y, btn_w, controls.height},
                    cosmos.has_sim ? "Re-spawn" : "Spawn",
                    {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, scale)) {
        cosmos.spawn();
    }
    if (draw_button({controls.x + btn_w + btn_gap, controls.y, btn_w, controls.height},
                    cosmos.running ? "Pause" : "Resume",
                    {18, 40, 36, 235}, {26, 70, 56, 255}, WL::PLASMA_GREEN, cosmos.has_sim, scale)) {
        cosmos.running = !cosmos.running;
    }
    if (draw_button({controls.x + (btn_w + btn_gap) * 2.0f, controls.y, btn_w, controls.height},
                    "Clear", {26, 30, 48, 228}, {38, 46, 70, 255}, WL::TEXT_PRIMARY,
                    cosmos.has_sim, scale)) {
        cosmos.clear_sim();
    }

    // ── Side columns ─────────────────────────────────────────────────────────
    draw_ladder(cosmos, ladder, scale);

    const float obs_h = 122.0f * scale;
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

    if (cosmos.has_sim) {
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

    return result;
}
