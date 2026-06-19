#pragma once
//  ─────────────────────────────────────────────────────────────────────────
//  FieldRenderer — the live "exotic universe" visual backend.
//
//  Instead of squashing the rich generated phase-flow onto a double pendulum,
//  this renderer integrates the *actual* generated law across a swarm of tens
//  of thousands of test masses and paints their world-lines as a luminous,
//  self-organising field — vortices, spiral arms, shear sheets, saddles and
//  attractors emerge directly from the seed's tensors.  Every universe gets a
//  signature colour palette derived from its own invariants, so no two seeds
//  look alike.
//
//  Design notes:
//    • Structure-of-arrays float state + a precomputed, branch-light force
//      operator (FlowOperator) → trivially auto-vectorised and cache friendly.
//    • Optional multithreaded advection across hardware threads.
//    • Fast transcendental approximations for the nonlinear potential weights.
//    • Feedback-fade accumulation texture + cheap multi-tap bloom for glow.
//    • Static streamline atlas + starfield computed once at configure time.
//
//  Header-only to match the rest of src/renderer and avoid build wiring.
//  ─────────────────────────────────────────────────────────────────────────

#include "raylib.h"

#include "math/Vec2.hpp"
#include "physics/LawSpec.hpp"
#include "renderer/FlowOperator.hpp"   // wlfield::{FlowOperator, Rng, Vec2f, fast_*}
#include "seed/MetaSpec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
class FieldRenderer {
public:
    struct Metrics {
        float exotic_index = 0.0f;  // 0..100 "exotic-ness" score
        float vorticity = 0.0f;     // mean |curl| of the live swarm
        float flux = 0.0f;          // mean speed
        float coherence = 0.0f;     // 0..1 alignment of neighbouring motion
        float handedness = 0.0f;    // signed swirl bias
        int alive = 0;
    };

    FieldRenderer(int w, int h) { resize(w, h); build_glow_sprite(); }

    ~FieldRenderer() {
        if (has_tex_) { UnloadRenderTexture(accum_); UnloadRenderTexture(bloom_); }
        if (glow_.id != 0) UnloadTexture(glow_);
    }

    FieldRenderer(const FieldRenderer&) = delete;
    FieldRenderer& operator=(const FieldRenderer&) = delete;

    void ensure_size(int w, int h) {
        if (w != width_ || h != height_) resize(w, h);
    }

    bool configured_for(std::uint64_t token) const { return has_config_ && token_ == token; }
    std::uint64_t token() const { return token_; }

    Color palette_cool() const { return cool_; }
    Color palette_mid() const { return mid_; }
    Color palette_hot() const { return hot_; }
    Color palette_accent() const { return accent_; }
    const Metrics& metrics() const { return metrics_; }

    // ── (re)build the field for a generated law ──────────────────────────────
    void configure(const LawSpec& law, std::uint64_t token, const std::string& seed) {
        token_ = token;
        has_config_ = true;
        const MetaSpec& m = law.meta_spec();

        op_.configure(law);
        build_palette(m, seed);
        choose_view_radius();
        spawn_all();
        build_streamlines();
        build_stars(seed);
        clear_accum_ = true;
        metrics_ = {};
        hero_trail_.clear();
        hero_head_ = 0;
        time_ = 0.0;
    }

    void reset() {
        if (!has_config_) return;
        spawn_all();
        clear_accum_ = true;
        hero_trail_.clear();
        hero_head_ = 0;
    }

    // advance the swarm
    void update(double dt, bool running, Vec2 hero_q, Vec2 hero_v) {
        if (!has_config_) return;
        time_ += dt;
        (void)hero_v;

        // record hero worldline
        if (running) {
            push_hero(static_cast<float>(hero_q.x), static_cast<float>(hero_q.y));
        }

        if (!running) return;

        // fixed-budget substeps for frame-rate independence
        const float frame = std::min(static_cast<float>(dt), 0.05f);
        const float speed = 1.35f; // simulation tempo
        float remaining = frame * speed;
        const float h = 1.0f / 90.0f;
        int guard = 0;
        while (remaining > 1.0e-4f && guard < 8) {
            const float step = std::min(h, remaining);
            integrate(step);
            remaining -= step;
            ++guard;
        }
        compute_metrics();
    }

