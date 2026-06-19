#include "ui/CosmosDescent.hpp"

#include "ui/CosmosExplorerInternal.hpp" // helpers + draw_universe_backdrop decl
#include "ui/CosmosNavigator.hpp"        // kZoomMin / kZoomMax
#include "renderer/Renderer.hpp"

#include "ui/UiCharts.hpp"

#include "cosmos/Astrobio.hpp"
#include "cosmos/Cosmography.hpp"
#include "cosmos/CosmoStats.hpp"
#include "cosmos/Demography.hpp"
#include "cosmos/EcoSim.hpp"
#include "cosmos/Organism.hpp"
#include "cosmos/Phenology.hpp"
#include "cosmos/SpatialHash.hpp"

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

// Single per-frame layout pass: compute every child's screen position once into
// the reusable buffers (read by hover/pick in update, and links/sprites in draw).
void fill_child_positions(CosmosState& cosmos, Rectangle stage, float t) {
    DescentState& d = cosmos.descent;
    const auto& kids = d.focus().children;
    const std::size_t n = kids.size();
    d.child_px.resize(n);
    d.child_py.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        float nx, ny;
        child_layout(kids[i], d.focus_kind(), t, nx, ny);
        const Vector2 p = layout_to_screen(nx, ny, stage, d.camera);
        d.child_px[i] = p.x;
        d.child_py[i] = p.y;
    }
}

