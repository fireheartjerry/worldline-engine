#include "ui/CosmosDescent.hpp"

#include "ui/CosmosExplorerInternal.hpp" // helpers + draw_universe_backdrop decl
#include "ui/CosmosNavigator.hpp"        // kZoomMin / kZoomMax
#include "renderer/Renderer.hpp"

#include "cosmos/Astrobio.hpp"
#include "cosmos/EcoSim.hpp"
#include "cosmos/SpatialHash.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

// Geometry of the analysis deck (a strip along the bottom of the stage). The
// HUD legend and on-stage keys sit above it when it is open, never under it.
// The map lays out in the part of the stage the deck leaves visible. The
// reserved strip animates (see update_descent), so toggling the deck glides the
// map rather than jumping it.
Rectangle layout_rect(const DescentState& d, Rectangle stage) {
    const float reserve = std::clamp(d.deck_reserve, 0.0f, stage.height * 0.6f);
    return {stage.x, stage.y, stage.width, stage.height - reserve};
}
float stage_content_bottom(const DescentState& d, Rectangle stage, float /*ui*/) {
    const Rectangle L = layout_rect(d, stage);
    return L.y + L.height;
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

// Horizontal layout half-extent of the focused level. Layout space is square,
// but an ecosystem's food web is fitted to the map's real width (see below).
float layout_half_x(const DescentState& d) {
    return d.focus_kind() == NodeKind::Ecosystem ? kDescentHalf * d.map_aspect : kDescentHalf;
}

// Fit the focused ecosystem's body-mass axis (x) to the community's own mass
// range and to the map area's width, so species spread across the stage
// instead of crowding a square in its middle (x keeps its meaning: relative
// log body mass). Cached per focus seed and map aspect.
void ensure_eco_fit(DescentState& d) {
    const ProcNode& f = d.focus();
    if (f.kind != NodeKind::Ecosystem) return;
    if (d.eco_fit_seed == f.seed && std::abs(d.eco_fit_aspect - d.map_aspect) < 1.0e-3f) return;
    float lo = 1.0e30f, hi = -1.0e30f;
    for (const ChildRef& c : f.children) { lo = std::min(lo, c.x); hi = std::max(hi, c.x); }
    const float span = hi - lo;
    const float target = 2.0f * 0.80f * layout_half_x(d); // use 80% of the map width
    d.eco_x_mid = f.children.empty() ? 0.0f : 0.5f * (lo + hi);
    d.eco_x_scale = span > 1.0e-3f ? std::clamp(target / span, 1.0f, 8.0f) : 1.0f;
    d.eco_fit_seed = f.seed;
    d.eco_fit_aspect = d.map_aspect;
}

// Layout of one of the FOCUSED node's children, including the ecosystem fit.
// Every consumer (hover/pick, minimap, H, ascend recentering) goes through
// here so they always agree on where a child is.
void focus_child_layout(DescentState& d, const ChildRef& c, float t, float& nx, float& ny) {
    child_layout(c, d.focus_kind(), t, nx, ny);
    if (d.focus_kind() == NodeKind::Ecosystem) {
        ensure_eco_fit(d);
        nx += (c.x - d.eco_x_mid) * d.eco_x_scale - c.x; // stretch the base, keep the wander
    }
}

// Single per-frame layout pass: compute every child's screen position once into
// the reusable buffers (read by hover/pick in update, and links/sprites in draw).
void fill_child_positions(CosmosState& cosmos, Rectangle stage, float t) {
    DescentState& d = cosmos.descent;
    {
        const Rectangle L = layout_rect(d, stage);
        d.map_aspect = std::clamp(L.width / std::max(1.0f, L.height), 1.0f, 2.6f);
    }
    const auto& kids = d.focus().children;
    const std::size_t n = kids.size();
    d.child_px.resize(n);
    d.child_py.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        float nx, ny;
        focus_child_layout(d, kids[i], t, nx, ny);
        const Vector2 p = layout_to_screen(nx, ny, layout_rect(d, stage), d.camera);
        d.child_px[i] = p.x;
        d.child_py[i] = p.y;
    }
}

inline Vector2 child_pos(const DescentState& d, int i) {
    return {d.child_px[static_cast<std::size_t>(i)], d.child_py[static_cast<std::size_t>(i)]};
}

// Renderer guard for the drawn sprite set. Kept far above the engine's maximum
// children per node (60) so the drawn set, hover, pick, and food-web links all
// cover the same objects — a child must never be enterable but invisible.
constexpr int kMaxDrawnChildren = 256;

// Interaction state is per-node: any change of focus (enter, ascend, jump,
// sibling hop, reseed) must drop it so a stale index never leaks into the newly
// focused node's children (e.g. the selection reticle snapping to an arbitrary
// child, or an ecosystem cross-highlighting the wrong species).
void reset_node_interaction(DescentState& d) {
    d.hovered_child = -1;
    d.hover_locked = -1;
    d.selected_child = -1;
    d.focus_species = -1;
    d.descend_hint = -1;
    d.zoom_anchor = {-1.0f, -1.0f};
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
    reset_node_interaction(d);
    // A reseed is a new universe: drop the live sim and its time controls, and
    // invalidate the per-universe render caches.
    d.live_seed = 0;
    d.sim_paused = false;
    d.sim_speed = 1.0;
    d.sim_step_once = false;
    d.web_census_seed = 0;
    d.web_edges_seed = 0;
    d.eco_fit_seed = 0;
    d.camera = CosmosCamera{};
    d.transition = 1.0f;
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
    // Pass the current focus as parent (stable copy in the path) so context-aware
    // generation (planet habitability, ecosystem climate) sees its star/planet.
    const ProcNode* parent = &d.path.back();
    ProcNode cn = d.universe->node(child.seed, child.kind, parent); // copy (engine ref may move)
    d.path.push_back(std::move(cn));
    reset_node_interaction(d);
    // Zoom-through: the new level appears at about half size and the camera
    // smoothing carries the inward motion to the framed zoom (1.0, where the
    // layout fits the stage), so entering reads as one continuous dive. Landing
    // framed also keeps a single scroll-out notch from bouncing straight back
    // up a level (the ascend threshold is kZoomMin).
    d.camera.target_zoom = 1.0;
    d.camera.zoom = kZoomMin * 1.1;
    d.camera.target_pan = d.camera.pan = Vec2{};
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
}

void descent_pop(CosmosState& cosmos) {
    DescentState& d = cosmos.descent;
    if (d.depth() <= 0) {
        return;
    }
    const std::uint64_t exited = d.focus().seed;
    d.path.pop_back();
    reset_node_interaction(d);
    // Land zoomed-in, recentered on the child we came from — at its current
    // animated position (planets orbit), not its phase-0 layout slot — so it
    // reads as pulling back out of where we were. Start past the resting zoom
    // and let the smoothing continue the outward motion across the boundary.
    d.camera.target_zoom = kZoomMax * 0.95;
    d.camera.zoom = kZoomMax * 1.30;
    Vec2 p{};
    for (const ChildRef& c : d.focus().children) {
        if (c.seed == exited) {
            float nx, ny;
            focus_child_layout(d, c, static_cast<float>(GetTime()), nx, ny);
            p = {nx, ny};
            break;
        }
    }
    d.camera.target_pan = d.camera.pan = p;
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
}