    // ── render everything into the stage rectangle ───────────────────────────
    void draw(Rectangle stage) {
        if (!has_config_ || !has_tex_) return;

        const Vector2 center = {stage.x + stage.width * 0.5f, stage.y + stage.height * 0.5f};
        const float view_px = 0.5f * std::min(stage.width, stage.height);
        const float scale = view_px / view_radius_;

        // 1) paint particles into the feedback-fade accumulation buffer
        BeginTextureMode(accum_);
        if (clear_accum_) {
            ClearBackground({0, 0, 0, 0});
            clear_accum_ = false;
        }
        // fade previous frame toward black (trail persistence)
        DrawRectangle(static_cast<int>(stage.x) - 2, static_cast<int>(stage.y) - 2,
                      static_cast<int>(stage.width) + 4, static_cast<int>(stage.height) + 4,
                      {0, 0, 0, fade_alpha_});
        BeginBlendMode(BLEND_ADDITIVE);
        paint_swarm(center, scale, stage);
        EndBlendMode();
        EndTextureMode();

        // 2) compose onto the screen target
        BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                         static_cast<int>(stage.width), static_cast<int>(stage.height));
        draw_backdrop(stage, center, scale);
        draw_streamlines(center, scale);

        const Rectangle src = {0.0f, 0.0f, static_cast<float>(width_), -static_cast<float>(height_)};
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTextureRec(accum_.texture, src, {0.0f, 0.0f}, WHITE);
        // cheap bloom: rescaled additive taps of the same buffer
        draw_bloom_tap(stage, center, 1.018f, 82);
        draw_bloom_tap(stage, center, 1.055f, 44);
        draw_bloom_tap(stage, center, 1.12f, 22);
        EndBlendMode();

        draw_hero(center, scale);
        EndScissorMode();

        DrawRectangleRoundedLines(stage, 0.02f, 16, 1.4f, {70, 120, 150, 150});
    }

    // expose particle count for the HUD
    int particle_count() const { return active_; }

