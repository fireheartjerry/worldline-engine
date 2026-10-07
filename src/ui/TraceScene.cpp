#include "ui/TraceScene.hpp"

#include "app/SeededUniverseRuntime.hpp"
#include "app/WorldlineCopy.hpp"
#include "ui/UiPrimitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Fixed-precision text that never shows "-0.00" for values that round to 0.
std::string metric_text(double value, int precision = 2) {
    const double unit = std::pow(10.0, -precision) * 0.5;
    if (std::abs(value) < unit) value = 0.0;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

// Phase-space preview.  Both axes share one scale so the orbit's real shape
// is kept (stretching x and y independently distorted it), centred in the
// card, with a start marker, a bright head and a faint axis cross.
void draw_preview_path(Rectangle rect, const std::vector<Vec2>& points, Color accent, float scale) {
    draw_card(rect, {8, 18, 30, 236}, with_alpha(accent, 90));
    if (points.size() < 2) {
        draw_text("No preview samples", {rect.x + 14.0f * scale, rect.y + 14.0f * scale}, 13.0f * scale, WL::TEXT_TERTIARY);
        return;
    }

    double min_x = points.front().x, max_x = min_x, min_y = points.front().y, max_y = min_y;
    for (const Vec2& point : points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }
    const float pad = 16.0f * scale;
    const float legend_h = 18.0f * scale;              // marker legend along the bottom edge
    const float avail_w = std::max(1.0f, rect.width - pad * 2.0f);
    const float avail_h = std::max(1.0f, rect.height - pad * 2.0f - legend_h);
    const double span_x = std::max(1.0e-6, max_x - min_x);
    const double span_y = std::max(1.0e-6, max_y - min_y);
    const float kk = static_cast<float>(std::min(avail_w / span_x, avail_h / span_y));
    const float cx = rect.x + rect.width * 0.5f;
    const float cy = rect.y + pad + avail_h * 0.5f;
    const double mx = (min_x + max_x) * 0.5;
    const double my = (min_y + max_y) * 0.5;
    auto to_screen = [&](const Vec2& p) {
        return Vector2{cx + static_cast<float>(p.x - mx) * kk, cy + static_cast<float>(p.y - my) * kk};
    };

    // Origin cross (where q = 0) when it lies inside the card.
    const Vector2 origin = to_screen({0.0, 0.0});
    if (CheckCollisionPointRec(origin, rect)) {
        DrawLineEx({rect.x + 6.0f, origin.y}, {rect.x + rect.width - 6.0f, origin.y}, 1.0f, with_alpha(WL::GLASS_BORDER, 50));
        DrawLineEx({origin.x, rect.y + 6.0f}, {origin.x, rect.y + rect.height - 6.0f}, 1.0f, with_alpha(WL::GLASS_BORDER, 50));
    }

    const float seg_count = static_cast<float>(std::max<std::size_t>(1, points.size() - 2));
    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const float t = static_cast<float>(index) / seg_count;
        DrawLineEx(to_screen(points[index]), to_screen(points[index + 1]), 1.4f + 0.8f * t,
                   with_alpha(accent, static_cast<unsigned char>(90 + 120 * t)));
    }
    const Vector2 start = to_screen(points.front());
    const Vector2 head = to_screen(points.back());
    DrawCircleLines(static_cast<int>(start.x), static_cast<int>(start.y), 4.0f * scale, with_alpha(WL::TEXT_SECONDARY, 200));
    DrawCircleV(head, 7.0f * scale, with_alpha(accent, 50));
    DrawCircleV(head, 3.2f * scale, accent);

    // Legend for the two markers.
    const float ls = 11.5f * scale;
    const float ly = rect.y + rect.height - pad * 0.5f - ls;
    DrawCircleLines(static_cast<int>(rect.x + pad), static_cast<int>(ly + ls * 0.5f), 3.5f * scale, with_alpha(WL::TEXT_SECONDARY, 200));
    draw_text("start", {rect.x + pad + 8.0f * scale, ly}, ls, WL::TEXT_TERTIARY);
    const float ex = rect.x + pad + 8.0f * scale + measure_ui_text("start", ls).x + 16.0f * scale;
    DrawCircleV({ex, ly + ls * 0.5f}, 3.0f * scale, accent);
    draw_text("end of preview", {ex + 8.0f * scale, ly}, ls, WL::TEXT_TERTIARY);
}