void descent_jump_to_depth(CosmosState& cosmos, int depth) {
    DescentState& d = cosmos.descent;
    depth = std::clamp(depth, 0, d.depth());
    d.path.resize(static_cast<std::size_t>(depth) + 1);
    reset_node_interaction(d);
    d.camera.target_zoom = d.camera.zoom = 1.0;
    d.camera.target_pan = d.camera.pan = Vec2{};
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
}

void descent_sibling(CosmosState& cosmos, int dir) {
    DescentState& d = cosmos.descent;
    if (d.depth() <= 0) return;
    const std::uint64_t cur = d.focus().seed;
    const ProcNode& parent = d.path[static_cast<std::size_t>(d.depth() - 1)];
    const int n = static_cast<int>(parent.children.size());
    int idx = -1;
    for (int k = 0; k < n; ++k)
        if (parent.children[static_cast<std::size_t>(k)].seed == cur) { idx = k; break; }
    if (idx < 0 || n <= 1) return;
    const int ni = ((idx + dir) % n + n) % n;
    d.path.pop_back();
    const ProcNode& par = d.path.back();
    const ChildRef child = par.children[static_cast<std::size_t>(ni)];
    ProcNode cn = d.universe->node(child.seed, child.kind, &par);
    d.path.push_back(std::move(cn));
    reset_node_interaction(d);
    d.live_seed = 0;
    d.camera.target_zoom = d.camera.zoom = 1.0;
    d.camera.target_pan = d.camera.pan = Vec2{};
    d.camera.flash = 1.0f;
    d.transition = 0.0f;
}

namespace {
// Move the selection cursor to the nearest child in a screen direction (dx,dy).
int select_directional(const DescentState& d, int from, float dirx, float diry) {
    const int n = static_cast<int>(d.child_px.size());
    if (n == 0) return -1;
    float ox, oy;
    if (from >= 0 && from < n) { ox = d.child_px[static_cast<std::size_t>(from)]; oy = d.child_py[static_cast<std::size_t>(from)]; }
    else {
        ox = 0.0f; oy = 0.0f;
        for (int i = 0; i < n; ++i) { ox += d.child_px[static_cast<std::size_t>(i)]; oy += d.child_py[static_cast<std::size_t>(i)]; }
        ox /= n; oy /= n;
    }
    int best = -1;
    float best_score = 1e30f;
    for (int i = 0; i < n; ++i) {
        if (i == from) continue;
        const float vx = d.child_px[static_cast<std::size_t>(i)] - ox;
        const float vy = d.child_py[static_cast<std::size_t>(i)] - oy;
        const float along = vx * dirx + vy * diry;     // projection onto the direction
        if (along <= 1.0f) continue;                    // must be in the pressed direction
        const float perp = std::abs(vx * diry - vy * dirx);
        const float score = along + perp * 2.5f;         // prefer aligned + near
        if (score < best_score) { best_score = score; best = i; }
    }
    return best;
}
} // namespace

