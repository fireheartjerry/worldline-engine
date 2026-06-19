#include "ui/CosmosDescent.hpp"

#include "ui/CosmosExplorerInternal.hpp" // helpers + draw_universe_backdrop decl
#include "ui/CosmosNavigator.hpp"        // kZoomMin / kZoomMax
#include "renderer/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>

using namespace cosmos;

namespace cosmos_ui {

namespace {

// Layout space is normalized ~[-1,1]; a touch over 1 leaves the outermost
// children a margin from the stage edge at zoom 1.
constexpr float kDescentHalf = 1.18f;

float base_scale(Rectangle stage) {
    return std::min(stage.width, stage.height) * 0.5f / kDescentHalf;
}

Vector2 layout_to_screen(float nx, float ny, Rectangle stage, const CosmosCamera& cam) {
    const float cx = stage.x + stage.width * 0.5f;
    const float cy = stage.y + stage.height * 0.5f;
    const float s = base_scale(stage) * static_cast<float>(cam.zoom);
    return {cx + (nx - static_cast<float>(cam.pan.x)) * s,
            cy + (ny - static_cast<float>(cam.pan.y)) * s};
}

// Animated layout position of a child (orbits in a star system, gentle wander in
// an ecosystem, static otherwise). Animation is render-time only — never stored.
void child_layout(const ChildRef& c, NodeKind parent, float t, float& nx, float& ny) {
    if (parent == NodeKind::StarSystem && c.orbit > 1.0e-4f) {
        const float speed = 0.35f / (0.25f + c.orbit); // inner orbits faster
        const float ang = c.phase + t * speed;
        nx = std::cos(ang) * c.orbit;
        ny = std::sin(ang) * c.orbit;
    } else if (parent == NodeKind::Ecosystem) {
        nx = c.x + 0.05f * std::sin(t * 0.7f + c.phase);
        ny = c.y + 0.05f * std::cos(t * 0.9f + c.phase * 1.3f);
    } else {
        nx = c.x;
        ny = c.y;
    }
}

Vector2 child_screen(const CosmosState& cosmos, int i, Rectangle stage, float t) {
    const DescentState& d = cosmos.descent;
    float nx, ny;
    child_layout(d.focus().children[static_cast<std::size_t>(i)], d.focus_kind(), t, nx, ny);
    return layout_to_screen(nx, ny, stage, d.camera);
}

} // namespace

void descent_ensure_init(CosmosState& cosmos) {
    DescentState& d = cosmos.descent;
    const std::uint64_t sig = cosmos.genome.signature ? cosmos.genome.signature : 1ull;
    if (d.initialized && d.root_seed == sig && !d.path.empty()) {
        return;
    }
    if (!d.universe) {
        d.universe = std::make_unique<ProcUniverse>(sig);
    } else {
        d.universe->reseed(sig);
    }
    d.root_seed = sig;
    d.path.clear();
    d.path.push_back(d.universe->root()); // copy
    d.hovered_child = -1;
    d.camera = CosmosCamera{};
    d.transition = 1.0f;
    d.transition_dir = 0;
    d.initialized = true;
}

void descent_push(CosmosState& cosmos, int child_index) {
    DescentState& d = cosmos.descent;
    if (child_index < 0 || child_index >= static_cast<int>(d.focus().children.size())) {
        return;
    }
    if (node_is_leaf(d.focus_kind())) {
        return;
    }
    const ChildRef child = d.focus().children[static_cast<std::size_t>(child_index)]; // copy
    ProcNode cn = d.universe->node(child.seed, child.kind); // copy (engine ref may move)
    d.path.push_back(std::move(cn));
    d.hovered_child = -1;
    d.camera.target_zoom = d.camera.zoom = kZoomMin * 1.05;
    d.camera.target_pan = d.camera.pan = Vec2{};
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
    d.transition_dir = 1;
}

void descent_pop(CosmosState& cosmos) {
    DescentState& d = cosmos.descent;
    if (d.depth() <= 0) {
        return;
    }
    const std::uint64_t exited = d.focus().seed;
    d.path.pop_back();
    d.hovered_child = -1;
    // Land zoomed-in, recentered on the child we came from, so it reads as
    // pulling back out of where we were.
    d.camera.target_zoom = d.camera.zoom = kZoomMax * 0.95;
    Vec2 p{};
    for (const ChildRef& c : d.focus().children) {
        if (c.seed == exited) { p = {c.x, c.y}; break; }
    }
    d.camera.target_pan = d.camera.pan = p;
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
    d.transition_dir = -1;
}

void descent_jump_to_depth(CosmosState& cosmos, int depth) {
    DescentState& d = cosmos.descent;
    depth = std::clamp(depth, 0, d.depth());
    d.path.resize(static_cast<std::size_t>(depth) + 1);
    d.hovered_child = -1;
    d.camera.target_zoom = d.camera.zoom = 1.0;
    d.camera.target_pan = d.camera.pan = Vec2{};
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
    d.transition_dir = 0;
}

void update_descent(CosmosState& cosmos, Rectangle stage, float dt, bool interactive) {
    descent_ensure_init(cosmos);
    DescentState& d = cosmos.descent;
    CosmosCamera& cam = d.camera;
    const float sc0 = base_scale(stage);
    const Vector2 ctr = {stage.x + stage.width * 0.5f, stage.y + stage.height * 0.5f};
    const float t = static_cast<float>(GetTime());

    if (interactive) {
        const bool over = CheckCollisionPointRec(GetMousePosition(), stage);

        // Cursor-anchored wheel zoom (world point under the cursor stays put).
        const float wheel = GetMouseWheelMove();
        if (over && wheel != 0.0f) {
            const Vector2 m = GetMousePosition();
            const double z0 = cam.target_zoom;
            const Vec2 before = {cam.target_pan.x + (m.x - ctr.x) / (sc0 * z0),
                                 cam.target_pan.y + (m.y - ctr.y) / (sc0 * z0)};
            cam.target_zoom *= std::exp(wheel * 0.22);
            const double z1 = cam.target_zoom;
            const Vec2 after = {cam.target_pan.x + (m.x - ctr.x) / (sc0 * z1),
                                cam.target_pan.y + (m.y - ctr.y) / (sc0 * z1)};
            cam.target_pan.x += before.x - after.x;
            cam.target_pan.y += before.y - after.y;
        }

        // Drag to pan.
        const bool down = IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
        const Vector2 mv = GetMouseDelta();
        if (over && down && (cam.dragging || std::abs(mv.x) + std::abs(mv.y) > 1.5f)) {
            cam.target_pan.x -= mv.x / (sc0 * cam.target_zoom);
            cam.target_pan.y -= mv.y / (sc0 * cam.target_zoom);
            cam.dragging = true;
        }
        if (!down) cam.dragging = false;

        // Keyboard.
        const double pan_step = (4.0 * dt) / std::max(0.35, cam.target_zoom);
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  cam.target_pan.x -= pan_step;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) cam.target_pan.x += pan_step;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    cam.target_pan.y -= pan_step;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  cam.target_pan.y += pan_step;
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD))      cam.target_zoom *= std::exp(2.2 * dt);
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) cam.target_zoom *= std::exp(-2.2 * dt);
        if (IsKeyPressed(KEY_BACKSPACE)) descent_pop(cosmos);

        // Hover: nearest child to the cursor within a small pixel radius.
        d.hovered_child = -1;
        if (!cam.dragging && over) {
            float best = 26.0f * std::max(1.0f, static_cast<float>(cam.zoom) * 0.5f);
            const Vector2 mouse = GetMousePosition();
            for (int i = 0; i < static_cast<int>(d.focus().children.size()); ++i) {
                const Vector2 p = child_screen(cosmos, i, stage, t);
                const float dist = std::hypot(p.x - mouse.x, p.y - mouse.y);
                if (dist < best) { best = dist; d.hovered_child = i; }
            }
            // Click a hovered non-leaf child to enter it.
            if (d.hovered_child >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                !node_is_leaf(d.focus_kind())) {
                descent_push(cosmos, d.hovered_child);
            }
        }
    }

    // Descend / ascend on zoom bounds.
    if (cam.target_zoom > kZoomMax) {
        const ProcNode& f = d.focus();
        if (!node_is_leaf(f.kind) && !f.children.empty()) {
            // Pick the child nearest the stage center (cursor-anchored zoom pulls
            // the aimed child there). Require it to be reasonably central.
            int best = -1;
            float best_d = 1.0e30f;
            for (int i = 0; i < static_cast<int>(f.children.size()); ++i) {
                const Vector2 p = child_screen(cosmos, i, stage, t);
                const float dist = std::hypot(p.x - ctr.x, p.y - ctr.y);
                if (dist < best_d) { best_d = dist; best = i; }
            }
            const float reach = 0.32f * std::min(stage.width, stage.height);
            if (best >= 0 && best_d < reach) {
                descent_push(cosmos, best);
            } else {
                cam.target_zoom = kZoomMax; // nothing to enter; hold at the edge
            }
        } else {
            cam.target_zoom = kZoomMax; // leaf or empty: cannot descend
        }
    } else if (cam.target_zoom < kZoomMin) {
        if (d.depth() > 0) {
            descent_pop(cosmos);
        } else {
            cam.target_zoom = kZoomMin; // already at the universe root
        }
    }
    cam.target_zoom = std::clamp(cam.target_zoom, kZoomMin * 0.9, kZoomMax * 1.1);

    // Smoothing + envelopes.
    const double k = 1.0 - std::exp(-13.0 * std::max(0.0001f, dt));
    cam.zoom  += (cam.target_zoom - cam.zoom) * k;
    cam.pan.x += (cam.target_pan.x - cam.pan.x) * k;
    cam.pan.y += (cam.target_pan.y - cam.pan.y) * k;
    cam.flash = std::max(0.0f, cam.flash - dt * 2.2f);
    d.transition = std::min(1.0f, d.transition + dt * 2.5f);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void draw_descent_stage(CosmosState& cosmos, Renderer& renderer, Rectangle stage, float ui) {
    DescentState& d = cosmos.descent;
    const CosmosCamera& cam = d.camera;
    const ProcNode& f = d.focus();
    const float t = static_cast<float>(GetTime());

    draw_universe_backdrop(cosmos.palette, cosmos.genome.signature ^ (f.seed * 0x9E3779B9ull),
                           stage, t);

    const float zoomf = static_cast<float>(cam.zoom);
    const float sc = base_scale(stage) * zoomf;

    // Orbit rings + central body (drawn under the additive field pass).
    BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                     static_cast<int>(stage.width), static_cast<int>(stage.height));
    const Vector2 center = layout_to_screen(0.0f, 0.0f, stage, cam);
    if (f.kind == NodeKind::StarSystem) {
        for (const ChildRef& c : f.children) {
            if (c.orbit > 1.0e-4f) {
                DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                                c.orbit * sc, with_alpha(WL::GLASS_BORDER, 70));
            }
        }
    } else if (f.kind == NodeKind::Planet) {
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y),
                           0.42f * sc, palette_color(f.color, 70), Color{0, 0, 0, 0});
    }
    EndScissorMode();

    // Build the bounded sprite set.
    std::vector<Renderer::FieldSprite> sprites;
    sprites.reserve(f.children.size() + 1);
    const unsigned char child_alpha =
        static_cast<unsigned char>(std::clamp(70.0f + 185.0f * d.transition, 0.0f, 255.0f));

    // A central body for levels that have one.
    if (f.kind == NodeKind::StarSystem || f.kind == NodeKind::Creature) {
        Renderer::FieldSprite s;
        s.pos = center;
        s.radius = std::clamp((f.kind == NodeKind::StarSystem ? 9.0f : 16.0f) * std::sqrt(zoomf),
                              4.0f, 40.0f);
        s.color = to_raylib(f.color);
        sprites.push_back(s);
    }

    const int n = std::min(static_cast<int>(f.children.size()), 60);
    for (int i = 0; i < n; ++i) {
        const ChildRef& c = f.children[static_cast<std::size_t>(i)];
        float nx, ny;
        child_layout(c, f.kind, t, nx, ny);
        Renderer::FieldSprite s;
        s.pos = layout_to_screen(nx, ny, stage, cam);
        const float kind_scale = (f.kind == NodeKind::Universe || f.kind == NodeKind::Galaxy) ? 6.0f
                               : (f.kind == NodeKind::Ecosystem) ? 4.0f : 5.5f;
        s.radius = std::clamp((2.0f + c.size * kind_scale) * std::sqrt(zoomf), 1.5f, 24.0f);
        Color col = to_raylib(c.color);
        col.a = child_alpha;
        s.color = col;
        sprites.push_back(s);
    }

    const bool animated = (f.kind == NodeKind::StarSystem || f.kind == NodeKind::Ecosystem);
    const unsigned char fade = (animated && d.transition > 0.3f) ? 30 : 255;
    renderer.accumulate_field(sprites, stage, fade);
    renderer.draw_field(sprites, stage);

    // Boundary flash ring.
    if (cam.flash > 0.001f) {
        DrawRectangleRoundedLines(stage, 0.02f, 12, 2.4f,
                                  with_alpha(WL::CYAN_CORE,
                                             static_cast<unsigned char>(170 * cam.flash)));
    }

    // Hover label + reticle.
    if (d.hovered_child >= 0 && d.hovered_child < static_cast<int>(f.children.size())) {
        const ChildRef& c = f.children[static_cast<std::size_t>(d.hovered_child)];
        const Vector2 p = child_screen(cosmos, d.hovered_child, stage, t);
        const float r = 16.0f * ui;
        BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                         static_cast<int>(stage.width), static_cast<int>(stage.height));
        draw_corner_brackets({p.x - r, p.y - r, r * 2.0f, r * 2.0f},
                             with_alpha(WL::CYAN_CORE, 220), 7.0f * ui, 1.6f, 0.0f);
        const std::string label = c.name + "  -  " + node_kind_name(c.kind);
        const float tw = measure_ui_text(label, 12.5f * ui).x;
        const Rectangle chip = {std::clamp(p.x - tw * 0.5f - 8.0f * ui, stage.x + 4.0f,
                                           stage.x + stage.width - tw - 20.0f * ui),
                                p.y + r + 4.0f * ui, tw + 16.0f * ui, 22.0f * ui};
        draw_glass_panel(chip, {8, 18, 30, 220}, with_alpha(WL::CYAN_DIM, 130), 0.3f, 2);
        draw_text(label, {chip.x + 8.0f * ui, chip.y + 4.0f * ui}, 12.5f * ui, WL::TEXT_PRIMARY);
        EndScissorMode();
    }
}

