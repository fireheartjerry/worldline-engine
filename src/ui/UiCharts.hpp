#pragma once
// Scientific plotting toolkit for the Cosmos analysis instruments. Header-only,
// built on raylib + the Worldline glass/text primitives. Every routine renders
// inside a Rectangle frame with linear or logarithmic axes, a faint cyan grid,
// axis labels, and a frosted backing — so each universe tier can show real
// data-visualizations (HR diagrams, mass-radius curves, rotation curves, power
// spectra, phase portraits, allometric scaling, pyramids, pies, heatmaps).

#include "raylib.h"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace charts {

// ── Axes ────────────────────────────────────────────────────────────────────
struct Axes {
    Rectangle plot;          // inner plotting rectangle (inside labels)
    double x0 = 0, x1 = 1;    // data range (already in log10 units if logx)
    double y0 = 0, y1 = 1;
    bool logx = false, logy = false;
};

inline float map_x(const Axes& a, double x) {
    const double v = a.logx ? std::log10(std::max(1e-300, x)) : x;
    const double t = (v - a.x0) / std::max(1e-12, a.x1 - a.x0);
    return a.plot.x + static_cast<float>(std::clamp(t, -0.05, 1.05)) * a.plot.width;
}
inline float map_y(const Axes& a, double y) {
    const double v = a.logy ? std::log10(std::max(1e-300, y)) : y;
    const double t = (v - a.y0) / std::max(1e-12, a.y1 - a.y0);
    return a.plot.y + a.plot.height - static_cast<float>(std::clamp(t, -0.05, 1.05)) * a.plot.height;
}
inline Vector2 map_pt(const Axes& a, double x, double y) { return {map_x(a, x), map_y(a, y)}; }

// ── Card frame: frosted panel + title + corner ticks; returns the inner plot rect.
inline Rectangle plot_card(Rectangle r, const std::string& title, float ui, Color accent = WL::CYAN_CORE) {
    draw_glass_panel(r, {7, 16, 28, 224}, with_alpha(accent, 90), 0.07f, 3);
    draw_corner_brackets(r, with_alpha(accent, 120), 9.0f * ui, 1.2f, 3.0f);
    draw_text(title, {r.x + 9.0f * ui, r.y + 5.0f * ui}, 10.0f * ui, with_alpha(accent, 220));
    // Inner plot area leaves room for the title and left/bottom axis labels.
    return {r.x + 30.0f * ui, r.y + 20.0f * ui, r.width - 40.0f * ui, r.height - 40.0f * ui};
}

// Grid + frame for an Axes already seated in a card's inner rect.
inline void draw_grid(const Axes& a, float ui, const std::string& xlabel, const std::string& ylabel,
                      int xticks = 4, int yticks = 3) {
    DrawRectangleRec(a.plot, Color{4, 10, 18, 150});
    for (int i = 0; i <= xticks; ++i) {
        const float x = a.plot.x + a.plot.width * static_cast<float>(i) / xticks;
        DrawLineEx({x, a.plot.y}, {x, a.plot.y + a.plot.height}, 1.0f, with_alpha(WL::GLASS_BORDER, 34));
    }
    for (int i = 0; i <= yticks; ++i) {
        const float y = a.plot.y + a.plot.height * static_cast<float>(i) / yticks;
        DrawLineEx({a.plot.x, y}, {a.plot.x + a.plot.width, y}, 1.0f, with_alpha(WL::GLASS_BORDER, 34));
    }
    DrawRectangleLinesEx(a.plot, 1.0f, with_alpha(WL::GLASS_BORDER, 90));
    if (!xlabel.empty())
        draw_text(xlabel, {a.plot.x + a.plot.width * 0.5f - measure_ui_text(xlabel, 8.5f * ui).x * 0.5f,
                           a.plot.y + a.plot.height + 5.0f * ui}, 8.5f * ui, with_alpha(WL::TEXT_TERTIARY, 200));
    if (!ylabel.empty()) {
        // Vertical-ish label rotated would need a render texture; place it at top-left.
        draw_text(ylabel, {a.plot.x - 26.0f * ui, a.plot.y - 1.0f * ui}, 8.5f * ui,
                  with_alpha(WL::TEXT_TERTIARY, 200));
    }
}

// ── Series + functions ───────────────────────────────────────────────────────
inline void draw_series(const Axes& a, const std::vector<double>& xs, const std::vector<double>& ys,
                        Color c, float thick = 1.5f) {
    const std::size_t n = std::min(xs.size(), ys.size());
    Vector2 prev{};
    for (std::size_t i = 0; i < n; ++i) {
        const Vector2 p = map_pt(a, xs[i], ys[i]);
        if (i > 0) DrawLineEx(prev, p, thick, c);
        prev = p;
    }
}

// Sample a function y=f(x) across the x-range (respecting log x sampling).
inline void draw_function(const Axes& a, const std::function<double(double)>& f, Color c,
                          int samples = 80, float thick = 1.6f) {
    Vector2 prev{};
    for (int i = 0; i <= samples; ++i) {
        const double tx = a.x0 + (a.x1 - a.x0) * static_cast<double>(i) / samples;
        const double x = a.logx ? std::pow(10.0, tx) : tx;
        const Vector2 p = map_pt(a, x, f(x));
        if (i > 0) DrawLineEx(prev, p, thick, c);
        prev = p;
    }
}