void update_descent(CosmosState& cosmos, Rectangle stage, float dt, bool interactive, float ui) {
    descent_ensure_init(cosmos);
    DescentState& d = cosmos.descent;
    CosmosCamera& cam = d.camera;
    // Reserve the deck's strip of the stage (animated; snaps on the first frame).
    const float reserve_target = d.analysis_open ? descent_deck_height(stage, ui) + kDescentDeckGap * ui * 2.0f : 0.0f;
    if (d.deck_reserve < 0.0f) d.deck_reserve = reserve_target;
    d.deck_reserve += (reserve_target - d.deck_reserve) * static_cast<float>(1.0 - std::exp(-10.0 * std::max(0.0001f, dt)));
    const Rectangle L = layout_rect(d, stage);
    const float sc0 = base_scale(L);
    const Vector2 ctr = {L.x + L.width * 0.5f, L.y + L.height * 0.5f};
    const float t = static_cast<float>(GetTime());

    if (interactive) {
        // Input over the map area only (the deck strip below is not the map).
        const bool over = CheckCollisionPointRec(GetMousePosition(), L);
        // Layout pass up front so the keyboard selection cursor and the mouse hover
        // share one set of child screen positions.
        fill_child_positions(cosmos, stage, t);

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
            // Remember where the zoom-in is aimed: the descend pick targets this
            // point, so the child under the cursor is the child you enter.
            d.zoom_anchor = (wheel > 0.0f) ? m : Vector2{-1.0f, -1.0f};
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

        // Pan with WASD (arrows drive the selection cursor); zoom with +/-.
        const double pan_step = (4.0 * dt) / std::max(0.35, cam.target_zoom);
        if (IsKeyDown(KEY_A)) cam.target_pan.x -= pan_step;
        if (IsKeyDown(KEY_D)) cam.target_pan.x += pan_step;
        if (IsKeyDown(KEY_W)) cam.target_pan.y -= pan_step;
        if (IsKeyDown(KEY_S)) cam.target_pan.y += pan_step;
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD))      cam.target_zoom *= std::exp(2.2 * dt);
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) cam.target_zoom *= std::exp(-2.2 * dt);
        if (IsKeyPressed(KEY_C)) { // recenter the current level without changing depth
            cam.target_pan = Vec2{};
            d.zoom_anchor = {-1.0f, -1.0f};
        }
        // Keep the camera centre inside the populated layout square so content
        // can never be panned irretrievably off-stage.
        cam.target_pan.x = std::clamp(cam.target_pan.x, -static_cast<double>(layout_half_x(d)),
                                      static_cast<double>(layout_half_x(d)));
        cam.target_pan.y = std::clamp(cam.target_pan.y, -static_cast<double>(kDescentHalf),
                                      static_cast<double>(kDescentHalf));

        // ── Selection cursor (keyboard): directional arrows, Tab cycle, number
        //    quick-jump, Enter to enter, [ ] siblings, Home to root. ────────────
        const int nchild = static_cast<int>(d.focus().children.size());
        if (nchild > 0) {
            int s = -1;
            if (IsKeyPressed(KEY_RIGHT)) s = select_directional(d, d.selected_child, 1.0f, 0.0f);
            else if (IsKeyPressed(KEY_LEFT))  s = select_directional(d, d.selected_child, -1.0f, 0.0f);
            else if (IsKeyPressed(KEY_DOWN))  s = select_directional(d, d.selected_child, 0.0f, 1.0f);
            else if (IsKeyPressed(KEY_UP))    s = select_directional(d, d.selected_child, 0.0f, -1.0f);
            if (s >= 0) d.selected_child = s;
            if (IsKeyPressed(KEY_TAB)) {
                const int step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? -1 : 1;
                const int cur = d.selected_child < 0 ? 0 : d.selected_child;
                d.selected_child = (cur + step + nchild) % nchild;
            }
            for (int k = 0; k < 9 && k < nchild; ++k)
                if (IsKeyPressed(KEY_ONE + k)) d.selected_child = k;
            if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && d.selected_child >= 0 &&
                !node_is_leaf(d.focus_kind())) {
                descent_push(cosmos, d.selected_child);
            }
        }
        // H: aim at the next notable child — habitable worlds in a star system,
        // the keystone species in an ecosystem — and pan the camera onto it.
        if (IsKeyPressed(KEY_H) && nchild > 0) {
            int pick = -1;
            if (d.focus_kind() == NodeKind::StarSystem) {
                for (int off = 1; off <= nchild; ++off) { // cycle from the cursor
                    const int i = ((d.selected_child < 0 ? -1 : d.selected_child) + off + nchild) % nchild;
                    if (d.focus().children[static_cast<std::size_t>(i)].habitable) { pick = i; break; }
                }
            } else if (d.focus_kind() == NodeKind::Ecosystem && d.live_seed == d.focus().seed) {
                const int ks = d.live.community.stats.keystone;
                if (ks >= 0 && ks < nchild) pick = ks;
            }
            if (pick >= 0) {
                d.selected_child = pick;
                float nx, ny;
                focus_child_layout(d, d.focus().children[static_cast<std::size_t>(pick)], t, nx, ny);
                cam.target_pan = {nx, ny};
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE)) descent_pop(cosmos);
        if (IsKeyPressed(KEY_HOME)) descent_jump_to_depth(cosmos, 0);
        if (IsKeyPressed(KEY_LEFT_BRACKET))  descent_sibling(cosmos, -1);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) descent_sibling(cosmos, 1);
        if (IsKeyPressed(KEY_G)) d.analysis_open = !d.analysis_open;

        // ── Simulation time controls: pause, scrub speed, single step, perturb. ─
        if (IsKeyPressed(KEY_SPACE)) d.sim_paused = !d.sim_paused;
        if (IsKeyPressed(KEY_PERIOD)) d.sim_speed = std::min(16.0, d.sim_speed * 2.0);
        if (IsKeyPressed(KEY_COMMA))  d.sim_speed = std::max(0.25, d.sim_speed * 0.5);
        if (IsKeyPressed(KEY_O)) d.sim_step_once = true;
        if (IsKeyPressed(KEY_X) && d.focus_species >= 0 &&
            d.focus_species < static_cast<int>(d.live.community.species.size())) {
            d.live.community.species[static_cast<std::size_t>(d.focus_species)].x *= 0.2; // perturb -> watch recovery
            d.perturb_flash = 1.0;
        }

        // O(1) spatial-hash hover with hysteresis (no flicker when two children are
        // near-equidistant; scales to thousands of children).
        d.hovered_child = -1;
        if (!cam.dragging && over) {
            const float radius = 26.0f * std::max(1.0f, static_cast<float>(cam.zoom) * 0.5f);
            static SpatialHash hash;
            hash.build(d.child_px, d.child_py, std::max(10.0f, radius));
            const Vector2 mouse = GetMousePosition();
            int cand = hash.nearest(mouse.x, mouse.y, radius);
            const int n = static_cast<int>(d.child_px.size());
            // Hysteresis: keep the locked child while it stays in range unless the
            // candidate is meaningfully (8 px) closer.
            if (d.hover_locked >= 0 && d.hover_locked < n) {
                const Vector2 lp = child_pos(d, d.hover_locked);
                const float dl = std::hypot(lp.x - mouse.x, lp.y - mouse.y);
                if (dl < radius * 1.25f) {
                    if (cand >= 0 && cand != d.hover_locked) {
                        const Vector2 cp = child_pos(d, cand);
                        const float dc = std::hypot(cp.x - mouse.x, cp.y - mouse.y);
                        if (dc < dl - 8.0f) d.hover_locked = cand;
                    }
                } else {
                    d.hover_locked = cand;
                }
            } else {
                d.hover_locked = cand;
            }
            d.hovered_child = d.hover_locked;
            if (d.hovered_child >= 0) d.selected_child = d.hovered_child; // unify mouse + keyboard cursor
            // Click a hovered child: enter a non-leaf, or focus a species in an ecosystem.
            if (d.hovered_child >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (d.focus_kind() == NodeKind::Ecosystem) d.focus_species = d.hovered_child;
                else if (!node_is_leaf(d.focus_kind())) descent_push(cosmos, d.hovered_child);
            }
        } else {
            d.hover_locked = -1;
        }
    } else {
        fill_child_positions(cosmos, stage, t);
    }

    // Descend pick, shared by the trigger below and the "about to enter"
    // highlight in draw: the child nearest the zoom anchor (the cursor point of
    // the last wheel zoom-in, else the stage centre), within reach of it.
    d.descend_hint = -1;
    if (!node_is_leaf(d.focus_kind()) && !d.focus().children.empty()) {
        const bool anchored = d.zoom_anchor.x >= 0.0f &&
                              CheckCollisionPointRec(d.zoom_anchor, L);
        const Vector2 aim = anchored ? d.zoom_anchor : ctr;
        int best = -1;
        float best_d = 1.0e30f;
        for (int i = 0; i < static_cast<int>(d.child_px.size()); ++i) {
            const Vector2 p = child_pos(d, i);
            const float dist = std::hypot(p.x - aim.x, p.y - aim.y);
            if (dist < best_d) { best_d = dist; best = i; }
        }
        const float reach = 0.32f * std::min(L.width, L.height);
        if (best >= 0 && best_d < reach) d.descend_hint = best;
    }

    // Descend / ascend on zoom bounds.
    if (cam.target_zoom > kZoomMax) {
        if (d.descend_hint >= 0) {
            descent_push(cosmos, d.descend_hint);
        } else {
            cam.target_zoom = kZoomMax; // nothing to enter; hold at the edge
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

    // Live ecosystem: build on entry, then advance with a fixed timestep so the
    // food web breathes (predators lag prey) smoothly and FPS-independently, while
    // recording population history for the observability sparklines.
    // Effective simulation timestep from the granular time controls.
    double sdt = d.sim_paused ? 0.0 : dt * d.sim_speed;
    if (d.sim_step_once) { sdt = d.live.clock.H; d.sim_step_once = false; }
    d.perturb_flash = std::max(0.0, d.perturb_flash - dt * 1.5);

    if (d.focus_kind() == NodeKind::Ecosystem) {
        if (d.live_seed != d.focus().seed) {
            // Reuse the engine's memoized community instead of rebuilding the
            // O(S^2) phylogeny + food web from scratch on every entry.
            ecosim::init_live(d.live, d.universe->community_cached(d.focus()));
            d.live_seed = d.focus().seed;
            d.focus_species = -1;
        }
        // The selection cursor doubles as the focused species for cross-highlight.
        if (d.selected_child >= 0 && d.selected_child < static_cast<int>(d.live.community.species.size()))
            d.focus_species = d.selected_child;
        ecosim::advance(d.live, sdt);
    } else if (d.focus_kind() == NodeKind::Creature && d.depth() > 0) {
        // Keep the parent ecosystem's community live so a creature is always shown
        // in its ecological context (and the analysis deck has its data), no matter
        // how the node was reached.
        const ProcNode& ecop = d.path[static_cast<std::size_t>(d.depth() - 1)];
        if (ecop.kind == NodeKind::Ecosystem) {
            if (d.live_seed != ecop.seed) {
                ecosim::init_live(d.live, d.universe->community_cached(ecop));
                d.live_seed = ecop.seed;
            }
            ecosim::advance(d.live, sdt);
        }
    }
}

// ── Rendering ────────────────────────────────────────────────────────────────

void draw_descent_stage(CosmosState& cosmos, Renderer& renderer, Rectangle stage, float ui) {
    DescentState& d = cosmos.descent;
    const CosmosCamera& cam = d.camera;
    const ProcNode& f = d.focus();
    const float t = static_cast<float>(GetTime());

    draw_universe_backdrop(cosmos.palette, cosmos.genome.signature ^ (f.seed * 0x9E3779B9ull),
                           stage, t);

    // Single layout pass for this frame (post-smoothing camera): hover/pick used
    // the pre-smoothing positions in update; here we recompute once for rendering.
    fill_child_positions(cosmos, stage, t);

    const Rectangle L = layout_rect(d, stage); // map area above the deck
    const float zoomf = static_cast<float>(cam.zoom);
    const float sc = base_scale(L) * zoomf;
    const bool eco_live = (f.kind == NodeKind::Ecosystem && d.live_seed == f.seed);

    // Orbit rings + central body (drawn under the additive field pass).
    BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                     static_cast<int>(stage.width), static_cast<int>(stage.height));
    const Vector2 center = layout_to_screen(0.0f, 0.0f, L, cam);
    if (f.kind == NodeKind::Universe) {
        // Cosmic web: link each galaxy to its two nearest neighbours, tracing the
        // filaments that thread the clusters (galaxies were placed on the density
        // field's peaks, so this proximity graph reads as the large-scale web).
        // Galaxy layout is static, so the O(n^2) neighbour topology is built once
        // per universe and only re-projected through the camera each frame.
        if (d.web_edges_seed != f.seed) {
            d.web_edges.clear();
            const int gn = static_cast<int>(f.children.size());
            for (int i = 0; i < gn; ++i) {
                const ChildRef& ci = f.children[static_cast<std::size_t>(i)];
                int b1 = -1, b2 = -1;
                float d1 = 1e30f, d2 = 1e30f;
                for (int j = 0; j < gn; ++j) {
                    if (j == i) continue;
                    const ChildRef& cj = f.children[static_cast<std::size_t>(j)];
                    const float dx = ci.x - cj.x;
                    const float dy = ci.y - cj.y;
                    const float dd = dx * dx + dy * dy;
                    if (dd < d1) { d2 = d1; b2 = b1; d1 = dd; b1 = j; }
                    else if (dd < d2) { d2 = dd; b2 = j; }
                }
                for (int b : {b1, b2}) {
                    if (b > i) d.web_edges.emplace_back(i, b); // each strand once
                }
            }
            d.web_edges_seed = f.seed;
        }
        const int gn = static_cast<int>(d.child_px.size());
        for (const auto& e : d.web_edges) {
            if (e.first >= gn || e.second >= gn) continue;
            const Vector2 pa = child_pos(d, e.first);
            const Vector2 pb = child_pos(d, e.second);
            const float len = std::hypot(pa.x - pb.x, pa.y - pb.y);
            // Nearer galaxies sit on a denser filament -> brighter strand.
            const unsigned char a = static_cast<unsigned char>(std::clamp(46.0f - len * 0.06f, 8.0f, 46.0f));
            DrawLineEx(pa, pb, 1.1f, with_alpha(WL::CYAN_DIM, a));
        }
    } else if (f.kind == NodeKind::Galaxy) {
        // Morphology-accurate structure under the star field: a halo glow plus
        // log-spiral arms (spiral) or concentric isophotes (elliptical).
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), 0.85f * sc,
                           palette_color(f.color, 24), Color{0, 0, 0, 0});
        if (f.subtype == 0) { // spiral
            const int arms = 2;
            for (int aidx = 0; aidx < arms; ++aidx) {
                const float base = static_cast<float>(aidx) * (6.2831853f / arms) + t * 0.02f;
                Vector2 prev{};
                for (float th = 0.0f; th < 3.4f; th += 0.12f) {
                    const float rr = 0.06f * std::exp(0.42f * th);
                    if (rr > 0.95f) break;
                    const float ang = base + th;
                    const Vector2 p = layout_to_screen(std::cos(ang) * rr, std::sin(ang) * rr, L, cam);
                    if (th > 0.0f) DrawLineEx(prev, p, 1.5f, with_alpha(to_raylib(f.color), 60));
                    prev = p;
                }
            }
        } else if (f.subtype == 1) { // elliptical isophotes
            for (int k = 1; k <= 4; ++k) {
                const float rr = 0.22f * static_cast<float>(k);
                DrawEllipseLines(static_cast<int>(center.x), static_cast<int>(center.y),
                                 0.92f * rr * sc, 0.60f * rr * sc, with_alpha(to_raylib(f.color), 52));
            }
        }
    } else if (f.kind == NodeKind::StarSystem) {
        // Map orbital distance (AU) to the normalized ring layout so the habitable
        // zone and the snow line can be drawn where they physically fall.
        const double lum = std::max(0.02, f.luminosity);
        const int cnt = std::max(1, static_cast<int>(f.children.size()));
        const double base_au = 0.25 * std::sqrt(lum);
        const double step = 0.80 / cnt;
        auto au_to_r = [&](double au) {
            const double i = std::log(std::max(1e-6, au / base_au)) / std::log(1.6);
            return static_cast<float>((0.18 + i * step) * sc);
        };
        const astro::HabitableZone hz = astro::habitable_zone(lum);
        const float r_in = au_to_r(hz.inner_au), r_out = au_to_r(hz.outer_au);
        if (r_out > r_in && r_out > 0.0f)
            DrawRing(center, std::max(0.0f, r_in), r_out, 0.0f, 360.0f, 96,
                     with_alpha(WL::PLASMA_GREEN, 24));
        // Snow line (~2.7 AU scaled by sqrt(L)): beyond it, volatiles condense.
        const float r_snow = au_to_r(2.7 * std::sqrt(lum));
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), r_snow,
                        with_alpha(WL::CYAN_DIM, 80));
        // Stellar corona.
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y),
                           0.10f * sc + 14.0f, palette_color(f.color, 60), Color{0, 0, 0, 0});
        for (const ChildRef& c : f.children) {
            if (c.orbit > 1.0e-4f) {
                DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                                c.orbit * sc, with_alpha(WL::GLASS_BORDER, 70));
            }
        }
    } else if (f.kind == NodeKind::Planet) {
        // The world as a soft globe: a body disc, an atmospheric limb, a sunlit
        // day side, latitude guides, and marked poles.
        const float pr = 0.46f * sc;
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y),
                           pr, palette_color(f.color, 60), Color{0, 0, 0, 0});
        // Atmosphere: an outer glow ring + crisp limb.
        DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), pr + 8.0f,
                           with_alpha(WL::CYAN_DIM, 26), Color{0, 0, 0, 0});
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), pr,
                        with_alpha(WL::GLASS_BORDER, 80));
        // Sunlit day side: an offset highlight toward the upper-left.
        DrawCircleGradient(static_cast<int>(center.x - pr * 0.32f),
                           static_cast<int>(center.y - pr * 0.32f), pr * 0.7f,
                           with_alpha(WL::GLASS_HILIGHT, 30), Color{0, 0, 0, 0});
        // Poles.
        DrawCircleV({center.x, center.y - pr}, 2.4f, with_alpha(WL::TEXT_TERTIARY, 180));
        DrawCircleV({center.x, center.y + pr}, 2.4f, with_alpha(WL::TEXT_TERTIARY, 180));
        const int latv[5] = {60, 30, 0, -30, -60};
        for (int i = 0; i < 5; ++i) {
            const float ny = -static_cast<float>(latv[i]) / 110.0f;
            const Vector2 p = layout_to_screen(0.0f, ny, L, cam);
            const float halfw = 0.46f * sc * std::cos(static_cast<float>(latv[i]) * 3.14159f / 180.0f);
            const unsigned char a = (latv[i] == 0) ? 90 : 36;
            DrawLineEx({center.x - halfw, p.y}, {center.x + halfw, p.y}, 1.0f,
                       with_alpha(WL::CYAN_DIM, a));
        }
    } else if (eco_live) {
        // Trophic strata guides: producers low -> apex high, so the vertical food
        // web reads as an energy pyramid. Labels sit at the left margin.
        const char* tl[5] = {"APEX", "CARNIVORES", "OMNIVORES", "HERBIVORES", "PRODUCERS"};
        const float ny5[5] = {-0.82f, -0.41f, 0.0f, 0.41f, 0.82f};
        for (int i = 0; i < 5; ++i) {
            const Vector2 p = layout_to_screen(0.0f, ny5[i], L, cam);
            if (p.y < L.y + 4.0f || p.y > L.y + L.height - 4.0f) continue;
            DrawLineEx({stage.x + 6.0f, p.y}, {stage.x + stage.width - 6.0f, p.y}, 1.0f,
                       with_alpha(WL::GLASS_BORDER, 30));
            draw_text(tl[i], {stage.x + 10.0f, p.y - 12.0f * ui}, 9.0f * ui,
                      with_alpha(WL::TEXT_TERTIARY, 110));
        }
        // Food web: links from prey up to predator (energy-flow direction), with a
        // soft glow and a brightness that pulses with the live predation flux.
        const int ns = static_cast<int>(d.child_px.size());
        for (const eco::Link& lk : d.live.community.links) {
            if (lk.pred >= ns || lk.prey >= ns) continue;
            const Vector2 a = child_pos(d, lk.prey);
            const Vector2 b = child_pos(d, lk.pred);
            const Color pc = to_raylib(f.children[static_cast<std::size_t>(lk.pred)].color);
            const float flux = (lk.prey < static_cast<int>(d.live.community.species.size()))
                               ? static_cast<float>(ecosim::render_x(d.live, lk.prey) /
                                   std::max(1.0e-9, d.live.community.species[static_cast<std::size_t>(lk.prey)].xeq))
                               : 1.0f;
            const unsigned char la = static_cast<unsigned char>(
                std::clamp(18.0 + 120.0 * lk.pref * std::min(1.5f, flux), 0.0, 200.0));
            DrawLineEx(a, b, 1.3f, with_alpha(pc, la));
        }
    }
    EndScissorMode();

    // Build the bounded sprite set.
    std::vector<Renderer::FieldSprite> sprites;
    sprites.reserve(f.children.size() + 1);
    const unsigned char child_alpha =
        static_cast<unsigned char>(std::clamp(70.0f + 185.0f * d.transition, 0.0f, 255.0f));

    // A central body for levels that have one: the star, the creature specimen,
    // or a galaxy's bright bulge/SMBH core.
    if (f.kind == NodeKind::StarSystem || f.kind == NodeKind::Creature ||
        f.kind == NodeKind::Galaxy) {
        Renderer::FieldSprite s;
        s.pos = center;
        const float core_r = f.kind == NodeKind::StarSystem ? 9.0f
                           : f.kind == NodeKind::Creature ? 16.0f : 11.0f;
        s.radius = std::clamp(core_r * std::sqrt(zoomf), 4.0f, 40.0f);
        s.color = f.kind == NodeKind::Galaxy ? lighten(to_raylib(f.color), 0.4f) : to_raylib(f.color);
        sprites.push_back(s);
    }

    const int n = std::min(static_cast<int>(f.children.size()), kMaxDrawnChildren);
    for (int i = 0; i < n; ++i) {
        const ChildRef& c = f.children[static_cast<std::size_t>(i)];
        Renderer::FieldSprite s;
        s.pos = child_pos(d, i);
        const float kind_scale = (f.kind == NodeKind::Universe || f.kind == NodeKind::Galaxy) ? 6.0f
                               : (f.kind == NodeKind::Ecosystem) ? 5.0f : 5.5f;
        float pulse = 1.0f;
        if (eco_live && i < static_cast<int>(d.live.community.species.size())) {
            // Interpolated + low-pass smoothed pulse: never flickers on population change.
            const double xeq = d.live.community.species[static_cast<std::size_t>(i)].xeq;
            pulse = static_cast<float>(ecosim::smoothed_pulse(ecosim::render_x(d.live, i), xeq));
        }
        s.radius = std::clamp((2.0f + c.size * kind_scale) * std::sqrt(zoomf) * pulse, 1.5f, 26.0f);
        Color col = to_raylib(c.color);
        col.a = child_alpha;
        s.color = col;
        sprites.push_back(s);
    }

    const bool animated = (f.kind == NodeKind::StarSystem || f.kind == NodeKind::Ecosystem);
    const unsigned char fade = (animated && d.transition > 0.3f) ? 30 : 255;
    renderer.accumulate_field(sprites, stage, fade);
    renderer.draw_field(sprites, stage);

    // Mark habitable (living) worlds in a star system with a green ring.
    if (f.kind == NodeKind::StarSystem) {
        BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                         static_cast<int>(stage.width), static_cast<int>(stage.height));
        for (int i = 0; i < n; ++i) {
            if (!f.children[static_cast<std::size_t>(i)].habitable) continue;
            const Vector2 p = child_pos(d, i);
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 9.0f * ui,
                            with_alpha(WL::PLASMA_GREEN, 230));
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 12.0f * ui,
                            with_alpha(WL::PLASMA_GREEN, 90));
        }
        EndScissorMode();
    }

    // Creature specimen viewer: concentric reticle rings + crosshair around the
    // single organism, so the leaf reads as a specimen under examination.
    if (f.kind == NodeKind::Creature) {
        const float rr = 34.0f * ui + 10.0f * ui * (0.5f + 0.5f * std::sin(t * 1.5f));
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), rr,
                        with_alpha(WL::CYAN_DIM, 90));
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), rr + 8.0f * ui,
                        with_alpha(WL::CYAN_DIM, 40));
        DrawLineEx({center.x - rr - 10.0f * ui, center.y}, {center.x - rr + 4.0f * ui, center.y},
                   1.2f, with_alpha(WL::CYAN_CORE, 150));
        DrawLineEx({center.x + rr - 4.0f * ui, center.y}, {center.x + rr + 10.0f * ui, center.y},
                   1.2f, with_alpha(WL::CYAN_CORE, 150));
        DrawLineEx({center.x, center.y - rr - 10.0f * ui}, {center.x, center.y - rr + 4.0f * ui},
                   1.2f, with_alpha(WL::CYAN_CORE, 150));
        DrawLineEx({center.x, center.y + rr - 4.0f * ui}, {center.x, center.y + rr + 10.0f * ui},
                   1.2f, with_alpha(WL::CYAN_CORE, 150));
        draw_text("SPECIMEN", {center.x - 26.0f * ui, center.y + rr + 14.0f * ui}, 9.5f * ui,
                  with_alpha(WL::TEXT_TERTIARY, 170));
    }

    // Ecosystem trophic-role colour legend (bottom-left of the stage).
    if (eco_live) {
        const char* rl[3] = {"producer", "herbivore", "carnivore"};
        const Color rc[3] = {WL::PLASMA_GREEN, WL::CYAN_CORE, {255, 92, 92, 255}};
        const float ly = stage_content_bottom(d, stage, ui) - 56.0f * ui;
        for (int i = 0; i < 3; ++i) {
            const float lx = stage.x + 14.0f * ui + i * 92.0f * ui;
            DrawCircleV({lx, ly + 5.0f * ui}, 4.0f * ui, with_alpha(rc[i], 220));
            draw_text(rl[i], {lx + 9.0f * ui, ly}, 9.5f * ui, with_alpha(WL::TEXT_TERTIARY, 200));
        }
    }

    // Instrument frame: engineered corner brackets + a slow vertical scan sweep.
    draw_corner_brackets(stage, with_alpha(WL::CYAN_DIM, 120), 18.0f * ui, 1.4f, 3.0f);
    {
        const float sweep = stage.y + (0.5f + 0.5f * std::sin(t * 0.35f)) * stage.height;
        DrawLineEx({stage.x + 4.0f, sweep}, {stage.x + stage.width - 4.0f, sweep}, 1.0f,
                   with_alpha(WL::CYAN_CORE, 16));
    }

    // Live-simulation badge for an active ecosystem.
    if (eco_live) {
        const float pa = 0.5f + 0.5f * std::sin(t * 3.0f);
        const Rectangle lb = {stage.x + stage.width - 78.0f * ui, stage.y + 12.0f * ui,
                              66.0f * ui, 20.0f * ui};
        draw_glass_panel(lb, {8, 22, 16, 210}, with_alpha(WL::PLASMA_GREEN, 120), 0.4f, 2);
        DrawCircleV({lb.x + 12.0f * ui, lb.y + lb.height * 0.5f}, 4.0f * ui,
                    with_alpha(WL::PLASMA_GREEN, static_cast<unsigned char>(140 + 115 * pa)));
        draw_text("LIVE", {lb.x + 22.0f * ui, lb.y + 4.0f * ui}, 11.0f * ui, WL::PLASMA_GREEN);
    }

    // Selection cursor reticle (keyboard/mouse), distinct from a bare hover: a
    // bright animated bracket + index, so the focused child is unmistakable.
    if (d.selected_child >= 0 && d.selected_child < static_cast<int>(d.child_px.size())) {
        const Vector2 p = child_pos(d, d.selected_child);
        const float rs = (14.0f + 2.0f * std::sin(t * 4.0f)) * ui;
        const Color sc_col = (eco_live && d.selected_child == d.focus_species) ? WL::PLASMA_GREEN : WL::CYAN_CORE;
        draw_corner_brackets({p.x - rs, p.y - rs, rs * 2.0f, rs * 2.0f}, sc_col, 6.0f * ui, 2.0f, 0.0f);
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), rs + 3.0f * ui, with_alpha(sc_col, 70));
    }

    // "About to enter" highlight: as the zoom-in approaches the descend
    // threshold, ring the child the camera will actually enter (the same pick
    // the trigger uses), so the hand-off is never a surprise.
    if (d.descend_hint >= 0 && d.descend_hint < static_cast<int>(d.child_px.size()) &&
        !node_is_leaf(f.kind)) {
        const float prog = static_cast<float>((cam.target_zoom - kZoomMax * 0.80) /
                                              (kZoomMax * 0.20));
        if (prog > 0.0f) {
            const float a01 = std::min(prog, 1.0f);
            const Vector2 p = child_pos(d, d.descend_hint);
            const float rr = (20.0f - 5.0f * a01 + 2.0f * std::sin(t * 6.0f)) * ui;
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), rr,
                            with_alpha(WL::XENON_CORE, static_cast<unsigned char>(200 * a01)));
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), rr + 4.0f * ui,
                            with_alpha(WL::XENON_CORE, static_cast<unsigned char>(80 * a01)));
            if (a01 > 0.5f)
                draw_text("ENTERING", {p.x - 25.0f * ui, p.y - rr - 14.0f * ui}, 9.0f * ui,
                          with_alpha(WL::XENON_CORE, static_cast<unsigned char>(220 * a01)));
        }
    }

    // Minimap: once the camera is zoomed in enough for children to leave the
    // stage, show the whole level with the current viewport marked, so panning
    // deep into a level never means losing the overview.
    if (zoomf > 1.55f && !f.children.empty()) {
        const float ms = 104.0f * ui;
        const Rectangle mm = {stage.x + stage.width - ms - 12.0f * ui,
                              stage.y + (eco_live ? 44.0f : 12.0f) * ui, ms, ms};
        draw_glass_panel(mm, {6, 14, 24, 205}, with_alpha(WL::CYAN_DIM, 110), 0.25f, 2);
        const float hx = layout_half_x(d);
        const auto to_mm = [&](float nx, float ny) -> Vector2 {
            return {mm.x + (nx / hx * 0.5f + 0.5f) * mm.width,
                    mm.y + (ny / kDescentHalf * 0.5f + 0.5f) * mm.height};
        };
        BeginScissorMode(static_cast<int>(mm.x), static_cast<int>(mm.y),
                         static_cast<int>(mm.width), static_cast<int>(mm.height));
        for (int i = 0; i < n; ++i) {
            const ChildRef& c = f.children[static_cast<std::size_t>(i)];
            float nx, ny;
            focus_child_layout(d, c, t, nx, ny);
            const Vector2 p = to_mm(nx, ny);
            DrawCircleV(p, std::max(1.0f, 1.4f * ui),
                        with_alpha(to_raylib(c.color), i == d.selected_child ? 255 : 170));
        }
        // Current viewport in layout space, mapped into the minimap.
        const float hw = (L.width * 0.5f) / sc / (2.0f * hx) * mm.width;
        const float hh = (L.height * 0.5f) / sc / (2.0f * kDescentHalf) * mm.height;
        const Vector2 vc = to_mm(static_cast<float>(cam.pan.x), static_cast<float>(cam.pan.y));
        DrawRectangleLinesEx({vc.x - hw, vc.y - hh, hw * 2.0f, hh * 2.0f}, 1.0f,
                             with_alpha(WL::XENON_CORE, 170));
        EndScissorMode();
    }

    // Granular simulation time-control HUD (when a community is live).
    const bool sim_live = (d.live_seed != 0) &&
                          (f.kind == NodeKind::Ecosystem || f.kind == NodeKind::Creature);
    if (sim_live) {
        const Rectangle tc = {stage.x + 12.0f * ui, stage.y + 60.0f * ui, 188.0f * ui, 22.0f * ui};
        draw_glass_panel(tc, {8, 18, 30, 215}, with_alpha(WL::CYAN_DIM, 120), 0.35f, 2);
        // Play/pause glyph.
        const Vector2 g = {tc.x + 12.0f * ui, tc.y + tc.height * 0.5f};
        if (d.sim_paused) {
            DrawRectangle(static_cast<int>(g.x - 4.0f * ui), static_cast<int>(g.y - 5.0f * ui), static_cast<int>(3.0f * ui), static_cast<int>(10.0f * ui), WL::XENON_CORE);
            DrawRectangle(static_cast<int>(g.x + 1.0f * ui), static_cast<int>(g.y - 5.0f * ui), static_cast<int>(3.0f * ui), static_cast<int>(10.0f * ui), WL::XENON_CORE);
        } else {
            DrawTriangle({g.x - 4.0f * ui, g.y - 5.0f * ui}, {g.x - 4.0f * ui, g.y + 5.0f * ui},
                         {g.x + 5.0f * ui, g.y}, WL::PLASMA_GREEN);
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s   %.2gx   t=%.0fs", d.sim_paused ? "PAUSED" : "RUN",
                      d.sim_speed, d.live.clock.sim_time);
        draw_text(buf, {tc.x + 26.0f * ui, tc.y + 5.0f * ui}, 10.0f * ui, WL::TEXT_SECONDARY);
        draw_text("space pause | , . speed | o step | x perturb",
                  {tc.x, tc.y + tc.height + 2.0f * ui}, 8.0f * ui, with_alpha(WL::TEXT_TERTIARY, 170));
        if (d.perturb_flash > 0.001) {
            DrawRectangleRoundedLines(stage, 0.02f, 12, 2.0f,
                                      with_alpha(WL::XENON_CORE, static_cast<unsigned char>(120 * d.perturb_flash)));
        }
    }

    // Boundary flash ring.
    if (cam.flash > 0.001f) {
        DrawRectangleRoundedLines(stage, 0.02f, 12, 2.4f,
                                  with_alpha(WL::CYAN_CORE,
                                             static_cast<unsigned char>(170 * cam.flash)));
    }

    // Hover label + reticle.
    if (d.hovered_child >= 0 && d.hovered_child < static_cast<int>(d.child_px.size())) {
        const ChildRef& c = f.children[static_cast<std::size_t>(d.hovered_child)];
        const Vector2 p = child_pos(d, d.hovered_child);
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

    // Tier scientific-instrument deck (real plots of the engine's physics).
    draw_descent_analysis(cosmos, stage, ui);
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

namespace {

// Compact "label .......... value" row for dense fact listings.
float draw_fact_row(Rectangle rect, float y, const std::string& k, const std::string& v, float ui) {
    draw_text(k, {rect.x + 14.0f * ui, y}, 11.5f * ui, WL::TEXT_TERTIARY);
    const Vector2 vs = measure_ui_text(v, 12.0f * ui);
    const float vx = std::max(rect.x + rect.width * 0.45f, rect.x + rect.width - 14.0f * ui - vs.x);
    draw_text(v, {vx, y}, 12.0f * ui, WL::TEXT_SECONDARY);
    return y + 18.5f * ui;
}

// A population-history sparkline (auto-scaled to its own min/max).
void draw_pop_spark(Rectangle r, const ecosim::PopRing& ring, Color c) {
    DrawRectangleRounded(r, 0.25f, 4, Color{5, 12, 22, 170});
    DrawRectangleRoundedLines(r, 0.25f, 4, 1.0f, with_alpha(WL::GLASS_BORDER, 60));
    const int n = ring.filled;
    if (n < 2) return;
    float mn = 1e30f, mx = -1e30f;
    for (int i = 0; i < n; ++i) { const float v = ring.at(i); mn = std::min(mn, v); mx = std::max(mx, v); }
    const float span = std::max(1e-12f, mx - mn);
    Vector2 prev{};
    for (int i = 0; i < n; ++i) {
        const float x = r.x + static_cast<float>(i) / static_cast<float>(n - 1) * r.width;
        const float yy = r.y + r.height - 1.0f - (ring.at(i) - mn) / span * (r.height - 2.0f);
        const Vector2 p{x, yy};
        if (i > 0) DrawLineEx(prev, p, 1.3f, c);
        prev = p;
    }
}

// Horizontal capacity gauge.
void draw_health_gauge(Rectangle r, const std::string& label, float v01, Color fill, float ui) {
    DrawRectangleRounded(r, 0.5f, 6, Color{5, 12, 22, 195});
    const float w = std::clamp(v01, 0.0f, 1.0f) * (r.width - 4.0f);
    DrawRectangleRounded({r.x + 2.0f, r.y + 2.0f, std::max(3.0f, w), r.height - 4.0f}, 0.5f, 6,
                         with_alpha(fill, 205));
    DrawRectangleRoundedLines(r, 0.5f, 6, 1.0f, with_alpha(fill, 120));
    draw_text(label, {r.x + 9.0f * ui, r.y + (r.height - 10.0f * ui) * 0.5f}, 10.0f * ui, WL::TEXT_PRIMARY);
}

} // namespace

void draw_descent_inspector(CosmosState& cosmos, Rectangle rect, float ui) {
    DescentState& d = cosmos.descent;
    const ProcNode& f = d.focus();
    const bool eco_live = (f.kind == NodeKind::Ecosystem && d.live_seed == f.seed);

    draw_card(rect, {6, 13, 24, 224}, with_alpha(WL::GLASS_BORDER, 120));
    draw_text(node_kind_name(f.kind), {rect.x + 14.0f * ui, rect.y + 12.0f * ui}, 12.0f * ui,
              with_alpha(WL::CYAN_CORE, 200));
    draw_tick_strip({rect.x + rect.width - 92.0f * ui, rect.y + 17.0f * ui}, 12, 6.0f * ui,
                    6.0f * ui, WL::CYAN_DIM);
    draw_text(f.name, {rect.x + 14.0f * ui, rect.y + 28.0f * ui}, 20.0f * ui, WL::TEXT_PRIMARY);
    draw_text_block(f.descriptor,
                    {rect.x + 14.0f * ui, rect.y + 54.0f * ui, rect.width - 28.0f * ui, 36.0f * ui},
                    12.5f * ui, WL::TEXT_TERTIARY, 2.0f * ui);

    // Key metric tiles: the four headline facts as a 2x2.
    float y = rect.y + 94.0f * ui;
    const float tile_w = (rect.width - 30.0f * ui) * 0.5f;
    const float tile_h = 40.0f * ui;
    const int n_tiles = std::min<int>(4, static_cast<int>(f.facts.size()));
    for (int i = 0; i < n_tiles; ++i) {
        const Rectangle tile = {rect.x + 12.0f * ui + (i % 2) * (tile_w + 6.0f * ui),
                                y + (i / 2) * (tile_h + 6.0f * ui), tile_w, tile_h};
        draw_metric(tile, f.facts[static_cast<std::size_t>(i)].first.c_str(),
                    f.facts[static_cast<std::size_t>(i)].second, ui);
    }
    y += static_cast<float>((n_tiles + 1) / 2) * (tile_h + 6.0f * ui) + 8.0f * ui;

    // ── Ecosystem observability: health gauge, trophic pyramid, live sparklines ──
    if (eco_live) {
        const eco::Community& C = d.live.community;

        const float health = std::clamp(0.5f + 0.5f * static_cast<float>(C.stats.stability_margin),
                                        0.0f, 1.0f);
        const Color hc = lerp_color(WL::XENON_CORE, WL::PLASMA_GREEN, health);
        draw_text("ECOSYSTEM HEALTH", {rect.x + 14.0f * ui, y}, 10.5f * ui, with_alpha(WL::CYAN_CORE, 180));
        y += 15.0f * ui;
        draw_health_gauge({rect.x + 12.0f * ui, y, rect.width - 24.0f * ui, 16.0f * ui},
                          C.stats.stability_margin > 0.0 ? "stable" : "stressed", health, hc, ui);
        y += 25.0f * ui;

        draw_text("TROPHIC BIOMASS PYRAMID", {rect.x + 14.0f * ui, y}, 10.5f * ui,
                  with_alpha(WL::CYAN_CORE, 180));
        y += 15.0f * ui;
        double band[5] = {0, 0, 0, 0, 0};
        for (const auto& s : C.species) {
            const int b = s.t.tau < 0.5 ? 0 : (s.t.tau < 1.5 ? 1 : (s.t.tau < 2.4 ? 2 : (s.t.tau < 3.5 ? 3 : 4)));
            band[b] += s.biomass;
        }
        double bmx = 1e-12;
        for (double b : band) bmx = std::max(bmx, b);
        const char* blbl[5] = {"Producers", "Herbivores", "Omnivores", "Carnivores", "Apex"};
        const Color bcol[5] = {WL::PLASMA_GREEN, WL::CYAN_CORE, {120, 200, 120, 255},
                               WL::XENON_CORE, {255, 92, 92, 255}};
        const float rowh = 15.0f * ui;
        for (int i = 0; i < 5; ++i) {
            const int bi = 4 - i; // apex on top
            const float frac = static_cast<float>(band[bi] / bmx);
            const float w = std::max(8.0f, frac * (rect.width - 32.0f * ui));
            const Rectangle bar = {rect.x + 16.0f * ui + ((rect.width - 32.0f * ui) - w) * 0.5f,
                                   y + i * rowh, w, rowh - 3.0f * ui};
            DrawRectangleRounded(bar, 0.4f, 4, with_alpha(bcol[bi], 175));
            draw_text(blbl[bi], {rect.x + 18.0f * ui, y + i * rowh - 0.5f * ui}, 9.0f * ui,
                      with_alpha(WL::TEXT_PRIMARY, 205));
        }
        y += 5.0f * rowh + 10.0f * ui;

        draw_text("POPULATION DYNAMICS", {rect.x + 14.0f * ui, y}, 10.5f * ui,
                  with_alpha(WL::CYAN_CORE, 180));
        y += 15.0f * ui;
        std::vector<int> order(C.species.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return C.species[static_cast<std::size_t>(a)].biomass >
                                             C.species[static_cast<std::size_t>(b)].biomass; });
        const float row_dy = 25.0f * ui;
        const int rows = std::min<int>(static_cast<int>(order.size()),
                                       static_cast<int>((rect.y + rect.height - y - 6.0f * ui) / row_dy));
        for (int r = 0; r < rows; ++r) {
            const int si = order[static_cast<std::size_t>(r)];
            const float ry = y + r * row_dy;
            const bool key = (si == C.stats.keystone);
            DrawCircleGradient(static_cast<int>(rect.x + 17.0f * ui), static_cast<int>(ry + 8.0f * ui),
                               5.0f * ui, to_raylib(C.species[static_cast<std::size_t>(si)].color), {0, 0, 0, 0});
            const std::string nm = C.species[static_cast<std::size_t>(si)].name + (key ? "  keystone" : "");
            draw_text(nm, {rect.x + 27.0f * ui, ry + 1.0f * ui}, 10.5f * ui,
                      key ? WL::PLASMA_GREEN : WL::TEXT_SECONDARY);
            if (si < static_cast<int>(d.live.history.size()))
                draw_pop_spark({rect.x + rect.width * 0.52f, ry, rect.width * 0.48f - 14.0f * ui, 16.0f * ui},
                               d.live.history[static_cast<std::size_t>(si)],
                               to_raylib(C.species[static_cast<std::size_t>(si)].color));
        }
        return;
    }

    // ── Non-ecosystem: remaining facts as dense rows (full physiology shows) ─────
    for (std::size_t i = static_cast<std::size_t>(n_tiles); i < f.facts.size(); ++i)
        y = draw_fact_row(rect, y, f.facts[i].first, f.facts[i].second, ui);
    y += 8.0f * ui;

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

    // Two-line control legend: camera on top, the full keyboard shell below —
    // every shortcut the shell honours is discoverable on screen.
    const float hy = stage_content_bottom(d, stage, ui) - 32.0f * ui;
    // Each line shrinks (to a legible floor) rather than overrunning a narrow stage.
    const float avail = stage.width - 24.0f * ui;
    auto fit_line = [&](const char* text, float y, float size, unsigned char alpha) {
        const float w = measure_ui_text(text, size).x;
        const float fitted = w > avail ? std::max(8.0f, size * avail / w) : size;
        draw_text(text, {stage.x + 12.0f * ui, y}, fitted, with_alpha(WL::TEXT_TERTIARY, alpha));
    };
    fit_line(node_is_leaf(f.kind)
                 ? "scroll out: ascend   |   drag / WASD: pan   |   C: recenter"
                 : "scroll in / click / Enter: enter   |   scroll out: ascend   |   drag / WASD: pan   |   C: recenter",
             hy, 10.5f * ui, 200);
    fit_line("arrows: aim   |   Tab: cycle   |   1-9: jump   |   H: notable   |   [ ]: siblings   |   backspace: up   |   Home: root   |   G: deck",
             hy + 14.0f * ui, 9.5f * ui, 150);
}

} // namespace cosmos_ui