// 2x2 tensor with a title strip.  Cell tint encodes sign (cyan +, violet -)
// and magnitude (stronger = more saturated), so structure reads at a glance.
void draw_matrix(Rectangle rect, const char* symbol, const char* name, const double matrix[2][2],
                 Color accent, float scale) {
    draw_card(rect, {8, 18, 30, 236}, with_alpha(accent, 90));
    const float pad = 10.0f * scale;
    const float head = 22.0f * scale;
    draw_text(symbol, {rect.x + pad, rect.y + 7.0f * scale}, 15.0f * scale, accent);
    draw_text_fit(name, {rect.x + pad + measure_ui_text(symbol, 15.0f * scale).x + 8.0f * scale, rect.y + 9.0f * scale},
                  rect.width - pad * 2.0f - 30.0f * scale, 12.0f * scale, 10.0f * scale, with_alpha(accent, 170));

    double peak = 1.0e-9;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 2; ++c) peak = std::max(peak, std::abs(matrix[r][c]));

    const float gap = 4.0f * scale;
    const float cell_w = (rect.width - pad * 2.0f - gap) * 0.5f;
    const float cell_h = (rect.height - head - pad - 4.0f * scale - gap) * 0.5f;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 2; ++col) {
            const Rectangle cell = {rect.x + pad + col * (cell_w + gap),
                                    rect.y + head + 4.0f * scale + row * (cell_h + gap), cell_w, cell_h};
            const double v = matrix[row][col];
            const float strength = static_cast<float>(std::min(1.0, std::abs(v) / peak));
            const bool zero = std::abs(v) < 0.005;
            const Color base = zero ? Color{14, 24, 36, 228}
                                    : lerp_color(Color{12, 24, 38, 228},
                                                 v >= 0.0 ? Color{14, 70, 86, 236} : Color{66, 26, 92, 236},
                                                 0.35f + 0.65f * strength);
            DrawRectangleRounded(cell, 0.10f, 6, base);
            DrawRectangleRoundedLines(cell, 0.10f, 6, 1.0f, with_alpha(accent, 60));
            draw_text_fit_in(metric_text(v, 2), {cell.x + 6.0f * scale, cell.y, cell.width - 12.0f * scale, cell.height},
                             std::min(16.0f * scale, cell.height * 0.55f), 10.0f * scale,
                             zero ? WL::TEXT_TERTIARY : WL::TEXT_PRIMARY, 0.5f);
        }
    }
}

// The 32 machine lanes as bars, coloured by the tensor group each lane feeds.
void draw_lane_strip(Rectangle rect, const std::vector<double>& lanes, float scale) {
    draw_card(rect, {8, 18, 30, 236}, with_alpha(WL::GLASS_BORDER, 90));
    if (lanes.empty()) return;
    const float pad = 10.0f * scale;
    const float n = static_cast<float>(lanes.size());
    const float slot = (rect.width - pad * 2.0f) / n;
    const float base = rect.y + rect.height - pad;
    const float h = rect.height - pad * 2.0f;
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        Color c = WL::TEXT_TERTIARY;
        if (i <= 2) c = WL::CYAN_CORE;              // metric g
        else if (i <= 5) c = WL::VIOLET_CORE;       // potential V
        else if (i <= 8) c = WL::PLASMA_GREEN;      // symmetry S
        else if (i <= 14) c = WL::XENON_CORE;       // couplings C0, C1
        else if (i >= 30) c = WL::ACCENT_NET;       // launch state
        const float v = static_cast<float>(std::clamp(lanes[i], 0.0, 1.0));
        const float x = rect.x + pad + static_cast<float>(i) * slot;
        DrawRectangleRec({x + slot * 0.18f, base - h, slot * 0.64f, h}, {255, 255, 255, 8});
        DrawRectangleRec({x + slot * 0.18f, base - h * v, slot * 0.64f, std::max(1.0f, h * v)}, with_alpha(c, 200));
    }
}

} // namespace