// A filled area under a series (for spectra / SEDs).
inline void draw_area(const Axes& a, const std::function<double(double)>& f, Color c, int samples = 64) {
    for (int i = 0; i < samples; ++i) {
        const double tx0 = a.x0 + (a.x1 - a.x0) * static_cast<double>(i) / samples;
        const double tx1 = a.x0 + (a.x1 - a.x0) * static_cast<double>(i + 1) / samples;
        const double xa = a.logx ? std::pow(10.0, tx0) : tx0;
        const double xb = a.logx ? std::pow(10.0, tx1) : tx1;
        const Vector2 p0 = map_pt(a, xa, f(xa));
        const Vector2 p1 = map_pt(a, xb, f(xb));
        const float base = a.plot.y + a.plot.height;
        DrawTriangle({p0.x, base}, {p0.x, p0.y}, {p1.x, p1.y}, with_alpha(c, 40));
        DrawTriangle({p0.x, base}, {p1.x, p1.y}, {p1.x, base}, with_alpha(c, 40));
    }
}

// "You are here" marker with a halo + crosshair.
inline void draw_marker(const Axes& a, double x, double y, Color c, float ui, const std::string& label = "") {
    const Vector2 p = map_pt(a, x, y);
    DrawCircleV(p, 5.0f * ui, with_alpha(c, 60));
    DrawCircleV(p, 2.6f * ui, c);
    DrawLineEx({p.x - 8.0f * ui, p.y}, {p.x + 8.0f * ui, p.y}, 1.0f, with_alpha(c, 150));
    DrawLineEx({p.x, p.y - 8.0f * ui}, {p.x, p.y + 8.0f * ui}, 1.0f, with_alpha(c, 150));
    if (!label.empty()) draw_text(label, {p.x + 7.0f * ui, p.y - 11.0f * ui}, 8.5f * ui, c);
}

// ── Bars / pyramid / pie / heatmap ────────────────────────────────────────────
inline void draw_hbars(Rectangle r, const std::vector<float>& vals, const std::vector<std::string>& labels,
                       const std::vector<Color>& cols, float ui, bool pyramid = false) {
    if (vals.empty()) return;
    float mx = 1e-9f;
    for (float v : vals) mx = std::max(mx, v);
    const float rowh = r.height / static_cast<float>(vals.size());
    for (std::size_t i = 0; i < vals.size(); ++i) {
        const float w = std::max(6.0f, vals[i] / mx * (r.width - 4.0f));
        const float x = pyramid ? r.x + (r.width - w) * 0.5f : r.x;
        const Rectangle bar = {x, r.y + i * rowh + 1.0f, w, rowh - 3.0f};
        DrawRectangleRounded(bar, 0.35f, 4, with_alpha(cols[i % cols.size()], 180));
        if (i < labels.size())
            draw_text(labels[i], {r.x + 4.0f * ui, r.y + i * rowh + 0.5f * ui}, 8.5f * ui,
                      with_alpha(WL::TEXT_PRIMARY, 205));
    }
}

inline void draw_pie(Rectangle r, const std::vector<float>& fracs, const std::vector<Color>& cols) {
    const Vector2 c = {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
    const float rad = std::min(r.width, r.height) * 0.42f;
    float a0 = -90.0f, sum = 0.0f;
    for (float f : fracs) sum += f;
    if (sum <= 0.0f) return;
    for (std::size_t i = 0; i < fracs.size(); ++i) {
        const float a1 = a0 + 360.0f * fracs[i] / sum;
        DrawCircleSector(c, rad, a0, a1, 48, with_alpha(cols[i % cols.size()], 200));
        DrawCircleSectorLines(c, rad, a0, a1, 48, with_alpha(WL::VOID_DEEP, 200));
        a0 = a1;
    }
    DrawCircleV(c, rad * 0.42f, Color{6, 14, 24, 235}); // donut hole
}

inline void draw_heatmap(Rectangle r, const std::vector<float>& vals, int rows, int cols,
                         float lo, float hi, Color cool, Color hot) {
    if (rows <= 0 || cols <= 0) return;
    const float cw = r.width / cols, ch = r.height / rows;
    const float span = std::max(1e-9f, hi - lo);
    for (int yy = 0; yy < rows; ++yy)
        for (int xx = 0; xx < cols; ++xx) {
            const std::size_t idx = static_cast<std::size_t>(yy * cols + xx);
            const float t = idx < vals.size() ? std::clamp((vals[idx] - lo) / span, 0.0f, 1.0f) : 0.0f;
            DrawRectangle(static_cast<int>(r.x + xx * cw), static_cast<int>(r.y + yy * ch),
                          static_cast<int>(cw + 1), static_cast<int>(ch + 1), lerp_color(cool, hot, t));
        }
    DrawRectangleLinesEx(r, 1.0f, with_alpha(WL::GLASS_BORDER, 80));
}

} // namespace charts
