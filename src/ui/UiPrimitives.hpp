#pragma once
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

// ── Worldline Alien-Tech Palette ─────────────────────────────────────────────
// Deep-void blacks, bioluminescent cyan/violet, hot xenon orange accents.
// The whole instrument is built from layered frosted glass floating over a
// living starfield — premium depth, restrained accent colour, crisp hairlines.

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

    // Surface glass — translucent so the starfield bleeds through every panel
    constexpr Color GLASS_1        = { 10,  20,  34, 196};  // card fill dark
    constexpr Color GLASS_2        = { 13,  25,  42, 182};  // card fill mid
    constexpr Color GLASS_3        = { 18,  32,  52, 168};  // raised / hover
    constexpr Color GLASS_BORDER   = { 92, 158, 192, 110};  // dim frost hairline
    constexpr Color GLASS_HILIGHT  = {200, 236, 250,  70};  // top-rim light catch

    // Text hierarchy
    constexpr Color TEXT_PRIMARY   = {231, 244, 250, 255};
    constexpr Color TEXT_SECONDARY = {150, 196, 216, 224};
    constexpr Color TEXT_TERTIARY  = { 96, 148, 174, 192};
    constexpr Color TEXT_INACTIVE  = { 60, 100, 122, 150};

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

    // Prefer a narrow, technical-looking font on every platform — fits the
    // alien-terminal aesthetic.  Linux/macOS paths included so the instrument
    // renders with proper type outside Windows, not the raylib bitmap fallback.
    const std::array<const char*, 14> candidates{{
        // Windows
        "C:/Windows/Fonts/consola.ttf",   // Consolas — monospace, sharp, techy
        "C:/Windows/Fonts/cour.ttf",      // Courier New fallback
        "C:/Windows/Fonts/segoeui.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        // macOS
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
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

// ── Time / motion helpers ───────────────────────────────────────────────────────
// Used sparingly for "alive" HUD cues (live badges, scan sweeps).  Restraint is
// deliberate — motion is a seasoning, not the meal.
inline float ui_time() { return static_cast<float>(GetTime()); }

inline float ui_pulse(float speed, float lo = 0.0f, float hi = 1.0f) {
    const float t = 0.5f + 0.5f * std::sin(ui_time() * speed);
    return lo + (hi - lo) * t;
}

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

inline unsigned char clamp_u8(float v) {
    return static_cast<unsigned char>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
}

// Linear blend between two colors (alpha included)
inline Color lerp_color(Color a, Color b, float t) {
    return {
        clamp_u8(a.r + (b.r - a.r) * t),
        clamp_u8(a.g + (b.g - a.g) * t),
        clamp_u8(a.b + (b.b - a.b) * t),
        clamp_u8(a.a + (b.a - a.a) * t)
    };
}

inline Color lighten(Color c, float amt) { Color w{255, 255, 255, c.a}; return lerp_color(c, w, amt); }
inline Color darken(Color c, float amt)  { Color k{0, 0, 0, c.a};       return lerp_color(c, k, amt); }

// Additive glow tint — mix color toward white at given strength
inline Color glow_tint(Color c, float strength) {
    return {
        clamp_u8(c.r + (255 - c.r) * strength),
        clamp_u8(c.g + (255 - c.g) * strength),
        clamp_u8(c.b + (255 - c.b) * strength),
        c.a
    };
}

// ── Background ────────────────────────────────────────────────────────────────
// Deep-void starfield with layered nebula blooms, a deterministic twinkling
// star layer, a cinematic vignette, and a barely-there scanline grain.  This is
// the single surface every glass panel floats over, so its translucency reads.
inline void draw_background(int screen_width, int screen_height) {
    const float W = static_cast<float>(screen_width);
    const float H = static_cast<float>(screen_height);
    const Rectangle frame = {0.0f, 0.0f, W, H};

    // Base void gradient — a touch of lift toward the lower band gives depth.
    DrawRectangleGradientEx(frame,
        { 3,  6, 14, 255},   // TL
        { 2,  5, 12, 255},   // TR
        { 6, 11, 21, 255},   // BR
        { 4,  8, 17, 255});  // BL

    // Layered nebula blooms — soft, low-alpha, anchored off the focal centre.
    DrawCircleGradient(static_cast<int>(W * 0.14f), static_cast<int>(H * 0.16f),
                       H * 0.62f, { 12, 78, 100, 58}, {0, 0, 0, 0});   // cyan, upper-left
    DrawCircleGradient(static_cast<int>(W * 0.87f), static_cast<int>(H * 0.20f),
                       H * 0.46f, { 56, 22, 104, 46}, {0, 0, 0, 0});   // violet, upper-right
    DrawCircleGradient(static_cast<int>(W * 0.83f), static_cast<int>(H * 0.86f),
                       H * 0.40f, {128, 56, 14, 40}, {0, 0, 0, 0});    // xenon, lower-right
    DrawCircleGradient(static_cast<int>(W * 0.30f), static_cast<int>(H * 0.92f),
                       H * 0.34f, { 10, 60, 82, 30}, {0, 0, 0, 0});    // cyan, lower-left

    // Deterministic starfield with gentle twinkle.  Positions are regenerated
    // each frame from a fixed LCG seed (cheap, ~160 dots) so nothing persists.
    const float t = ui_time();
    unsigned int rng = 0x9E3779B9u;
    auto nextf = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>((rng >> 8) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
    };
    for (int i = 0; i < 160; ++i) {
        const float x    = nextf() * W;
        const float y    = nextf() * H;
        const float seed = nextf();
        const float size = seed > 0.94f ? 1.7f : (seed > 0.74f ? 1.1f : 0.7f);
        const float tw   = 0.55f + 0.45f * std::sin(t * (0.5f + seed * 1.4f) + static_cast<float>(i) * 1.7f);
        const unsigned char a = clamp_u8(34.0f + seed * 150.0f * tw);
        const Color sc = seed > 0.86f ? Color{188, 232, 246, a} : Color{150, 186, 212, a};
        DrawCircleV({x, y}, size, sc);
        if (size > 1.5f) DrawCircleV({x, y}, size + 1.4f, with_alpha(sc, a / 5));  // faint halo on bright stars
    }

    // Cinematic vignette — darken the frame edges to push focus inward.
    DrawRectangleGradientV(0, 0, static_cast<int>(W), static_cast<int>(H * 0.20f),
                           {0, 0, 0, 96}, {0, 0, 0, 0});
    DrawRectangleGradientV(0, static_cast<int>(H * 0.78f), static_cast<int>(W), static_cast<int>(H * 0.22f),
                           {0, 0, 0, 0}, {0, 0, 0, 120});
    DrawRectangleGradientH(0, 0, static_cast<int>(W * 0.13f), static_cast<int>(H),
                           {0, 0, 0, 90}, {0, 0, 0, 0});
    DrawRectangleGradientH(static_cast<int>(W * 0.87f), 0, static_cast<int>(W * 0.13f), static_cast<int>(H),
                           {0, 0, 0, 0}, {0, 0, 0, 90});

    // Barely-there scanline grain — texture without distraction.
    for (int y = 0; y < screen_height; y += 3) {
        DrawLine(0, y, screen_width, y, {255, 255, 255, 2});
    }
}

// ── Frosted-glass core ──────────────────────────────────────────────────────────
// Every panel in the app is built from this one routine so depth, light, and
// material read identically everywhere.  Layers, bottom to top:
//   1. soft drop shadow (elevation)         5. top-rim light catch
//   2. translucent frosted fill             6. bottom inner shade
//   3. single soft top sheen band           7. accent base underglow
//   4. crisp accent hairline border
inline void draw_glass_panel(Rectangle r, Color fill, Color accent,
                             float roundness = 0.06f, int elevation = 3) {
    const int   seg   = 10;
    const float cr    = roundness * std::min(r.width, r.height);
    const float inset = std::max(4.0f, cr + 2.0f);

    // 1) Drop shadow — stacked, fading, offset downward for grounded elevation.
    for (int i = elevation; i >= 1; --i) {
        const float o = static_cast<float>(i);
        DrawRectangleRounded({r.x - o, r.y + o * 0.55f + 1.5f, r.width + o * 2.0f, r.height + o * 2.0f},
                             roundness, seg, {0, 0, 0, clamp_u8(13.0f - i * 1.6f)});
    }

    // 2) Frosted translucent fill — the starfield bleeds through this.
    DrawRectangleRounded(r, roundness, seg, fill);

    // 3) Soft top sheen — one inset band, low alpha, simulates light pooling on
    //    the upper face of the glass.  Kept subtle to avoid a hard seam.
    const Color sheen = with_alpha(lighten(fill, 0.30f), 24);
    DrawRectangleRounded({r.x + 2.0f, r.y + 2.0f, r.width - 4.0f, r.height * 0.42f},
                         std::min(0.5f, roundness * 1.5f), seg, sheen);

    // 4) Crisp hairline border — the defining edge of the pane.
    DrawRectangleRoundedLines(r, roundness, seg, 1.2f, with_alpha(accent, 150));

    // 5) Top-rim light catch — the brightest line, just inside the upper edge.
    DrawLineEx({r.x + inset, r.y + 1.6f}, {r.x + r.width - inset, r.y + 1.6f},
               1.0f, WL::GLASS_HILIGHT);

    // 6) Bottom inner shade — grounds the lower edge.
    DrawLineEx({r.x + inset, r.y + r.height - 1.6f}, {r.x + r.width - inset, r.y + r.height - 1.6f},
               1.0f, {0, 0, 0, 70});

    // 7) Accent base underglow — a powered hairline along the floor of the pane.
    DrawLineEx({r.x + inset * 1.3f, r.y + r.height - 2.6f},
               {r.x + r.width - inset * 1.3f, r.y + r.height - 2.6f},
               1.0f, with_alpha(accent, 58));
}

// ── Sci-fi corner brackets ───────────────────────────────────────────────────────
// L-shaped framing marks for HUD surfaces.  Reads as engineered targeting
// reticles without the clutter of a full border treatment.
inline void draw_corner_brackets(Rectangle r, Color accent,
                                  float arm = 13.0f, float thick = 1.6f, float inset = 5.0f) {
    const float x0 = r.x + inset, y0 = r.y + inset;
    const float x1 = r.x + r.width - inset, y1 = r.y + r.height - inset;
    const Color c = with_alpha(accent, 200);
    // Top-left
    DrawLineEx({x0, y0}, {x0 + arm, y0}, thick, c);
    DrawLineEx({x0, y0}, {x0, y0 + arm}, thick, c);
    // Top-right
    DrawLineEx({x1, y0}, {x1 - arm, y0}, thick, c);
    DrawLineEx({x1, y0}, {x1, y0 + arm}, thick, c);
    // Bottom-left
    DrawLineEx({x0, y1}, {x0 + arm, y1}, thick, c);
    DrawLineEx({x0, y1}, {x0, y1 - arm}, thick, c);
    // Bottom-right
    DrawLineEx({x1, y1}, {x1 - arm, y1}, thick, c);
    DrawLineEx({x1, y1}, {x1, y1 - arm}, thick, c);
}

// A short row of tick marks — engineered detailing for headers / dividers.
inline void draw_tick_strip(Vector2 origin, int count, float gap, float height, Color accent) {
    for (int i = 0; i < count; ++i) {
        const float h = (i % 4 == 0) ? height : height * 0.55f;
        const unsigned char a = (i % 4 == 0) ? 170 : 90;
        DrawLineEx({origin.x + i * gap, origin.y}, {origin.x + i * gap, origin.y + h},
                   1.0f, with_alpha(accent, a));
    }
}

// ── Core drawing primitives ────────────────────────────────────────────────────

// Card: frosted glass pane with a crisp accent hairline.  Backwards compatible —
// `fill` is the translucent tint, `outline` is the accent edge.
inline void draw_card(Rectangle rect, Color fill, Color outline) {
    draw_glass_panel(rect, fill, outline, 0.055f, 3);
}

// Accentuated card with a glowing colored left-edge stripe.
inline void draw_card_accented(Rectangle rect, Color fill, Color border, Color stripe) {
    draw_glass_panel(rect, fill, border, 0.055f, 3);
    // Left stripe — soft glow halo, then a crisp 2px bar.
    DrawRectangle(static_cast<int>(rect.x),
                  static_cast<int>(rect.y + 4),
                  4,
                  static_cast<int>(rect.height - 8),
                  with_alpha(stripe, 50));
    DrawRectangle(static_cast<int>(rect.x + 1),
                  static_cast<int>(rect.y + 4),
                  2,
                  static_cast<int>(rect.height - 8),
                  with_alpha(stripe, 210));
}

inline void draw_text(const std::string& text, Vector2 position, float size, Color color, float spacing = -1.0f) {
    const float sp = (spacing >= 0.0f) ? spacing : ui_text_spacing(size);
    DrawTextEx(ui_font(), text.c_str(), position, size, sp, color);
}

// Badge: frosted pill with a soft glow halo and glass top sheen.
inline void draw_badge(Rectangle rect, const char* label, Color fill, Color text_color, float scale = 1.0f) {
    // Glow halo
    DrawRectangleRounded({rect.x - 1.5f, rect.y - 1.5f, rect.width + 3.0f, rect.height + 3.0f},
                         0.50f, 12, with_alpha(text_color, 26));
    DrawRectangleRounded(rect, 0.50f, 12, fill);
    // Top sheen
    DrawRectangleRounded({rect.x + 2.0f, rect.y + 1.5f, rect.width - 4.0f, rect.height * 0.46f},
                         0.50f, 12, with_alpha(lighten(fill, 0.35f), 30));
    DrawRectangleRoundedLines(rect, 0.50f, 12, 1.0f, with_alpha(text_color, 95));

    const float th = fit_ui_text_size(label, rect.width - 10.0f * scale, 14.0f * scale, 10.0f * scale);
    const Vector2 ts = measure_ui_text(label, th);
    draw_text(label, {rect.x + (rect.width - ts.x) * 0.5f, rect.y + (rect.height - ts.y) * 0.5f}, th, text_color);
}

// Button: frosted glass key with hover lift, pressed inset, and an accent
// base bar that lights on hover.  Sharp-ish corners keep the sci-fi register.
inline bool draw_button(Rectangle rect,
                        const char* label,
                        Color base,
                        Color hover,
                        Color text_color,
                        bool enabled = true,
                        float scale = 1.0f) {
    const Vector2 mouse = GetMousePosition();
    const bool hot     = enabled && CheckCollisionPointRec(mouse, rect);
    const bool pressed_now = hot && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const bool clicked = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    const float roundness = 0.16f;
    const int   seg = 8;
    const Color fill = enabled ? (hot ? hover : base) : with_alpha(base, 60);
    const Color text = enabled ? text_color : with_alpha(text_color, 90);

    // Hover outer glow halo
    if (hot && enabled) {
        DrawRectangleRounded({rect.x - 2.5f, rect.y - 2.5f, rect.width + 5.0f, rect.height + 5.0f},
                             roundness, seg, with_alpha(hover, 36));
    }
    // Grounding shadow
    DrawRectangleRounded({rect.x, rect.y + 2.0f, rect.width, rect.height}, roundness, seg, {0, 0, 0, 55});

    // Body fill (pressed darkens slightly)
    DrawRectangleRounded(rect, roundness, seg, pressed_now ? darken(fill, 0.12f) : fill);
    // Top sheen
    DrawRectangleRounded({rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height * 0.50f},
                         roundness * 1.2f, seg, with_alpha(lighten(fill, 0.26f), hot ? 42 : 26));
    // Hairline border
    DrawRectangleRoundedLines(rect, roundness, seg, 1.2f,
                              with_alpha(text_color, enabled ? (hot ? 150 : 72) : 28));
    // Top-rim light catch
    if (enabled) {
        DrawLineEx({rect.x + 5.0f, rect.y + 1.6f}, {rect.x + rect.width - 5.0f, rect.y + 1.6f},
                   1.0f, with_alpha(WL::GLASS_HILIGHT, hot ? 90 : 55));
    }
    // Accent base bar — lights up on hover
    if (hot && enabled) {
        DrawLineEx({rect.x + 8.0f, rect.y + rect.height - 2.0f},
                   {rect.x + rect.width - 8.0f, rect.y + rect.height - 2.0f},
                   1.6f, with_alpha(text_color, 130));
    }

    const float th = fit_ui_text_size(label, rect.width - 16.0f * scale, 19.0f * scale, 12.0f * scale);
    const Vector2 ts = measure_ui_text(label, th);
    const float ny = rect.y + (rect.height - ts.y) * 0.5f + (pressed_now ? 1.0f : 0.0f);
    draw_text(label, {rect.x + (rect.width - ts.x) * 0.5f, ny}, th, text);
    return clicked;
}

// Checkbox: alien-panel toggle with a glowing indicator diamond.
inline bool draw_checkbox(Rectangle rect,
                          bool checked,
                          const char* label,
                          const char* note,
                          Color accent,
                          float scale = 1.0f) {
    const Vector2 mouse = GetMousePosition();
    const bool hot     = CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    const Color fill   = hot ? WL::GLASS_3 : WL::GLASS_1;
    const Color border = hot ? with_alpha(accent, 130) : WL::GLASS_BORDER;
    draw_glass_panel(rect, fill, border, 0.08f, hot ? 3 : 2);

    // Indicator diamond — more alien than a plain square
    const float bx = rect.x + 16.0f * scale;
    const float by = rect.y + rect.height * 0.5f;
    const float bs = 8.0f * scale;
    if (checked) {
        const Vector2 pts[4] = {
            {bx,      by - bs},
            {bx + bs, by},
            {bx,      by + bs},
            {bx - bs, by}
        };
        // Glow ring then filled glowing diamond
        DrawCircleV({bx, by}, bs + 4.0f * scale, with_alpha(accent, 40));
        DrawTriangle(pts[0], pts[3], pts[2], with_alpha(accent, 220));
        DrawTriangle(pts[0], pts[2], pts[1], with_alpha(accent, 220));
        DrawCircleLines(static_cast<int>(bx), static_cast<int>(by),
                        bs + 2.0f * scale, with_alpha(accent, 90));
    } else {
        DrawLine(static_cast<int>(bx),      static_cast<int>(by - bs),
                 static_cast<int>(bx + bs), static_cast<int>(by),
                 with_alpha(accent, 100));
        DrawLine(static_cast<int>(bx + bs), static_cast<int>(by),
                 static_cast<int>(bx),      static_cast<int>(by + bs),
                 with_alpha(accent, 100));
        DrawLine(static_cast<int>(bx),      static_cast<int>(by + bs),
                 static_cast<int>(bx - bs), static_cast<int>(by),
                 with_alpha(accent, 100));
        DrawLine(static_cast<int>(bx - bs), static_cast<int>(by),
                 static_cast<int>(bx),      static_cast<int>(by - bs),
                 with_alpha(accent, 100));
    }

    const float lx = rect.x + 34.0f * scale;
    const float label_size = fit_ui_text_size(label, rect.width - 50.0f * scale, 18.0f * scale, 13.0f * scale);
    draw_text(label, {lx, rect.y + 8.0f * scale}, label_size, WL::TEXT_PRIMARY);
    draw_text_block(note,
                    {lx, rect.y + 26.0f * scale, rect.width - 44.0f * scale, rect.height - 30.0f * scale},
                    13.5f * scale,
                    WL::TEXT_TERTIARY,
                    2.0f * scale);
    return pressed;
}

// Metric tile — frosted data-readout cell with an accent ledger line.
inline void draw_metric(Rectangle rect,
                        const char* label,
                        const std::string& value,
                        float scale = 1.0f) {
    draw_glass_panel(rect, WL::GLASS_1, WL::GLASS_BORDER, 0.08f, 2);

    // Top accent ledger line
    DrawLineEx({rect.x + 3.0f, rect.y + 2.0f}, {rect.x + rect.width * 0.42f, rect.y + 2.0f},
               1.5f, with_alpha(WL::CYAN_CORE, 130));

    const float label_size = 13.5f * scale;
    const float value_size = fit_ui_text_size(value, rect.width - 18.0f * scale, 20.0f * scale, 13.0f * scale);
    draw_text(label, {rect.x + 10.0f * scale, rect.y + 7.0f * scale}, label_size, WL::TEXT_TERTIARY);
    draw_text(value, {rect.x + 10.0f * scale, rect.y + 24.0f * scale}, value_size, WL::TEXT_PRIMARY);
}

inline void draw_scrollbar(Rectangle viewport, float scroll, float max_scroll) {
    if (max_scroll <= 0.0f) return;
    const float track_w = 5.0f;
    const float track_x = viewport.x + viewport.width - track_w - 3.0f;
    const float track_y = viewport.y + 3.0f;
    const float track_h = viewport.height - 6.0f;
    const float thumb_h = std::max(28.0f, track_h * (viewport.height / (viewport.height + max_scroll)));
    const float thumb_y = track_y + (track_h - thumb_h) * (scroll / max_scroll);
    DrawRectangleRounded({track_x, track_y, track_w, track_h}, 0.5f, 8, {255, 255, 255, 10});
    DrawRectangleRounded({track_x, thumb_y, track_w, thumb_h}, 0.5f, 8, with_alpha(WL::CYAN_CORE, 120));
    DrawRectangleRoundedLines({track_x, thumb_y, track_w, thumb_h}, 0.5f, 8, 1.0f, with_alpha(WL::CYAN_CORE, 60));
}

inline bool draw_back_to_menu_button(Rectangle viewport, float scale = 1.0f) {
    return draw_button(
        {viewport.x + 20.0f * scale, viewport.y + 20.0f * scale, 160.0f * scale, 36.0f * scale},
        "< Back",
        {14, 28, 46, 228},
        {22, 44, 70, 255},
        WL::TEXT_PRIMARY,
        true,
        scale);
}
