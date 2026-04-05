#pragma once
#include "raylib.h"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

struct UiFontState {
    Font regular{};
    bool loaded = false;
};

inline UiFontState& ui_font_state() {
    static UiFontState state{GetFontDefault(), false};
    return state;
}

inline float ui_text_spacing(float size) {
    return std::max(0.0f, size * 0.02f);
}

inline void init_ui_font() {
    UiFontState& state = ui_font_state();
    if (state.loaded) {
        return;
    }

    const std::array<const char*, 5> candidates{{
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/seguiemj.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        "C:/Windows/Fonts/arial.ttf",
    }};

    for (const char* path : candidates) {
        if (!FileExists(path)) {
            continue;
        }

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

inline const Font& ui_font() {
    return ui_font_state().regular;
}

inline Vector2 measure_ui_text(const std::string& text,
                               float size,
                               float spacing = -1.0f) {
    const float resolved_spacing = (spacing >= 0.0f) ? spacing : ui_text_spacing(size);
    return MeasureTextEx(ui_font(), text.c_str(), size, resolved_spacing);
}

inline float fit_ui_text_size(const std::string& text,
                              float max_width,
                              float preferred_size,
                              float min_size) {
    float size = preferred_size;
    while (size > min_size && measure_ui_text(text, size).x > max_width) {
        size -= 0.5f;
    }
    return size;
}

inline std::vector<std::string> wrap_ui_text(const std::string& text,
                                             float max_width,
                                             float size) {
    std::vector<std::string> lines;
    if (text.empty() || max_width <= 0.0f) {
        lines.push_back(text);
        return lines;
    }

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
                while (word_start < paragraph.size() && paragraph[word_start] == ' ') {
                    ++word_start;
                }
                if (word_start >= paragraph.size()) {
                    break;
                }
                std::size_t word_end = paragraph.find(' ', word_start);
                const std::string word = paragraph.substr(
                    word_start,
                    word_end == std::string::npos ? std::string::npos : word_end - word_start);
                const std::string candidate = current.empty() ? word : current + " " + word;
                if (!current.empty() && measure_ui_text(candidate, size).x > max_width) {
                    lines.push_back(current);
                    current = word;
                } else {
                    current = candidate;
                }
                if (word_end == std::string::npos) {
                    break;
                }
                word_start = word_end + 1;
            }
            if (!current.empty()) {
                lines.push_back(current);
            }
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    if (lines.empty()) {
        lines.push_back(text);
    }
    return lines;
}

inline float measure_wrapped_ui_text_height(const std::string& text,
                                            float max_width,
                                            float size,
                                            float line_gap = 4.0f) {
    const std::vector<std::string> lines = wrap_ui_text(text, max_width, size);
    if (lines.empty()) {
        return 0.0f;
    }
    return lines.size() * size + (lines.size() - 1) * line_gap;
}

inline void draw_text_block(const std::string& text,
                            Rectangle bounds,
                            float size,
                            Color color,
                            float line_gap = 4.0f) {
    const std::vector<std::string> lines = wrap_ui_text(text, bounds.width, size);
    float y = bounds.y;
    for (const std::string& line : lines) {
        DrawTextEx(ui_font(), line.c_str(), {bounds.x, y}, size, ui_text_spacing(size), color);
        y += size + line_gap;
        if (y > bounds.y + bounds.height) {
            break;
        }
    }
}

inline Color with_alpha(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}

inline void draw_background(int screen_width, int screen_height) {
    const Rectangle frame = {0.0f, 0.0f, static_cast<float>(screen_width), static_cast<float>(screen_height)};
    DrawRectangleGradientEx(frame, {4, 10, 18, 255}, {10, 18, 28, 255}, {17, 34, 41, 255}, {6, 11, 18, 255});
    DrawCircleGradient(static_cast<int>(screen_width * 0.14f), static_cast<int>(screen_height * 0.20f), screen_height * 0.45f, {18, 90, 98, 88}, {0, 0, 0, 0});
    DrawCircleGradient(static_cast<int>(screen_width * 0.84f), static_cast<int>(screen_height * 0.76f), screen_height * 0.36f, {170, 106, 42, 62}, {0, 0, 0, 0});
}

inline void draw_card(Rectangle rect, Color fill, Color outline) {
    DrawRectangleRounded(rect, 0.05f, 16, fill);
    DrawRectangleRoundedLines(rect, 0.05f, 16, 1.5f, outline);
}

inline void draw_text(const std::string& text,
                      Vector2 position,
                      float size,
                      Color color,
                      float spacing = -1.0f) {
    const float resolved_spacing = (spacing >= 0.0f) ? spacing : ui_text_spacing(size);
    DrawTextEx(ui_font(), text.c_str(), position, size, resolved_spacing, color);
}

inline void draw_badge(Rectangle rect,
                       const char* label,
                       Color fill,
                       Color text_color,
                       float scale = 1.0f) {
    DrawRectangleRounded(rect, 0.35f, 12, fill);
    const float text_height = fit_ui_text_size(label,
                                               rect.width - 12.0f * scale,
                                               16.0f * scale,
                                               11.5f * scale);
    const Vector2 text_size = measure_ui_text(label, text_height);
    draw_text(label,
              {rect.x + (rect.width - text_size.x) * 0.5f,
               rect.y + (rect.height - text_size.y) * 0.5f},
              text_height,
              text_color);
}

inline bool draw_button(Rectangle rect,
                        const char* label,
                        Color base,
                        Color hover,
                        Color text_color,
                        bool enabled = true,
                        float scale = 1.0f) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = enabled && CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    Color fill = enabled ? (hot ? hover : base) : with_alpha(base, 90);
    Color text = enabled ? text_color : with_alpha(text_color, 110);

    DrawRectangleRounded(rect, 0.24f, 12, fill);
    DrawRectangleRoundedLines(rect, 0.24f, 12, 1.4f, with_alpha(text_color, enabled ? 80 : 40));

    const float text_height = fit_ui_text_size(label,
                                               rect.width - 18.0f * scale,
                                               20.0f * scale,
                                               13.5f * scale);
    const Vector2 text_size = measure_ui_text(label, text_height);
    draw_text(label,
              {rect.x + (rect.width - text_size.x) * 0.5f,
               rect.y + (rect.height - text_size.y) * 0.5f},
              text_height,
              text);
    return pressed;
}