void draw_descent_breadcrumb(CosmosState& cosmos, Rectangle rect, float ui) {
    DescentState& d = cosmos.descent;
    draw_card(rect, {6, 13, 24, 224}, with_alpha(WL::GLASS_BORDER, 120));
    draw_text("YOU ARE HERE", {rect.x + 14.0f * ui, rect.y + 12.0f * ui}, 13.0f * ui,
              with_alpha(WL::CYAN_CORE, 200));

    const float top = rect.y + 38.0f * ui;
    const float row_h = 44.0f * ui;
    for (int i = 0; i < static_cast<int>(d.path.size()); ++i) {
        const ProcNode& node = d.path[static_cast<std::size_t>(i)];
        const Rectangle row = {rect.x + 8.0f * ui, top + row_h * i, rect.width - 16.0f * ui,
                               row_h - 6.0f * ui};
        const bool active = (i == d.depth());
        const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRounded(row, 0.16f, 6,
                             active ? Color{12, 30, 50, 240}
                                    : (hot ? Color{9, 20, 36, 220} : Color{6, 13, 24, 170}));
        if (active) {
            DrawRectangle(static_cast<int>(row.x + 2), static_cast<int>(row.y + 4), 3,
                          static_cast<int>(row.height - 8), with_alpha(WL::CYAN_CORE, 220));
        }
        DrawCircleGradient(static_cast<int>(row.x + 16.0f * ui),
                           static_cast<int>(row.y + row.height * 0.5f), 7.0f * ui,
                           to_raylib(node.color), {0, 0, 0, 0});
        draw_text(node_kind_name(node.kind), {row.x + 30.0f * ui, row.y + 5.0f * ui},
                  10.5f * ui, with_alpha(WL::TEXT_TERTIARY, 220));
        draw_text(node.name, {row.x + 30.0f * ui, row.y + 19.0f * ui}, 14.0f * ui,
                  active ? WL::TEXT_PRIMARY : WL::TEXT_SECONDARY);
        if (clicked(row) && i != d.depth()) {
            descent_jump_to_depth(cosmos, i);
        }
    }
}