TraceSceneResult draw_trace_scene(AppState& app,
                                  Rectangle viewport) {
    TraceSceneResult result;
    SeededUniverseUiState& seeded = app.ui.seeded;
    TraceSceneState& trace = app.ui.trace;
    if (!seeded.result.ready && seeded.result.error.empty()) {
        regenerate_seeded_universe(seeded);
    }

    const float scale = std::clamp(std::min(viewport.width / 1500.0f, viewport.height / 900.0f), 0.90f, 1.20f);
    const float margin = 20.0f * scale;
    const float gap = 16.0f * scale;

    const Rectangle header = {viewport.x + margin, viewport.y + margin, viewport.width - margin * 2.0f, 76.0f * scale};
    const float body_y = header.y + header.height + gap;
    const float body_h = viewport.y + viewport.height - margin - body_y;
    const float left_w = std::floor((header.width - gap) * 0.40f);
    const Rectangle left = {header.x, body_y, left_w, body_h};
    const Rectangle right = {left.x + left_w + gap, body_y, header.width - left_w - gap, body_h};

    // ── Header ────────────────────────────────────────────────────────────────
    draw_card(header, {6, 14, 26, 236}, with_alpha(WL::CYAN_DIM, 110));
    const Vector2 back_size = back_button_size(scale);
    if (draw_back_button({header.x + 16.0f * scale, header.y + (header.height - back_size.y) * 0.5f, back_size.x, back_size.y}, scale)
        || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
    }
    const float title_x = header.x + 16.0f * scale + back_size.x + 20.0f * scale;
    DrawLineEx({title_x - 10.0f * scale, header.y + 16.0f * scale}, {title_x - 10.0f * scale, header.y + header.height - 16.0f * scale},
               1.0f, with_alpha(WL::GLASS_BORDER, 90));

    // The return key says where it goes (Trace opens from the workspace or the
    // Reference System; Back already covers the menu case).
    const AppScreen origin = app.ui.seeded_debug_return_screen;
    const char* return_label = origin == AppScreen::SEEDED_WORKSPACE ? "Return to Workspace"
                             : origin == AppScreen::REFERENCE_LAB    ? "Return to Reference"
                                                                     : nullptr;
    float title_right = header.x + header.width - 16.0f * scale;
    if (return_label != nullptr) {
        const float rw = 210.0f * scale;
        const Rectangle rr = {header.x + header.width - 16.0f * scale - rw, header.y + (header.height - 38.0f * scale) * 0.5f, rw, 38.0f * scale};
        if (draw_button(rr, return_label, {10, 84, 98, 240}, {18, 126, 140, 255}, WL::CYAN_CORE, true, scale)) {
            result.open_workspace = true;
        }
        title_right = rr.x - 16.0f * scale;
    }
    draw_text_fit("Trace", {title_x, header.y + 14.0f * scale}, title_right - title_x, 26.0f * scale, 18.0f * scale, WL::TEXT_PRIMARY);
    draw_text_fit("Generator output, law preview, and glossary, without crowding the workspace.",
                  {title_x, header.y + 46.0f * scale}, title_right - title_x, 14.0f * scale, 11.0f * scale, WL::TEXT_SECONDARY);

    // ── Left: generator summary + glossary ────────────────────────────────────
    draw_card(left, {6, 14, 24, 230}, with_alpha(WL::GLASS_BORDER, 100));
    const float pad = 16.0f * scale;
    const float inner = left.width - pad * 2.0f;
    draw_text("Generator Summary", {left.x + pad, left.y + 14.0f * scale}, 19.0f * scale, WL::TEXT_PRIMARY);
    if (!seeded.result.ready) {
        const std::string why = seeded.result.error.empty() ? std::string("No generated universe loaded.")
                                                            : "Generation failed: " + seeded.result.error;
        draw_text_block_fit(why, {left.x + pad, left.y + 46.0f * scale, inner, 80.0f * scale},
                            14.0f * scale, WL::TEXT_SECONDARY, 4.0f * scale);
        draw_card(right, {6, 14, 24, 230}, with_alpha(WL::GLASS_BORDER, 100));
        return result;
    }
    draw_text_fit("seed \"" + seeded.result.seed + "\"", {left.x + pad, left.y + 40.0f * scale}, inner,
                  13.0f * scale, 11.0f * scale, with_alpha(WL::CYAN_CORE, 190));

    float y = left.y + 62.0f * scale;
    const float summary_h = std::clamp(left.height * 0.20f, 54.0f * scale, 112.0f * scale);
    draw_text_block_fit(ui_prose_summary(seeded.result.descriptor), {left.x + pad, y, inner, summary_h},
                        13.0f * scale, WL::TEXT_SECONDARY, 3.5f * scale);
    y += summary_h + 12.0f * scale;

    const float tile_gap = 10.0f * scale;
    const float tile_w = (inner - tile_gap) * 0.5f;
    const float tile_h = 52.0f * scale;
    draw_metric({left.x + pad, y, tile_w, tile_h}, "Gain", metric_text(seeded.result.law_preview.linear_gain, 2), scale);
    draw_metric({left.x + pad + tile_w + tile_gap, y, tile_w, tile_h}, "Accel Ceiling",
                metric_text(seeded.result.law_preview.accel_ceiling, 2), scale);
    draw_metric({left.x + pad, y + tile_h + tile_gap, tile_w, tile_h}, "Radius Mean",
                metric_text(seeded.result.law_preview.radius_mean, 2), scale);
    draw_metric({left.x + pad + tile_w + tile_gap, y + tile_h + tile_gap, tile_w, tile_h}, "Handedness",
                metric_text(seeded.result.law_preview.handedness, 2), scale);
    y += tile_h * 2.0f + tile_gap + 18.0f * scale;

    draw_text("Glossary", {left.x + pad, y}, 16.0f * scale, with_alpha(WL::CYAN_CORE, 200));
    y += 26.0f * scale;
    const Rectangle gloss = {left.x + pad, y, inner, left.y + left.height - pad - y};
    const float term_size = 14.0f * scale;
    const float def_size = 12.5f * scale;
    const float def_gap = 3.0f * scale;
    const float entry_gap = 12.0f * scale;
    const float text_w = gloss.width - 12.0f * scale;   // leave room for the scrollbar
    float content_h = 0.0f;
    for (const GlossaryEntry& entry : Copy::glossary_entries()) {
        content_h += term_size + 4.0f * scale
            + measure_wrapped_ui_text_height(entry.short_definition, text_w, def_size, def_gap) + entry_gap;
    }
    const float max_scroll = std::max(0.0f, content_h - entry_gap - gloss.height);
    if (CheckCollisionPointRec(GetMousePosition(), gloss)) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) trace.scroll -= wheel * 40.0f * scale;
    }
    trace.scroll = std::clamp(trace.scroll, 0.0f, max_scroll);
    if (gloss.height > term_size) {
        BeginScissorMode(static_cast<int>(gloss.x), static_cast<int>(gloss.y),
                         static_cast<int>(gloss.width), static_cast<int>(gloss.height));
        float gy = gloss.y - trace.scroll;
        for (const GlossaryEntry& entry : Copy::glossary_entries()) {
            const float def_h = measure_wrapped_ui_text_height(entry.short_definition, text_w, def_size, def_gap);
            const float entry_h = term_size + 4.0f * scale + def_h;
            if (gy + entry_h >= gloss.y && gy <= gloss.y + gloss.height) {
                DrawRectangleRec({gloss.x, gy + 2.0f * scale, 2.0f * scale, term_size - 2.0f * scale}, with_alpha(WL::CYAN_CORE, 160));
                draw_text(entry.term, {gloss.x + 10.0f * scale, gy}, term_size, WL::TEXT_PRIMARY);
                draw_text_block(entry.short_definition, {gloss.x + 10.0f * scale, gy + term_size + 4.0f * scale, text_w - 10.0f * scale, def_h + 2.0f},
                                def_size, WL::TEXT_SECONDARY, def_gap);
            }
            gy += entry_h + entry_gap;
        }
        EndScissorMode();
        draw_scrollbar(gloss, trace.scroll, max_scroll);
        if (max_scroll > 0.0f && trace.scroll < max_scroll - 1.0f) {
            // Fade hint that more entries continue below.
            DrawRectangleGradientV(static_cast<int>(gloss.x), static_cast<int>(gloss.y + gloss.height - 24.0f * scale),
                                   static_cast<int>(gloss.width - 12.0f * scale), static_cast<int>(24.0f * scale),
                                   {6, 14, 24, 0}, {6, 14, 24, 230});
        }
    }

    // ── Right: law preview, lanes, tensors ────────────────────────────────────
    draw_card(right, {6, 14, 24, 230}, with_alpha(WL::GLASS_BORDER, 100));
    const float rpad = 16.0f * scale;
    const float rinner = right.width - rpad * 2.0f;
    draw_text("Law Preview", {right.x + rpad, right.y + 14.0f * scale}, 19.0f * scale, WL::TEXT_PRIMARY);
    {
        const std::string caption = std::to_string(seeded.result.law_preview.phase_path.size()) + " samples  |  peak accel "
            + metric_text(seeded.result.law_preview.max_accel, 2) + "  |  p "
            + (seeded.result.meta_spec.p_dynamic ? std::string("dynamic") : metric_text(seeded.result.meta_spec.p, 3));
        const float cs = 12.0f * scale;
        const std::string shown = ellipsize_ui_text(caption, rinner - 150.0f * scale, cs);
        draw_text(shown, {right.x + right.width - rpad - measure_ui_text(shown, cs).x, right.y + 20.0f * scale}, cs, WL::TEXT_TERTIARY);
    }

    const float content_top = right.y + 48.0f * scale;
    const float content_bottom = right.y + right.height - rpad;
    const float matrix_gap = 10.0f * scale;
    const float matrix_h = std::clamp((right.height - 48.0f * scale) * 0.25f, 96.0f * scale, 128.0f * scale);
    const float tensors_h = 24.0f * scale + matrix_h * 2.0f + matrix_gap;
    const float lanes_h = 22.0f * scale + 46.0f * scale;
    const float avail = content_bottom - content_top - tensors_h - 14.0f * scale;
    const bool show_lanes = avail - lanes_h - 12.0f * scale >= 150.0f * scale;
    const float preview_h = std::max(90.0f * scale, avail - (show_lanes ? lanes_h + 12.0f * scale : 0.0f));

    const Rectangle preview_rect = {right.x + rpad, content_top, rinner, preview_h};
    draw_preview_path(preview_rect, seeded.result.law_preview.phase_path, WL::CYAN_CORE, scale);
    float ry = preview_rect.y + preview_rect.height + 12.0f * scale;

    if (show_lanes) {
        draw_text("Machine lanes", {right.x + rpad, ry}, 14.0f * scale, with_alpha(WL::CYAN_CORE, 190));
        // Colour key, right-aligned: which tensor group each lane feeds.
        struct KeyItem { const char* label; Color color; };
        const KeyItem keys[] = {{"g", WL::CYAN_CORE}, {"V", WL::VIOLET_CORE}, {"S", WL::PLASMA_GREEN},
                                {"C", WL::XENON_CORE}, {"helpers", WL::TEXT_TERTIARY}, {"launch", WL::ACCENT_NET}};
        const float ks = 11.5f * scale;
        float kx = right.x + right.width - rpad;
        for (int i = 5; i >= 0; --i) {
            const float w = measure_ui_text(keys[i].label, ks).x;
            kx -= w;
            draw_text(keys[i].label, {kx, ry + 2.0f * scale}, ks, WL::TEXT_TERTIARY);
            kx -= 12.0f * scale;
            DrawRectangleRec({kx, ry + 4.0f * scale, 8.0f * scale, 8.0f * scale}, with_alpha(keys[i].color, 210));
            kx -= 14.0f * scale;
        }
        draw_lane_strip({right.x + rpad, ry + 22.0f * scale, rinner, 46.0f * scale}, seeded.result.lanes, scale);
        ry += lanes_h + 12.0f * scale;
    }

    draw_text("Primary tensors", {right.x + rpad, ry}, 14.0f * scale, with_alpha(WL::CYAN_CORE, 190));
    const float matrix_y = ry + 24.0f * scale;
    const float matrix_w = (rinner - matrix_gap) * 0.5f;
    const MetaSpec& ms = seeded.result.meta_spec;
    draw_matrix({right.x + rpad, matrix_y, matrix_w, matrix_h}, "g", "metric", ms.g, WL::CYAN_CORE, scale);
    draw_matrix({right.x + rpad + matrix_w + matrix_gap, matrix_y, matrix_w, matrix_h}, "V", "potential", ms.V, WL::VIOLET_CORE, scale);
    draw_matrix({right.x + rpad, matrix_y + matrix_h + matrix_gap, matrix_w, matrix_h}, "S", "symmetry", ms.S, WL::PLASMA_GREEN, scale);
    draw_matrix({right.x + rpad + matrix_w + matrix_gap, matrix_y + matrix_h + matrix_gap, matrix_w, matrix_h}, "W", "warp contamination",
                ms.W, WL::XENON_CORE, scale);

    return result;
}
