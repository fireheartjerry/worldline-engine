#pragma once
#include "raylib.h"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

// ── Worldline Alien-Tech Palette ─────────────────────────────────────────────
// Deep-void blacks, bioluminescent cyan/violet, hot xenon orange accents.
// Everything feels like it was designed inside a neutron star.

namespace WL {
    // Void backgrounds
    constexpr Color VOID_DEEP      = {  2,   4,   9, 255};
    constexpr Color VOID_MID       = {  5,  10,  18, 255};
    constexpr Color VOID_SURFACE   = {  8,  16,  28, 255};

    // Bioluminescent primaries
    constexpr Color CYAN_CORE      = { 64, 228, 240, 255};  // main brand glow
    constexpr Color CYAN_DIM       = { 32, 140, 158, 255};
    constexpr Color VIOLET_CORE    = {148,  72, 255, 255};  // exotic / seeded
    constexpr Color VIOLET_DIM     = { 80,  38, 148, 255};
    constexpr Color XENON_CORE     = {255, 148,  48, 255};  // warning / energy
    constexpr Color XENON_DIM      = {140,  72,  18, 255};
    constexpr Color PLASMA_GREEN   = { 72, 255, 180, 255};  // live / running
    constexpr Color PLASMA_DIM     = { 28, 110,  72, 255};

    // Surface glass
    constexpr Color GLASS_1        = {  8,  18,  32, 210};  // card fill dark
    constexpr Color GLASS_2        = { 10,  22,  40, 195};  // card fill mid
    constexpr Color GLASS_BORDER   = { 32,  80, 108,  90};  // dim border

    // Text hierarchy
    constexpr Color TEXT_PRIMARY   = {230, 242, 248, 255};
    constexpr Color TEXT_SECONDARY = {120, 172, 196, 218};
    constexpr Color TEXT_TERTIARY  = { 76, 128, 152, 185};
    constexpr Color TEXT_INACTIVE  = { 52,  90, 112, 140};

    // HUD diagnostic accents  (kept compatible with CanvasHud callers)
    constexpr Color ACCENT_VELOCITY   = { 64, 228, 240, 255};
    constexpr Color ACCENT_GRAVITY    = {112, 142, 255, 255};
    constexpr Color ACCENT_DRAG       = {255, 148,  72, 255};
    constexpr Color ACCENT_REACTION   = { 72, 248, 182, 255};
    constexpr Color ACCENT_NET        = {255, 102, 196, 255};
    constexpr Color ACCENT_LINK_DRAG  = {255, 210, 100, 255};
    constexpr Color ACCENT_TORQUE     = {255, 108, 140, 255};
    constexpr Color ACCENT_FLOW       = { 96, 220, 255, 255};
}

// ── Font state ────────────────────────────────────────────────────────────────
struct UiFontState {
    Font regular{};
    bool loaded = false;
};

inline UiFontState& ui_font_state() {
    static UiFontState state{GetFontDefault(), false};
    return state;
}

inline float ui_text_spacing(float size) {
    return std::max(0.0f, size * 0.018f);
}

inline void init_ui_font() {
    UiFontState& state = ui_font_state();
    if (state.loaded) return;

    // Prefer a narrow, technical-looking system font — fits the alien-terminal aesthetic
    const std::array<const char*, 6> candidates{{
        "C:/Windows/Fonts/consola.ttf",   // Consolas — monospace, sharp, techy
        "C:/Windows/Fonts/cour.ttf",      // Courier New fallback
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        "C:/Windows/Fonts/arial.ttf",
    }};

    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        Font font = LoadFontEx(path, 96, nullptr, 0);
        if (font.texture.id != 0) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            state.regular = font;
            state.loaded = true;
            return;
        }
    }
}

inline void shutdown_ui_font() {
    UiFontState& state = ui_font_state();
    if (state.loaded) {
        UnloadFont(state.regular);
        state.regular = GetFontDefault();
        state.loaded = false;
    }
}

inline const Font& ui_font() { return ui_font_state().regular; }