void draw_descent_inspector(CosmosState& cosmos, Rectangle rect, float ui) {
    DescentState& d = cosmos.descent;
    const ProcNode& f = d.focus();
    draw_card(rect, {6, 13, 24, 224}, with_alpha(WL::GLASS_BORDER, 120));
    draw_text(node_kind_name(f.kind), {rect.x + 14.0f * ui, rect.y + 12.0f * ui}, 12.0f * ui,
              with_alpha(WL::CYAN_CORE, 200));
    draw_text(f.name, {rect.x + 14.0f * ui, rect.y + 28.0f * ui}, 20.0f * ui, WL::TEXT_PRIMARY);
    draw_text_block(f.descriptor,
                    {rect.x + 14.0f * ui, rect.y + 54.0f * ui, rect.width - 28.0f * ui, 40.0f * ui},
                    12.5f * ui, WL::TEXT_TERTIARY, 2.0f * ui);

    // Facts grid.
    const float grid_y = rect.y + 96.0f * ui;
    const float tile_w = (rect.width - 30.0f * ui) * 0.5f;
    const float tile_h = 40.0f * ui;
    for (std::size_t i = 0; i < f.facts.size() && i < 8; ++i) {
        const Rectangle tile = {rect.x + 12.0f * ui + (i % 2) * (tile_w + 6.0f * ui),
                                grid_y + (i / 2) * (tile_h + 6.0f * ui), tile_w, tile_h};
        draw_metric(tile, f.facts[i].first.c_str(), f.facts[i].second, ui);
    }

    float y = grid_y + static_cast<float>((f.facts.size() + 1) / 2) * (tile_h + 6.0f * ui) +
              8.0f * ui;

    // Hovered child preview.
    if (d.hovered_child >= 0 && d.hovered_child < static_cast<int>(f.children.size())) {
        const ChildRef& c = f.children[static_cast<std::size_t>(d.hovered_child)];
        const Rectangle card = {rect.x + 12.0f * ui, y, rect.width - 24.0f * ui, 50.0f * ui};
        draw_glass_panel(card, {10, 22, 38, 220}, with_alpha(WL::CYAN_DIM, 130), 0.12f, 2);
        DrawCircleGradient(static_cast<int>(card.x + 16.0f * ui),
                           static_cast<int>(card.y + card.height * 0.5f), 8.0f * ui,
                           to_raylib(c.color), {0, 0, 0, 0});
        draw_text(c.name, {card.x + 30.0f * ui, card.y + 7.0f * ui}, 14.0f * ui, WL::TEXT_PRIMARY);
        draw_text(std::string("scroll in or click to enter this ") + node_kind_name(c.kind),
                  {card.x + 30.0f * ui, card.y + 26.0f * ui}, 11.0f * ui,
                  with_alpha(WL::TEXT_TERTIARY, 220));
        y += 58.0f * ui;
    }

    // Child list.
    if (!node_is_leaf(f.kind)) {
        draw_text(std::to_string(f.children.size()) + " " + node_child_noun(f.kind),
                  {rect.x + 14.0f * ui, y}, 11.5f * ui, with_alpha(WL::CYAN_CORE, 180));
        y += 18.0f * ui;
        const float row_h = 20.0f * ui;
        const int max_rows = std::max(0, static_cast<int>((rect.y + rect.height - y) / row_h));
        const int shown = std::min(static_cast<int>(f.children.size()), max_rows);
        for (int i = 0; i < shown; ++i) {
            const ChildRef& c = f.children[static_cast<std::size_t>(i)];
            const Rectangle row = {rect.x + 12.0f * ui, y + row_h * i, rect.width - 24.0f * ui,
                                   row_h - 3.0f * ui};
            const bool hot = CheckCollisionPointRec(GetMousePosition(), row);
            if (hot) DrawRectangleRounded(row, 0.3f, 4, Color{9, 20, 36, 200});
            DrawCircleGradient(static_cast<int>(row.x + 8.0f * ui),
                               static_cast<int>(row.y + row.height * 0.5f), 5.0f * ui,
                               to_raylib(c.color), {0, 0, 0, 0});
            draw_text(c.name, {row.x + 18.0f * ui, row.y + 2.0f * ui}, 12.5f * ui,
                      hot ? WL::TEXT_PRIMARY : WL::TEXT_SECONDARY);
            if (clicked(row)) descent_push(cosmos, i);
        }
    }
}