inline Vector2 child_pos(const DescentState& d, int i) {
    return {d.child_px[static_cast<std::size_t>(i)], d.child_py[static_cast<std::size_t>(i)]};
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
    // Pass the current focus as parent (stable copy in the path) so context-aware
    // generation (planet habitability, ecosystem climate) sees its star/planet.
    const ProcNode* parent = &d.path.back();
    ProcNode cn = d.universe->node(child.seed, child.kind, parent); // copy (engine ref may move)
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
        if (IsKeyPressed(KEY_G)) d.analysis_open = !d.analysis_open;

        // Single layout pass + O(1) spatial-hash hover with hysteresis (no flicker
        // when two children are near-equidistant; scales to thousands of children).
        fill_child_positions(cosmos, stage, t);
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
            // Click a hovered non-leaf child to enter it.
            if (d.hovered_child >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                !node_is_leaf(d.focus_kind())) {
                descent_push(cosmos, d.hovered_child);
            }
        } else {
            d.hover_locked = -1;
        }
    } else {
        fill_child_positions(cosmos, stage, t);
    }

    // Descend / ascend on zoom bounds.
    if (cam.target_zoom > kZoomMax) {
        const ProcNode& f = d.focus();
        if (!node_is_leaf(f.kind) && !f.children.empty()) {
            // Pick the child nearest the stage center (cursor-anchored zoom pulls
            // the aimed child there). Require it to be reasonably central.
            int best = -1;
            float best_d = 1.0e30f;
            for (int i = 0; i < static_cast<int>(d.child_px.size()); ++i) {
                const Vector2 p = child_pos(d, i);
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

    // Live ecosystem: build on entry, then advance with a fixed timestep so the
    // food web breathes (predators lag prey) smoothly and FPS-independently, while
    // recording population history for the observability sparklines.
    if (d.focus_kind() == NodeKind::Ecosystem) {
        if (d.live_seed != d.focus().seed) {
            ecosim::init_live(d.live, community_for_ecosystem(d.focus()));
            d.live_seed = d.focus().seed;
        }
        ecosim::advance(d.live, dt);
    } else if (d.focus_kind() == NodeKind::Creature && d.depth() > 0) {
        // Keep the parent ecosystem's community live so a creature is always shown
        // in its ecological context (and the analysis deck has its data), no matter
        // how the node was reached.
        const ProcNode& ecop = d.path[static_cast<std::size_t>(d.depth() - 1)];
        if (ecop.kind == NodeKind::Ecosystem) {
            if (d.live_seed != ecop.seed) {
                ecosim::init_live(d.live, community_for_ecosystem(ecop));
                d.live_seed = ecop.seed;
            }
            ecosim::advance(d.live, dt);
        }
    }
}

// ── Tier analysis deck: a strip of real scientific instruments per level ──────
// Every plot is driven by the engine's own physics (CosmoStats / Cosmography /
// Astrobio / Phenology / Organism / Demography / the live ecosystem), so the
// console reads the actual model, not decoration.
void draw_descent_analysis(CosmosState& cosmos, Rectangle stage, float ui) {
    using namespace charts;
    DescentState& d = cosmos.descent;
    if (!d.analysis_open) return;
    const ProcNode& f = d.focus();
    const float t = static_cast<float>(GetTime());

    const float gap = 8.0f * ui;
    const float ch = std::min(176.0f * ui, stage.height * 0.34f);
    const float cw = (stage.width - 4.0f * gap) / 3.0f;
    const float cy = stage.y + stage.height - ch - gap;
    const Rectangle C0 = {stage.x + gap, cy, cw, ch};
    const Rectangle C1 = {C0.x + cw + gap, cy, cw, ch};
    const Rectangle C2 = {C1.x + cw + gap, cy, cw, ch};

    if (f.kind == NodeKind::Universe) {
        // (1) LambdaCDM energy budget pie.
        Rectangle p = plot_card(C0, "ENERGY BUDGET", ui, WL::VIOLET_CORE);
        draw_pie(p, {static_cast<float>(cstat::kOmegaB), static_cast<float>(cstat::kOmegaC),
                     static_cast<float>(cstat::kOmegaLambda)},
                 {WL::XENON_CORE, WL::CYAN_CORE, WL::VIOLET_CORE});
        draw_text("baryon 5%", {C0.x + 9.0f * ui, C0.y + C0.height - 40.0f * ui}, 8.0f * ui, WL::XENON_CORE);
        draw_text("dark matter 27%", {C0.x + 9.0f * ui, C0.y + C0.height - 28.0f * ui}, 8.0f * ui, WL::CYAN_CORE);
        draw_text("dark energy 68%", {C0.x + 9.0f * ui, C0.y + C0.height - 16.0f * ui}, 8.0f * ui, WL::VIOLET_CORE);

        // (2) Matter power spectrum P(k) (log-log) with turnover + BAO scale.
        Axes a;
        a.plot = plot_card(C1, "MATTER POWER SPECTRUM P(k)", ui);
        a.logx = a.logy = true;
        a.x0 = std::log10(1e-3); a.x1 = std::log10(1.0);
        a.y0 = std::log10(1e2);  a.y1 = std::log10(1e6);
        draw_grid(a, ui, "k [h/Mpc]", "P(k)");
        draw_function(a, [](double k) { return cstat::power_spectrum_pk(k) * 1e4; }, WL::CYAN_CORE);
        const float kt = map_x(a, 0.018);
        DrawLineEx({kt, a.plot.y}, {kt, a.plot.y + a.plot.height}, 1.0f, with_alpha(WL::XENON_CORE, 120));
        draw_text("turnover", {kt + 2.0f * ui, a.plot.y + 2.0f * ui}, 8.0f * ui, with_alpha(WL::XENON_CORE, 200));

        // (3) Cosmic-web census (void/wall/filament/node volume share).
        Rectangle q = plot_card(C2, "COSMIC WEB CENSUS", ui, WL::PLASMA_GREEN);
        std::vector<cosmoweb::WebPoint> web = cosmoweb::sample_cosmic_web(f.seed, 3000, 200.0);
        float cnt[4] = {0, 0, 0, 0};
        for (const auto& w : web) cnt[static_cast<int>(w.cls)] += 1.0f;
        const float tot = std::max(1.0f, cnt[0] + cnt[1] + cnt[2] + cnt[3]);
        draw_hbars(q, {cnt[0] / tot, cnt[1] / tot, cnt[2] / tot, cnt[3] / tot},
                   {"void", "wall", "filament", "node"},
                   {WL::VOID_SURFACE, WL::CYAN_DIM, WL::CYAN_CORE, WL::XENON_CORE}, ui);
        return;
    }

    if (f.kind == NodeKind::Galaxy) {
        const double mstar = std::max(1e7, f.phys_mass);
        // (1) Rotation curve: flat (with dark-matter halo) vs declining (baryons only).
        Axes a;
        a.plot = plot_card(C0, "ROTATION CURVE", ui);
        a.x0 = 0; a.x1 = 30; a.y0 = 0; a.y1 = 260;
        draw_grid(a, ui, "r [kpc]", "v [km/s]");
        const double vflat = 180.0 * std::pow(mstar / 5e10, 0.25);
        draw_function(a, [&](double r) { return vflat * std::sqrt(1.0 - std::exp(-r / 4.0)); },
                      WL::CYAN_CORE);                                   // with dark matter -> flat
        draw_function(a, [&](double r) { return vflat * std::sqrt(3.0 / (r + 3.0)); },
                      with_alpha(WL::XENON_CORE, 200));                  // baryons only -> Keplerian fall
        draw_text("halo (DM)", {a.plot.x + 4.0f * ui, a.plot.y + 2.0f * ui}, 8.0f * ui, WL::CYAN_CORE);
        draw_text("baryons", {a.plot.x + 4.0f * ui, a.plot.y + 12.0f * ui}, 8.0f * ui, WL::XENON_CORE);

        // (2) NFW dark-matter density profile (log-log; r^-1 inner, r^-3 outer).
        Axes b;
        b.plot = plot_card(C1, "NFW HALO PROFILE", ui);
        b.logx = b.logy = true;
        b.x0 = std::log10(0.05); b.x1 = std::log10(50.0);
        b.y0 = std::log10(1e-4); b.y1 = std::log10(1e2);
        draw_grid(b, ui, "r / r_s", "rho");
        draw_function(b, [](double r) { return cstat::nfw_density(r, 1.0, 1.0); }, WL::VIOLET_CORE);

        // (3) Galaxy stellar mass function (Schechter) with this galaxy marked.
        Axes c;
        c.plot = plot_card(C2, "STELLAR MASS FUNCTION", ui, WL::PLASMA_GREEN);
        c.logx = c.logy = true;
        c.x0 = std::log10(1e8); c.x1 = std::log10(3e11);
        c.y0 = std::log10(1e-5); c.y1 = std::log10(1e-1);
        draw_grid(c, ui, "M* [Msun]", "phi");
        const double Mstar = std::pow(10.0, 10.66);
        draw_function(c, [&](double M) { return cstat::schechter_phi(M, Mstar, -1.2, 3.96e-3); },
                      WL::PLASMA_GREEN);
        draw_marker(c, mstar, cstat::schechter_phi(mstar, Mstar, -1.2, 3.96e-3), WL::CYAN_CORE, ui, "this");
        return;
    }

    if (f.kind == NodeKind::StarSystem) {
        const double L = std::max(1e-4, f.luminosity);
        const double Teff = std::max(2200.0, f.phys_radius);
        // (1) HR diagram (Teff reversed) with the main sequence + this star.
        Axes a;
        a.plot = plot_card(C0, "HR DIAGRAM", ui, WL::XENON_CORE);
        a.logx = a.logy = true;
        a.x0 = std::log10(45000.0); a.x1 = std::log10(2200.0); // hot left -> cool right
        a.y0 = std::log10(1e-4); a.y1 = std::log10(1e6);
        draw_grid(a, ui, "Teff [K]", "L/Lsun");
        const auto& tbl = astro::star_table();
        std::vector<double> ms_t, ms_l;
        for (const auto& s : tbl) { ms_t.push_back(s.temp_k); ms_l.push_back(astro::star_luminosity(s.mass_sun)); }
        draw_series(a, ms_t, ms_l, with_alpha(WL::CYAN_DIM, 220), 1.6f);
        draw_marker(a, Teff, L, WL::XENON_CORE, ui, "star");

        // (2) Blackbody spectral energy distribution.
        Axes b;
        b.plot = plot_card(C1, "BLACKBODY SED", ui);
        b.logx = true; b.x0 = std::log10(1e-7); b.x1 = std::log10(3e-6);
        b.y0 = 0; b.y1 = 1.05;
        draw_grid(b, ui, "lambda [m]", "B");
        const double lam_pk = 2.898e-3 / Teff;
        auto planck = [&](double lam) {
            const double c2 = 1.4388e-2; // m K
            const double v = (1.0 / std::pow(lam, 5)) / (std::exp(c2 / (lam * Teff)) - 1.0);
            const double vp = (1.0 / std::pow(lam_pk, 5)) / (std::exp(c2 / (lam_pk * Teff)) - 1.0);
            return v / std::max(1e-300, vp);
        };
        draw_area(b, planck, palette_color(f.color, 255));
        draw_function(b, planck, palette_color(f.color, 255));

        // (3) Mass-luminosity relation with this star marked.
        Axes c;
        c.plot = plot_card(C2, "MASS-LUMINOSITY", ui, WL::PLASMA_GREEN);
        c.logx = c.logy = true;
        c.x0 = std::log10(0.08); c.x1 = std::log10(40.0);
        c.y0 = std::log10(1e-4); c.y1 = std::log10(1e6);
        draw_grid(c, ui, "M [Msun]", "L/Lsun");
        draw_function(c, [](double m) { return cstat::stellar_luminosity(m); }, WL::PLASMA_GREEN);
        draw_marker(c, std::max(0.08, f.phys_mass), L, WL::CYAN_CORE, ui, "");
        return;
    }

    if (f.kind == NodeKind::Planet) {
        // (1) Mass-radius diagram (Chen-Kipping) with this planet + regime lines.
        Axes a;
        a.plot = plot_card(C0, "MASS-RADIUS", ui);
        a.logx = a.logy = true;
        a.x0 = std::log10(0.1); a.x1 = std::log10(1e4);
        a.y0 = std::log10(0.3); a.y1 = std::log10(20.0);
        draw_grid(a, ui, "M [Mearth]", "R [Rearth]");
        draw_function(a, [](double m) { return cstat::planet_radius_earth(m); }, WL::CYAN_CORE);
        for (double bx : {2.04, 132.0}) {
            const float xx = map_x(a, bx);
            DrawLineEx({xx, a.plot.y}, {xx, a.plot.y + a.plot.height}, 1.0f, with_alpha(WL::GLASS_BORDER, 70));
        }
        if (f.phys_mass > 0.0)
            draw_marker(a, f.phys_mass, f.phys_radius, WL::XENON_CORE, ui, "world");

        // (2) Interior structure: differentiated core / mantle / crust / atmosphere.
        Rectangle q = plot_card(C1, "INTERIOR STRUCTURE", ui, WL::XENON_CORE);
        const Vector2 ic = {q.x + q.width * 0.5f, q.y + q.height * 0.5f};
        const float R = std::min(q.width, q.height) * 0.46f;
        const bool rocky = (f.phys_radius > 0.0 && f.phys_radius < 2.2);
        const Color shell[4] = {WL::XENON_CORE, {180, 90, 40, 255}, {110, 92, 70, 255}, WL::CYAN_DIM};
        float frac[4];
        if (rocky) { frac[0] = 0.50f; frac[1] = 0.85f; frac[2] = 0.97f; frac[3] = 1.0f; }
        else       { frac[0] = 0.25f; frac[1] = 0.55f; frac[2] = 0.80f; frac[3] = 1.0f; }
        for (int i = 3; i >= 0; --i)
            DrawCircleV(ic, R * frac[i], with_alpha(shell[i], 200));
        DrawCircleLines(static_cast<int>(ic.x), static_cast<int>(ic.y), R, with_alpha(WL::CYAN_DIM, 120));
        draw_text(rocky ? "rocky: Fe core" : "volatile envelope",
                  {q.x + 8.0f * ui, q.y + q.height - 16.0f * ui}, 8.0f * ui, WL::TEXT_TERTIARY);

        // (3) Insolation vs latitude with this world's regions plotted.
        Axes c;
        c.plot = plot_card(C2, "INSOLATION x LATITUDE", ui, WL::PLASMA_GREEN);
        c.x0 = -90; c.x1 = 90; c.y0 = 0; c.y1 = 1.05;
        draw_grid(c, ui, "latitude", "rel. insol.");
        draw_function(c, [](double lat) { return pheno::annual_mean_insolation(lat); }, WL::PLASMA_GREEN);
        for (const ChildRef& rg : f.children) {
            const double lat = -static_cast<double>(rg.y) * 110.0;
            draw_marker(c, lat, pheno::annual_mean_insolation(lat), to_raylib(rg.color), ui);
        }
        return;
    }

    if (f.kind == NodeKind::Ecosystem && d.live_seed == f.seed) {
        const eco::Community& Cm = d.live.community;
        const int S = static_cast<int>(Cm.species.size());
        // (1) Population time-series (all species, from the live history rings).
        Axes a;
        a.plot = plot_card(C0, "POPULATION TIME SERIES", ui, WL::PLASMA_GREEN);
        a.x0 = 0; a.x1 = static_cast<double>(ecosim::PopRing::N); a.y0 = 0; a.y1 = 1;
        // y auto-scaled per draw by normalizing each ring to its own max.
        draw_grid(a, ui, "time", "N/Nmax");
        for (int i = 0; i < S && i < static_cast<int>(d.live.history.size()); ++i) {
            const ecosim::PopRing& rg = d.live.history[static_cast<std::size_t>(i)];
            if (rg.filled < 2) continue;
            float mx = 1e-12f;
            for (int k = 0; k < rg.filled; ++k) mx = std::max(mx, rg.at(k));
            std::vector<double> xs, ys;
            for (int k = 0; k < rg.filled; ++k) { xs.push_back(k); ys.push_back(rg.at(k) / mx); }
            draw_series(a, xs, ys, with_alpha(to_raylib(Cm.species[static_cast<std::size_t>(i)].color), 200), 1.1f);
        }

        // (2) Predator-prey phase portrait (the keystone predator vs its main prey).
        Axes b;
        b.plot = plot_card(C1, "PHASE PORTRAIT", ui, WL::CYAN_CORE);
        int pred = -1, prey = -1;
        for (const eco::Link& lk : Cm.links)
            if (lk.pred < S && lk.prey < S && Cm.species[static_cast<std::size_t>(lk.pred)].t.tau > 1.5) {
                pred = lk.pred; prey = lk.prey; break;
            }
        b.x0 = 0; b.x1 = 1.6; b.y0 = 0; b.y1 = 1.6;
        draw_grid(b, ui, "prey N/Neq", "pred N/Neq");
        if (pred >= 0 && prey >= 0 && pred < static_cast<int>(d.live.history.size()) &&
            prey < static_cast<int>(d.live.history.size())) {
            const ecosim::PopRing& rpr = d.live.history[static_cast<std::size_t>(prey)];
            const ecosim::PopRing& rpd = d.live.history[static_cast<std::size_t>(pred)];
            const double xeq_pr = std::max(1e-9, Cm.species[static_cast<std::size_t>(prey)].xeq);
            const double xeq_pd = std::max(1e-9, Cm.species[static_cast<std::size_t>(pred)].xeq);
            const int nn = std::min(rpr.filled, rpd.filled);
            std::vector<double> xs, ys;
            for (int k = 0; k < nn; ++k) { xs.push_back(rpr.at(k) / xeq_pr); ys.push_back(rpd.at(k) / xeq_pd); }
            draw_series(b, xs, ys, WL::CYAN_CORE, 1.2f);
            if (nn > 0) draw_marker(b, xs.back(), ys.back(), WL::XENON_CORE, ui);
        }

        // (3) Community interaction matrix (predation strengths, heatmap).
        Rectangle q = plot_card(C2, "INTERACTION MATRIX", ui, WL::VIOLET_CORE);
        const int n = std::min(S, 16);
        std::vector<float> mat(static_cast<std::size_t>(n * n), 0.0f);
        for (const eco::Link& lk : Cm.links)
            if (lk.pred < n && lk.prey < n)
                mat[static_cast<std::size_t>(lk.pred * n + lk.prey)] = static_cast<float>(lk.pref);
        draw_heatmap(q, mat, n, n, 0.0f, 1.0f, Color{6, 14, 24, 255}, WL::CYAN_CORE);
        return;
    }

    if (f.kind == NodeKind::Creature) {
        // Recover this creature's species traits from the (frozen) parent ecosystem.
        const eco::SpeciesTraits* tr = nullptr;
        if (d.depth() > 0) {
            const ProcNode& ecop = d.path[static_cast<std::size_t>(d.depth() - 1)];
            if (ecop.kind == NodeKind::Ecosystem && d.live_seed == ecop.seed) {
                for (std::size_t k = 0; k < ecop.children.size() &&
                     k < d.live.community.species.size(); ++k)
                    if (ecop.children[k].seed == f.seed) { tr = &d.live.community.species[k].t; break; }
            }
        }
        if (!tr) return;
        const bio::Organism org = bio::derive_organism(*tr, tr->T_opt);
        const double M = std::pow(10.0, tr->m);

        // (1) Allometric scaling: metabolic rate vs body mass (Kleiber) with marker.
        Axes a;
        a.plot = plot_card(C0, "ALLOMETRY: BMR vs MASS", ui, WL::XENON_CORE);
        a.logx = a.logy = true;
        a.x0 = std::log10(1e-4); a.x1 = std::log10(1e5);
        a.y0 = std::log10(1e-3); a.y1 = std::log10(1e6);
        draw_grid(a, ui, "M [kg]", "BMR [W]");
        draw_function(a, [](double m) { return 3.4 * std::pow(m, 0.75); }, WL::CYAN_DIM); // Kleiber line
        draw_marker(a, M, org.bmr_w, WL::XENON_CORE, ui, "");

        // (2) von Bertalanffy growth curve L(age).
        Axes b;
        b.plot = plot_card(C1, "GROWTH (von BERTALANFFY)", ui, WL::PLASMA_GREEN);
        b.x0 = 0; b.x1 = std::max(1.0, org.lifespan_yr); b.y0 = 0; b.y1 = 1.05;
        draw_grid(b, ui, "age [yr]", "L/Linf");
        draw_function(b, [&](double age) { return 1.0 - std::exp(-org.growth_k * age); }, WL::PLASMA_GREEN);
        const float am = map_x(b, org.age_maturity_yr);
        DrawLineEx({am, b.plot.y}, {am, b.plot.y + b.plot.height}, 1.0f, with_alpha(WL::XENON_CORE, 130));
        draw_text("maturity", {am + 2.0f * ui, b.plot.y + 2.0f * ui}, 8.0f * ui, with_alpha(WL::XENON_CORE, 200));

        // (3) Demography: stable age structure (Lefkovitch) as a pyramid.
        Rectangle q = plot_card(C2, "DEMOGRAPHY (LEFKOVITCH)", ui, WL::CYAN_CORE);
        const demo::StageModel sm = demo::stage_model(*tr, org);
        draw_hbars(q, {static_cast<float>(sm.stable[2]), static_cast<float>(sm.stable[1]),
                       static_cast<float>(sm.stable[0])},
                   {"adult", "subadult", "juvenile"},
                   {WL::CYAN_CORE, {120, 200, 120, 255}, WL::PLASMA_GREEN}, ui, true);
        const std::string st = sm.survivorship > 2.3 ? "Type III" : (sm.survivorship < 1.5 ? "Type I" : "Type II");
        draw_text(st + " survivorship", {q.x + 8.0f * ui, q.y + q.height - 15.0f * ui}, 8.0f * ui,
                  with_alpha(WL::TEXT_TERTIARY, 220));
        return;
    }
    (void)t;
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

    const float zoomf = static_cast<float>(cam.zoom);
    const float sc = base_scale(stage) * zoomf;
    const bool eco_live = (f.kind == NodeKind::Ecosystem && d.live_seed == f.seed);

    // Orbit rings + central body (drawn under the additive field pass).
    BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                     static_cast<int>(stage.width), static_cast<int>(stage.height));
    const Vector2 center = layout_to_screen(0.0f, 0.0f, stage, cam);
    if (f.kind == NodeKind::Universe) {
        // Cosmic web: link each galaxy to its two nearest neighbours, tracing the
        // filaments that thread the clusters (galaxies were placed on the density
        // field's peaks, so this proximity graph reads as the large-scale web).
        const int gn = static_cast<int>(d.child_px.size());
        for (int i = 0; i < gn; ++i) {
            int b1 = -1, b2 = -1;
            float d1 = 1e30f, d2 = 1e30f;
            for (int j = 0; j < gn; ++j) {
                if (j == i) continue;
                const float dx = d.child_px[static_cast<std::size_t>(i)] - d.child_px[static_cast<std::size_t>(j)];
                const float dy = d.child_py[static_cast<std::size_t>(i)] - d.child_py[static_cast<std::size_t>(j)];
                const float dd = dx * dx + dy * dy;
                if (dd < d1) { d2 = d1; b2 = b1; d1 = dd; b1 = j; }
                else if (dd < d2) { d2 = dd; b2 = j; }
            }
            const int nb[2] = {b1, b2};
            for (int b : nb) {
                if (b <= i) continue; // draw each strand once
                const float len = std::hypot(d.child_px[static_cast<std::size_t>(i)] - d.child_px[static_cast<std::size_t>(b)],
                                             d.child_py[static_cast<std::size_t>(i)] - d.child_py[static_cast<std::size_t>(b)]);
                // Nearer galaxies sit on a denser filament -> brighter strand.
                const unsigned char a = static_cast<unsigned char>(std::clamp(46.0f - len * 0.06f, 8.0f, 46.0f));
                DrawLineEx(child_pos(d, i), child_pos(d, b), 1.1f, with_alpha(WL::CYAN_DIM, a));
            }
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
                    const Vector2 p = layout_to_screen(std::cos(ang) * rr, std::sin(ang) * rr, stage, cam);
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
            const Vector2 p = layout_to_screen(0.0f, ny, stage, cam);
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
            const Vector2 p = layout_to_screen(0.0f, ny5[i], stage, cam);
            if (p.y < stage.y + 4.0f || p.y > stage.y + stage.height - 4.0f) continue;
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

    const int n = std::min(static_cast<int>(f.children.size()), 60);
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
        const float ly = stage.y + stage.height - 22.0f * ui;
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

    draw_text(node_is_leaf(f.kind)
                  ? "scroll out to ascend   |   drag / WASD: pan   |   backspace: up"
                  : "scroll in: enter   |   click an object to enter   |   scroll out: ascend   |   drag: pan",
              {stage.x + 12.0f * ui, stage.y + stage.height - 18.0f * ui}, 10.5f * ui,
              with_alpha(WL::TEXT_TERTIARY, 200));
}

} // namespace cosmos_ui