inline bool draw_checkbox(Rectangle rect,
                          bool checked,
                          const char* label,
                          const char* note,
                          Color accent,
                          float scale = 1.0f) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    const bool pressed = hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawRectangleRounded(rect, 0.16f, 10, hot ? Color{11, 20, 28, 245} : Color{9, 17, 24, 230});
    DrawRectangleRoundedLines(rect, 0.16f, 10, 1.2f, hot ? with_alpha(accent, 140) : Color{43, 74, 88, 110});

    const Rectangle box = {rect.x + 12.0f * scale, rect.y + 13.0f * scale, 18.0f * scale, 18.0f * scale};
    DrawRectangleRounded(box, 0.25f, 8, checked ? accent : Color{14, 24, 33, 255});
    DrawRectangleRoundedLines(box, 0.25f, 8, 1.0f, checked ? accent : Color{71, 106, 120, 165});

    if (checked) {
        DrawLineEx({box.x + 4.0f * scale, box.y + 9.5f * scale},
                   {box.x + 8.0f * scale, box.y + 14.0f * scale},
                   2.0f * scale,
                   {8, 18, 24, 255});
        DrawLineEx({box.x + 8.0f * scale, box.y + 14.0f * scale},
                   {box.x + 15.0f * scale, box.y + 5.0f * scale},
                   2.0f * scale,
                   {8, 18, 24, 255});
    }

    const float label_size = fit_ui_text_size(label,
                                              rect.width - 54.0f * scale,
                                              19.0f * scale,
                                              15.0f * scale);
    const float note_size = 14.0f * scale;
    draw_text(label,
              {rect.x + 40.0f * scale, rect.y + 9.0f * scale},
              label_size,
              {233, 241, 247, 255});
    draw_text_block(note,
                    {rect.x + 40.0f * scale,
                     rect.y + 28.0f * scale,
                     rect.width - 52.0f * scale,
                     rect.height - 30.0f * scale},
                    note_size,
                    {132, 175, 193, 210},
                    2.0f * scale);
    return pressed;
}

inline void draw_metric(Rectangle rect,
                        const char* label,
                        const std::string& value,
                        float scale = 1.0f) {
    DrawRectangleRounded(rect, 0.16f, 10, {9, 16, 22, 225});
    DrawRectangleRoundedLines(rect, 0.16f, 10, 1.0f, {48, 81, 96, 140});
    const float label_size = 15.0f * scale;
    const float value_size = fit_ui_text_size(value, rect.width - 20.0f * scale, 21.0f * scale, 14.0f * scale);
    draw_text(label,
              {rect.x + 10.0f * scale, rect.y + 8.0f * scale},
              label_size,
              {114, 162, 182, 220});
    draw_text(value,
              {rect.x + 10.0f * scale, rect.y + 26.0f * scale},
              value_size,
              {234, 242, 247, 255});
}