void draw_descent_hud(const CosmosState& cosmos, Rectangle stage, float ui) {
    const DescentState& d = cosmos.descent;
    const ProcNode& f = d.focus();

    const Rectangle chip = {stage.x + 12.0f * ui, stage.y + 12.0f * ui, 230.0f * ui, 40.0f * ui};
    draw_glass_panel(chip, {8, 18, 30, 205}, with_alpha(WL::CYAN_DIM, 120), 0.18f, 2);
    draw_text(std::string(node_kind_name(f.kind)) + "  -  depth " + std::to_string(d.depth()),
              {chip.x + 10.0f * ui, chip.y + 6.0f * ui}, 10.0f * ui, with_alpha(WL::CYAN_CORE, 175));
    draw_text(f.name, {chip.x + 10.0f * ui, chip.y + 19.0f * ui}, 16.0f * ui, WL::TEXT_PRIMARY);

    draw_text(node_is_leaf(f.kind)
                  ? "scroll out to ascend   |   drag / WASD: pan   |   backspace: up"
                  : "scroll in: enter   |   click an object to enter   |   scroll out: ascend   |   drag: pan",
              {stage.x + 12.0f * ui, stage.y + stage.height - 18.0f * ui}, 10.5f * ui,
              with_alpha(WL::TEXT_TERTIARY, 200));
}

} // namespace cosmos_ui