// ── Text helpers ──────────────────────────────────────────────────────────────
inline Vector2 measure_ui_text(const std::string& text, float size, float spacing = -1.0f) {
    const float sp = (spacing >= 0.0f) ? spacing : ui_text_spacing(size);
    return MeasureTextEx(ui_font(), text.c_str(), size, sp);
}

inline float fit_ui_text_size(const std::string& text, float max_width, float preferred_size, float min_size) {
    float size = preferred_size;
    while (size > min_size && measure_ui_text(text, size).x > max_width)
        size -= 0.5f;
    return size;
}

inline std::vector<std::string> wrap_ui_text(const std::string& text, float max_width, float size) {
    std::vector<std::string> lines;
    if (text.empty() || max_width <= 0.0f) { lines.push_back(text); return lines; }

    std::string paragraph;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        paragraph = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (paragraph.empty()) {
            lines.emplace_back();
        } else {
            std::string current;
            std::size_t word_start = 0;
            while (word_start < paragraph.size()) {
                while (word_start < paragraph.size() && paragraph[word_start] == ' ') ++word_start;
                if (word_start >= paragraph.size()) break;
                std::size_t word_end = paragraph.find(' ', word_start);
                const std::string word = paragraph.substr(word_start, word_end == std::string::npos ? std::string::npos : word_end - word_start);
                const std::string candidate = current.empty() ? word : current + " " + word;
                if (!current.empty() && measure_ui_text(candidate, size).x > max_width) {
                    lines.push_back(current);
                    current = word;
                } else {
                    current = candidate;
                }
                if (word_end == std::string::npos) break;
                word_start = word_end + 1;
            }
            if (!current.empty()) lines.push_back(current);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (lines.empty()) lines.push_back(text);
    return lines;
}

inline float measure_wrapped_ui_text_height(const std::string& text, float max_width, float size, float line_gap = 4.0f) {
    const auto lines = wrap_ui_text(text, max_width, size);
    if (lines.empty()) return 0.0f;
    return lines.size() * size + (lines.size() - 1) * line_gap;
}

inline void draw_text_block(const std::string& text, Rectangle bounds, float size, Color color, float line_gap = 4.0f) {
    const auto lines = wrap_ui_text(text, bounds.width, size);
    float y = bounds.y;
    for (const auto& line : lines) {
        DrawTextEx(ui_font(), line.c_str(), {bounds.x, y}, size, ui_text_spacing(size), color);
        y += size + line_gap;
        if (y > bounds.y + bounds.height) break;
    }
}

// ── Color utilities ───────────────────────────────────────────────────────────
inline Color with_alpha(Color c, unsigned char a) { c.a = a; return c; }

// Additive glow tint — mix color toward white at given strength
inline Color glow_tint(Color c, float strength) {
    return {
        static_cast<unsigned char>(c.r + (255 - c.r) * strength),
        static_cast<unsigned char>(c.g + (255 - c.g) * strength),
        static_cast<unsigned char>(c.b + (255 - c.b) * strength),
        c.a
    };
}

// ── Background ────────────────────────────────────────────────────────────────
// Deep-void starfield with two nebula blooms — cyan at top-left, orange at bottom-right.
inline void draw_background(int screen_width, int screen_height) {
    const Rectangle frame = {0.0f, 0.0f, static_cast<float>(screen_width), static_cast<float>(screen_height)};

    // Base void gradient
    DrawRectangleGradientEx(frame,
        { 2,  5, 12, 255},   // TL
        { 4,  8, 16, 255},   // TR
        { 6, 12, 22, 255},   // BR
        { 3,  6, 14, 255});  // BL

    // Cyan nebula bloom — upper left
    DrawCircleGradient(
        static_cast<int>(screen_width * 0.12f),
        static_cast<int>(screen_height * 0.18f),
        screen_height * 0.52f,
        { 12, 88, 108,  72},
        {0, 0, 0, 0});

    // Secondary violet nebula — upper right
    DrawCircleGradient(
        static_cast<int>(screen_width * 0.88f),
        static_cast<int>(screen_height * 0.22f),
        screen_height * 0.38f,
        { 48, 18, 98,  54},
        {0, 0, 0, 0});

    // Xenon bloom — lower right
    DrawCircleGradient(
        static_cast<int>(screen_width * 0.86f),
        static_cast<int>(screen_height * 0.80f),
        screen_height * 0.34f,
        {140,  58, 12,  50},
        {0, 0, 0, 0});

    // Subtle horizontal scan-line feel — very faint repeating lines
    for (int y = 0; y < screen_height; y += 4) {
        DrawLine(0, y, screen_width, y, {255, 255, 255, 3});
    }
}

// ── Core drawing primitives ────────────────────────────────────────────────────

// Card: sharp corners (roundness 0.04), glassy dark fill, accent border glow
inline void draw_card(Rectangle rect, Color fill, Color outline) {
    // Outer glow layer
    DrawRectangleRounded({rect.x - 1, rect.y - 1, rect.width + 2, rect.height + 2},
                         0.04f, 8, with_alpha(outline, 40));
    DrawRectangleRounded(rect, 0.04f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.04f, 8, 1.2f, with_alpha(outline, 160));

    // Top-edge highlight — simulates internal panel lighting
    DrawLineEx({rect.x + 4, rect.y + 1}, {rect.x + rect.width - 4, rect.y + 1},
               1.0f, with_alpha(outline, 60));
}

// Accentuated card with a colored left-edge stripe
inline void draw_card_accented(Rectangle rect, Color fill, Color border, Color stripe) {
    draw_card(rect, fill, border);
    // Left stripe — 2px wide, inset 1px, full height minus top/bottom radius
    DrawRectangle(static_cast<int>(rect.x + 1),
                  static_cast<int>(rect.y + 4),
                  2,
                  static_cast<int>(rect.height - 8),
                  with_alpha(stripe, 200));
}

inline void draw_text(const std::string& text, Vector2 position, float size, Color color, float spacing = -1.0f) {
    const float sp = (spacing >= 0.0f) ? spacing : ui_text_spacing(size);
    DrawTextEx(ui_font(), text.c_str(), position, size, sp, color);
}

// Badge: pill shape with glowing border
inline void draw_badge(Rectangle rect, const char* label, Color fill, Color text_color, float scale = 1.0f) {
    // Glow halo
    DrawRectangleRounded({rect.x - 1, rect.y - 1, rect.width + 2, rect.height + 2},
                         0.50f, 10, with_alpha(text_color, 18));
    DrawRectangleRounded(rect, 0.50f, 10, fill);
    DrawRectangleRoundedLines(rect, 0.50f, 10, 1.0f, with_alpha(text_color, 80));

    const float th = fit_ui_text_size(label, rect.width - 10.0f * scale, 14.0f * scale, 10.0f * scale);
    const Vector2 ts = measure_ui_text(label, th);
    draw_text(label, {rect.x + (rect.width - ts.x) * 0.5f, rect.y + (rect.height - ts.y) * 0.5f}, th, text_color);
}

// Button: sharp corners, glow on hover, sci-fi feel
inline bool draw_button(Rectangle rect,
                        const char* label,
                        Color base,
                        Color hover,
                        Color text_color,
                        bool enabled = true,
                        float scale = 1.0f) {
    const Vector2 mouse = GetMousePosition();
    const bool hot     = enabled && CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    Color fill = enabled ? (hot ? hover : base) : with_alpha(base, 70);
    Color text = enabled ? text_color : with_alpha(text_color, 90);

    if (hot) {
        // Outer glow halo on hover
        DrawRectangleRounded({rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4},
                             0.14f, 8, with_alpha(hover, 40));
    }

    DrawRectangleRounded(rect, 0.14f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.14f, 8, 1.2f, with_alpha(text_color, enabled ? (hot ? 120 : 55) : 28));

    // Top shimmer line
    if (enabled) {
        DrawLineEx({rect.x + 4, rect.y + 1}, {rect.x + rect.width - 4, rect.y + 1},
                   1.0f, with_alpha(text_color, hot ? 60 : 28));
    }

    const float th = fit_ui_text_size(label, rect.width - 16.0f * scale, 19.0f * scale, 12.0f * scale);
    const Vector2 ts = measure_ui_text(label, th);
    draw_text(label, {rect.x + (rect.width - ts.x) * 0.5f, rect.y + (rect.height - ts.y) * 0.5f}, th, text);
    return pressed;
}

// Checkbox: alien-panel toggle style
inline bool draw_checkbox(Rectangle rect,
                          bool checked,
                          const char* label,
                          const char* note,
                          Color accent,
                          float scale = 1.0f) {
    const Vector2 mouse = GetMousePosition();
    const bool hot     = CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    const Color fill   = hot ? Color{10, 22, 38, 248} : Color{7, 16, 28, 230};
    const Color border = hot ? with_alpha(accent, 120) : Color{28, 58, 80,  90};
    DrawRectangleRounded(rect, 0.08f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.08f, 8, 1.1f, border);

    // Indicator diamond — more alien than a plain square
    const float bx = rect.x + 13.0f * scale;
    const float by = rect.y + rect.height * 0.5f;
    const float bs = 8.0f * scale;
    if (checked) {
        // Filled glowing diamond
        const Vector2 pts[4] = {
            {bx,      by - bs},
            {bx + bs, by},
            {bx,      by + bs},
            {bx - bs, by}
        };
        DrawTriangle(pts[0], pts[3], pts[2], with_alpha(accent, 210));
        DrawTriangle(pts[0], pts[2], pts[1], with_alpha(accent, 210));
        // Glow ring
        DrawCircleLines(static_cast<int>(bx), static_cast<int>(by),
                        bs + 2.0f * scale, with_alpha(accent, 80));
    } else {
        // Hollow diamond outline
        DrawLine(static_cast<int>(bx),      static_cast<int>(by - bs),
                 static_cast<int>(bx + bs), static_cast<int>(by),
                 with_alpha(accent, 90));
        DrawLine(static_cast<int>(bx + bs), static_cast<int>(by),
                 static_cast<int>(bx),      static_cast<int>(by + bs),
                 with_alpha(accent, 90));
        DrawLine(static_cast<int>(bx),      static_cast<int>(by + bs),
                 static_cast<int>(bx - bs), static_cast<int>(by),
                 with_alpha(accent, 90));
        DrawLine(static_cast<int>(bx - bs), static_cast<int>(by),
                 static_cast<int>(bx),      static_cast<int>(by - bs),
                 with_alpha(accent, 90));
    }

    const float lx = rect.x + 32.0f * scale;
    const float label_size = fit_ui_text_size(label, rect.width - 48.0f * scale, 18.0f * scale, 13.0f * scale);
    draw_text(label, {lx, rect.y + 8.0f * scale}, label_size, WL::TEXT_PRIMARY);
    draw_text_block(note,
                    {lx, rect.y + 26.0f * scale, rect.width - 44.0f * scale, rect.height - 30.0f * scale},
                    13.5f * scale,
                    WL::TEXT_TERTIARY,
                    2.0f * scale);
    return pressed;
}

// Metric tile — data readout cell
inline void draw_metric(Rectangle rect,
                        const char* label,
                        const std::string& value,
                        float scale = 1.0f) {
    DrawRectangleRounded(rect, 0.06f, 6, {6, 14, 24, 230});
    DrawRectangleRoundedLines(rect, 0.06f, 6, 1.0f, {28, 68, 92, 110});

    // Top accent line
    DrawLineEx({rect.x + 2, rect.y + 1}, {rect.x + rect.width * 0.4f, rect.y + 1},
               1.5f, {48, 180, 200, 100});

    const float label_size = 13.5f * scale;
    const float value_size = fit_ui_text_size(value, rect.width - 18.0f * scale, 20.0f * scale, 13.0f * scale);
    draw_text(label, {rect.x + 9.0f * scale, rect.y + 7.0f * scale}, label_size, WL::TEXT_TERTIARY);
    draw_text(value, {rect.x + 9.0f * scale, rect.y + 24.0f * scale}, value_size, WL::TEXT_PRIMARY);
}