private:
    // ── geometry / textures ──────────────────────────────────────────────────
    int width_ = 0, height_ = 0;
    bool has_tex_ = false;
    RenderTexture2D accum_{};
    RenderTexture2D bloom_{};
    Texture2D glow_{};
    bool clear_accum_ = true;
    unsigned char fade_alpha_ = 21;

    void resize(int w, int h) {
        if (has_tex_) { UnloadRenderTexture(accum_); UnloadRenderTexture(bloom_); }
        width_ = std::max(2, w);
        height_ = std::max(2, h);
        accum_ = LoadRenderTexture(width_, height_);
        bloom_ = LoadRenderTexture(std::max(2, width_ / 2), std::max(2, height_ / 2));
        SetTextureFilter(accum_.texture, TEXTURE_FILTER_BILINEAR);
        has_tex_ = true;
        clear_accum_ = true;
    }

    void build_glow_sprite() {
        const int n = 32;
        Image img = GenImageColor(n, n, {0, 0, 0, 0});
        const float c = (n - 1) * 0.5f;
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const float dx = (x - c) / c, dy = (y - c) / c;
                float r = std::sqrt(dx * dx + dy * dy);
                float a = std::max(0.0f, 1.0f - r);
                a = a * a * a; // tight core, soft halo
                ImageDrawPixel(&img, x, y, {255, 255, 255,
                              static_cast<unsigned char>(std::min(255.0f, a * 255.0f))});
            }
        }
        glow_ = LoadTextureFromImage(img);
        SetTextureFilter(glow_, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }

    // ── configuration ────────────────────────────────────────────────────────
    std::uint64_t token_ = 0;
    bool has_config_ = false;
    wlfield::FlowOperator op_{};
    float view_radius_ = 2.6f;
    double time_ = 0.0;

    Color cool_{40, 120, 220, 255};
    Color mid_{120, 90, 240, 255};
    Color hot_{255, 150, 80, 255};
    Color accent_{120, 255, 220, 255};
    float speed_norm_ = 1.0f; // maps speed→colour ramp

    static Color hsv(float h, float s, float v, unsigned char a = 255) {
        Color c = ColorFromHSV(h - 360.0f * std::floor(h / 360.0f), s, v);
        c.a = a;
        return c;
    }

    void build_palette(const MetaSpec& m, const std::string& seed) {
        // signature hue from metric orientation + a seed hash
        const double a = m.g[0][0], b = 0.5 * (m.g[0][1] + m.g[1][0]), d = m.g[1][1];
        const double theta = 0.5 * std::atan2(2.0 * b, a - d);
        std::uint64_t hh = 1469598103934665603ull;
        for (char ch : seed) { hh ^= static_cast<unsigned char>(ch); hh *= 1099511628257ull; }
        const float hash_hue = static_cast<float>(hh % 360u);

        const double aniso_g = std::abs(a - d) / (std::abs(a) + std::abs(d) + 1.0e-9);
        const double pmag = std::min(1.0, std::abs(m.p) / 4.5);
        const double warp = std::sqrt(m.W[0][0] * m.W[0][0] + m.W[0][1] * m.W[0][1]
                                    + m.W[1][0] * m.W[1][0] + m.W[1][1] * m.W[1][1]);

        const float base_hue = static_cast<float>(theta * (180.0 / 3.14159265) * 2.0) + hash_hue;
        const float spread = static_cast<float>(40.0 + 120.0 * aniso_g + 90.0 * pmag);

        cool_   = hsv(base_hue - spread * 0.5f, 0.85f, 0.95f);
        mid_    = hsv(base_hue + spread * 0.20f, 0.80f, 1.0f);
        hot_    = hsv(base_hue + spread, 0.78f + 0.20f * static_cast<float>(pmag), 1.0f);
        accent_ = hsv(base_hue + 150.0f + 60.0f * static_cast<float>(warp), 0.55f, 1.0f);
        speed_norm_ = std::max(0.4f, op_.ceiling * 0.10f);
    }

    void choose_view_radius() {
        // sample peak excursion of a probe orbit to frame the field nicely
        float qx = 0.4f, qy = 0.05f, vx = 0.0f, vy = 0.6f;
        float peak = 0.5f;
        for (int i = 0; i < 700; ++i) {
            float ax, ay; op_.accel(qx, qy, vx, vy, ax, ay);
            vx += ax * (1.0f / 90.0f); vy += ay * (1.0f / 90.0f);
            qx += vx * (1.0f / 90.0f); qy += vy * (1.0f / 90.0f);
            peak = std::max(peak, std::sqrt(qx * qx + qy * qy));
            if (!std::isfinite(peak)) { peak = 2.2f; break; }
        }
        view_radius_ = std::isfinite(peak) ? std::clamp(peak * 1.25f, 1.4f, 3.0f) : 2.2f;
    }

    // ── particle state (SoA) ──────────────────────────────────────────────────
    static constexpr int kMaxParticles = 16384;
    int active_ = 13000;
    std::vector<float> qx_, qy_, vx_, vy_, qpx_, qpy_;
    std::vector<float> age_, life_;

    void spawn_one(int i, wlfield::Rng& rng) {
        // Fill the whole field; a mild tangential bias lets swirl read as spirals
        // while the law's flow does the rest.  (Tuned in the offline preview.)
        const float r = view_radius_ * (0.04f + 1.55f * std::sqrt(rng.unit()));
        const float a = rng.unit() * 6.2831853f;
        qx_[i] = r * std::cos(a);
        qy_[i] = r * std::sin(a);
        const float tang = 0.32f;
        vx_[i] = -std::sin(a) * tang + 0.12f * rng.sym();
        vy_[i] = std::cos(a) * tang + 0.12f * rng.sym();
        qpx_[i] = qx_[i]; qpy_[i] = qy_[i];
        age_[i] = 0.0f;
        life_[i] = 2.5f + 7.0f * rng.unit();
    }

    void spawn_all() {
        ensure_capacity();
        wlfield::Rng rng(0xC0FFEEu ^ static_cast<std::uint32_t>(token_ * 2654435761u));
        for (int i = 0; i < active_; ++i) spawn_one(i, rng);
    }

    void ensure_capacity() {
        if (static_cast<int>(qx_.size()) >= active_) return;
        qx_.resize(active_); qy_.resize(active_);
        vx_.resize(active_); vy_.resize(active_);
        qpx_.resize(active_); qpy_.resize(active_);
        age_.resize(active_); life_.resize(active_);
    }

    // ── integration (optionally threaded) ─────────────────────────────────────
    void integrate(float h) {
        for (int i = 0; i < active_; ++i) { qpx_[i] = qx_[i]; qpy_[i] = qy_[i]; }

        const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        const int workers = (active_ >= 4000 && hw > 1)
            ? static_cast<int>(std::min(hw, 8u)) : 1;
        if (workers <= 1) {
            integrate_range(0, active_, h, 0x51u);
            return;
        }
        std::vector<std::thread> pool;
        pool.reserve(workers);
        const int chunk = (active_ + workers - 1) / workers;
        for (int w = 0; w < workers; ++w) {
            const int lo = w * chunk;
            const int hi = std::min(active_, lo + chunk);
            if (lo >= hi) break;
            pool.emplace_back([this, lo, hi, h, w] {
                integrate_range(lo, hi, h, 0x9E37u + static_cast<std::uint32_t>(w) * 2246822519u);
            });
        }
        for (auto& t : pool) t.join();
    }

    void integrate_range(int lo, int hi, float h, std::uint32_t rng_seed) {
        wlfield::Rng rng(rng_seed ^ static_cast<std::uint32_t>(time_ * 1000.0));
        const float resp2 = (view_radius_ * 3.2f) * (view_radius_ * 3.2f);
        const float damp = std::exp(-0.20f * h);  // gentle viscosity → concentrate on attractors
        for (int i = lo; i < hi; ++i) {
            float qx = qx_[i], qy = qy_[i], vx = vx_[i], vy = vy_[i];
            // RK2 midpoint for smoother, more faithful streaklines
            float a1x, a1y; op_.accel(qx, qy, vx, vy, a1x, a1y);
            const float hmx = qx + vx * (0.5f * h);
            const float hmy = qy + vy * (0.5f * h);
            const float hvx = vx + a1x * (0.5f * h);
            const float hvy = vy + a1y * (0.5f * h);
            float a2x, a2y; op_.accel(hmx, hmy, hvx, hvy, a2x, a2y);
            vx = (vx + a2x * h) * damp;
            vy = (vy + a2y * h) * damp;
            qx += hvx * h; qy += hvy * h;
            age_[i] += h;

            const bool dead = (qx * qx + qy * qy) > resp2 || age_[i] > life_[i]
                            || !std::isfinite(qx) || !std::isfinite(qy);
            if (dead) {
                spawn_one(i, rng);
            } else {
                qx_[i] = qx; qy_[i] = qy; vx_[i] = vx; vy_[i] = vy;
            }
        }
    }

    // ── painting ───────────────────────────────────────────────────────────────
    Color ramp(float t) const {
        t = std::clamp(t, 0.0f, 1.0f);
        const Color a = cool_, b = mid_, c = hot_;
        float r, g, bl;
        if (t < 0.5f) {
            const float u = t * 2.0f;
            r = a.r + (b.r - a.r) * u; g = a.g + (b.g - a.g) * u; bl = a.b + (b.b - a.b) * u;
        } else {
            const float u = (t - 0.5f) * 2.0f;
            r = b.r + (c.r - b.r) * u; g = b.g + (c.g - b.g) * u; bl = b.b + (c.b - b.b) * u;
        }
        return {static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                static_cast<unsigned char>(bl), 255};
    }

    // Per-particle screen-space appearance, computed once per visible particle.
    struct Splat { float sx, sy, px, py, t, wgt; bool streak; };

    void paint_swarm(Vector2 center, float scale, Rectangle stage) {
        const float margin = 40.0f;
        const float minx = stage.x - margin, maxx = stage.x + stage.width + margin;
        const float miny = stage.y - margin, maxy = stage.y + stage.height + margin;

        splats_.clear();
        if (static_cast<int>(splats_.capacity()) < active_) splats_.reserve(active_);
        for (int i = 0; i < active_; ++i) {
            const float sx = center.x + qx_[i] * scale;
            const float sy = center.y + qy_[i] * scale;
            if (sx < minx || sx > maxx || sy < miny || sy > maxy) continue;
            const float px = center.x + qpx_[i] * scale;
            const float py = center.y + qpy_[i] * scale;
            const float sp = std::sqrt(vx_[i] * vx_[i] + vy_[i] * vy_[i]);
            const float t = sp / (sp + speed_norm_);
            const float la = std::min(1.0f, age_[i] * 3.0f) *
                             std::min(1.0f, (life_[i] - age_[i]) * 1.5f);
            const float wgt = la * (0.3f + 0.7f * t); // slow/converged particles dimmer
            const float dx = sx - px, dy = sy - py;
            const float seglen2 = dx * dx + dy * dy;
            splats_.push_back({sx, sy, px, py, t, wgt, seglen2 > 0.16f && seglen2 < 6400.0f});
        }

        // Pass 1: streak lines — all share the default texture, so a single batch.
        for (const Splat& s : splats_) {
            if (!s.streak) continue;
            Color line = ramp(s.t); line.a = static_cast<unsigned char>(172.0f * s.wgt);
            DrawLineEx({s.px, s.py}, {s.sx, s.sy}, 1.4f + 1.7f * s.t, line);
        }
        // Pass 2: glow heads — all share the glow sprite, so one more batch (no
        // per-particle texture thrash → ~2 GPU flushes instead of ~2N).
        const float gw = glow_.width > 0 ? static_cast<float>(glow_.width) : 32.0f;
        const Rectangle gsrc = {0, 0, gw, gw};
        for (const Splat& s : splats_) {
            const float hs = 2.6f + 5.0f * s.t;
            Color head = ramp(s.t); head.a = static_cast<unsigned char>(155.0f * s.wgt);
            DrawTexturePro(glow_, gsrc, {s.sx - hs, s.sy - hs, hs * 2.0f, hs * 2.0f}, {0, 0}, 0.0f, head);
        }
    }
    std::vector<Splat> splats_;

    void draw_bloom_tap(Rectangle stage, Vector2 center, float zoom, unsigned char alpha) {
        const Rectangle src = {0.0f, 0.0f, static_cast<float>(width_), -static_cast<float>(height_)};
        const float w = stage.width * zoom, h = stage.height * zoom;
        const Rectangle dst = {center.x - w * 0.5f, center.y - h * 0.5f, w, h};
        // map only the stage sub-rectangle of the accum, zoomed about the centre
        const float u0 = stage.x / width_, v0 = stage.y / height_;
        const float uw = stage.width / width_, vh = stage.height / height_;
        const Rectangle ssrc = {u0 * width_, (1.0f - v0) * height_, uw * width_, -(vh * height_)};
        (void)src;
        DrawTexturePro(accum_.texture, ssrc, dst, {0, 0}, 0.0f, {255, 255, 255, alpha});
    }

    // ── backdrop: void + signature nebula + starfield + faint grid ─────────────
    struct Star { float x, y, b; };
    std::vector<Star> stars_;

    void build_stars(const std::string& seed) {
        std::uint32_t s = 0x1234u;
        for (char c : seed) s = s * 131u + static_cast<unsigned char>(c);
        wlfield::Rng rng(s | 1u);
        stars_.clear();
        stars_.reserve(260);
        for (int i = 0; i < 260; ++i) {
            stars_.push_back({rng.unit(), rng.unit(), 0.25f + 0.75f * rng.unit()});
        }
    }

    void draw_backdrop(Rectangle stage, Vector2 center, float scale) {
        DrawRectangleRec(stage, {3, 6, 12, 255});
        // signature nebula blooms
        DrawCircleGradient(static_cast<int>(stage.x + stage.width * 0.30f),
                           static_cast<int>(stage.y + stage.height * 0.34f),
                           std::min(stage.width, stage.height) * 0.55f,
                           with_a(cool_, 26), {0, 0, 0, 0});
        DrawCircleGradient(static_cast<int>(stage.x + stage.width * 0.72f),
                           static_cast<int>(stage.y + stage.height * 0.66f),
                           std::min(stage.width, stage.height) * 0.45f,
                           with_a(hot_, 22), {0, 0, 0, 0});
        // starfield with gentle parallax twinkle
        const float tw = static_cast<float>(time_);
        for (const Star& st : stars_) {
            const float px = stage.x + st.x * stage.width;
            const float py = stage.y + st.y * stage.height;
            const float tk = 0.6f + 0.4f * std::sin(tw * 1.7f + st.x * 40.0f + st.y * 23.0f);
            const unsigned char a = static_cast<unsigned char>(st.b * tk * 150.0f);
            DrawPixel(static_cast<int>(px), static_cast<int>(py), {200, 224, 255, a});
        }
        // faint polar grid centred on the field origin
        const Color ring = {60, 96, 120, 34};
        for (int i = 1; i <= 3; ++i) {
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                            scale * view_radius_ * (i / 3.0f), ring);
        }
        DrawLineEx({stage.x, center.y}, {stage.x + stage.width, center.y}, 1.0f, {60, 96, 120, 26});
        DrawLineEx({center.x, stage.y}, {center.x, stage.y + stage.height}, 1.0f, {60, 96, 120, 26});
    }

    static Color with_a(Color c, unsigned char a) { c.a = a; return c; }

    // ── streamline atlas (computed once) ───────────────────────────────────────
    std::vector<std::vector<wlfield::Vec2f>> streams_;

    void build_streamlines() {
        streams_.clear();
        const int seeds = 26;
        for (int s = 0; s < seeds; ++s) {
            const float a = (s / static_cast<float>(seeds)) * 6.2831853f;
            const float r = view_radius_ * (0.35f + 0.5f * ((s * 7) % 5) / 5.0f);
            float qx = r * std::cos(a), qy = r * std::sin(a);
            float vx = -std::sin(a) * 0.4f, vy = std::cos(a) * 0.4f;
            std::vector<wlfield::Vec2f> line;
            line.reserve(120);
            for (int i = 0; i < 120; ++i) {
                line.push_back({qx, qy});
                float ax, ay; op_.accel(qx, qy, vx, vy, ax, ay);
                const float dt = 1.0f / 60.0f;
                vx += ax * dt; vy += ay * dt;
                qx += vx * dt; qy += vy * dt;
                if (qx * qx + qy * qy > view_radius_ * view_radius_ * 9.0f) break;
            }
            if (line.size() > 4) streams_.push_back(std::move(line));
        }
    }

    void draw_streamlines(Vector2 center, float scale) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (const auto& line : streams_) {
            for (std::size_t i = 1; i < line.size(); ++i) {
                const Vector2 a = {center.x + line[i - 1].x * scale, center.y + line[i - 1].y * scale};
                const Vector2 b = {center.x + line[i].x * scale, center.y + line[i].y * scale};
                DrawLineEx(a, b, 1.0f, with_a(accent_, 14));
            }
        }
        EndBlendMode();
    }

    // ── hero worldline (the runtime's actual state) ───────────────────────────
    static constexpr int kHeroTrail = 420;
    std::vector<wlfield::Vec2f> hero_trail_;
    int hero_head_ = 0;

    void push_hero(float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        if (static_cast<int>(hero_trail_.size()) < kHeroTrail) {
            hero_trail_.push_back({x, y});
        } else {
            hero_trail_[hero_head_] = {x, y};
            hero_head_ = (hero_head_ + 1) % kHeroTrail;
        }
    }

    void draw_hero(Vector2 center, float scale) {
        const int n = static_cast<int>(hero_trail_.size());
        if (n < 2) return;
        BeginBlendMode(BLEND_ADDITIVE);
        Vector2 prev{};
        bool have_prev = false;
        Vector2 last{};
        for (int k = 0; k < n; ++k) {
            const int idx = (hero_head_ + k) % n;
            const wlfield::Vec2f& pt = hero_trail_[idx];
            const Vector2 s = {center.x + pt.x * scale, center.y + pt.y * scale};
            if (have_prev) {
                const float t = k / static_cast<float>(n);
                Color c = accent_;
                c.a = static_cast<unsigned char>(40.0f + 150.0f * t);
                DrawLineEx(prev, s, 1.4f + 2.0f * t, c);
            }
            prev = s; have_prev = true; last = s;
        }
        // bright comet head
        const float gw = glow_.width > 0 ? static_cast<float>(glow_.width) : 32.0f;
        const Rectangle gsrc = {0, 0, gw, gw};
        const float hs = 13.0f;
        DrawTexturePro(glow_, gsrc, {last.x - hs, last.y - hs, hs * 2.0f, hs * 2.0f}, {0, 0}, 0.0f,
                       with_a(WHITE, 220));
        EndBlendMode();
        DrawCircleV(last, 3.0f, WHITE);
    }

    // ── live metrics for the HUD ───────────────────────────────────────────────
    Metrics metrics_{};

    void compute_metrics() {
        // sub-sampled scan of the swarm
        double speed_sum = 0.0, curl_sum = 0.0, hand_sum = 0.0;
        double align_sum = 0.0;
        int count = 0;
        const int stride = std::max(1, active_ / 1500);
        float pvx = 0.0f, pvy = 0.0f;
        bool have_prev = false;
        for (int i = 0; i < active_; i += stride) {
            const float sp = std::sqrt(vx_[i] * vx_[i] + vy_[i] * vy_[i]);
            speed_sum += sp;
            const float ang = qx_[i] * vy_[i] - qy_[i] * vx_[i];
            hand_sum += ang;
            curl_sum += std::abs(ang);
            if (have_prev) {
                const float d = vx_[i] * pvx + vy_[i] * pvy;
                const float m = std::sqrt((vx_[i] * vx_[i] + vy_[i] * vy_[i] + 1e-6f) *
                                          (pvx * pvx + pvy * pvy + 1e-6f));
                align_sum += d / m;
            }
            pvx = vx_[i]; pvy = vy_[i]; have_prev = true;
            ++count;
        }
        if (count == 0) return;
        const float mean_speed = static_cast<float>(speed_sum / count);
        metrics_.flux = mean_speed;
        metrics_.vorticity = static_cast<float>(curl_sum / count);
        metrics_.handedness = static_cast<float>(hand_sum / count);
        metrics_.coherence = std::clamp(static_cast<float>(align_sum / count) * 0.5f + 0.5f, 0.0f, 1.0f);
        metrics_.alive = active_;

        // exotic index: blends vorticity, asymmetry of operator, warp & speed spread
        const float warp = std::sqrt(op_.W[0] * op_.W[0] + op_.W[1] * op_.W[1]
                                   + op_.W[2] * op_.W[2] + op_.W[3] * op_.W[3]);
        const float gyro = std::abs(op_.G0[1]) + std::abs(op_.G1[1]) + std::abs(op_.Tsk[1]);
        const float pexp = std::abs(op_.p) / 4.5f;
        float idx = 18.0f * std::min(1.0f, metrics_.vorticity / (view_radius_ * 0.9f))
                  + 24.0f * std::min(1.0f, warp / 0.28f)
                  + 22.0f * std::min(1.0f, gyro / 0.6f)
                  + 18.0f * pexp
                  + 18.0f * (1.0f - metrics_.coherence);
        // smooth toward target so the readout doesn't jitter
        metrics_.exotic_index += (std::clamp(idx, 0.0f, 100.0f) - metrics_.exotic_index) * 0.05f;
    }
};
