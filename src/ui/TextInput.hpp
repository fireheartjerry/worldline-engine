#pragma once

#include "UiPrimitives.hpp"

#include <string>

inline void handle_text_input(std::string& value,
                              bool active,
                              bool& select_all,
                              float& backspace_repeat_timer,
                              std::size_t max_length,
                              bool allow_newline = false) {
    if (!active) {
        return;
    }

    const bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl_down && IsKeyPressed(KEY_A)) {
        select_all = true;
    }

    int ch = GetCharPressed();
    while (ch > 0) {
        const bool printable = (ch >= 32 && ch <= 126) || (allow_newline && ch == '\n');
        if (printable && value.size() < max_length) {
            if (select_all) {
                value.clear();
                select_all = false;
            }
            value.push_back(static_cast<char>(ch));
        }
        ch = GetCharPressed();
    }

    if (ctrl_down && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (select_all) {
            value.clear();
            select_all = false;
        }
        while (clipboard != nullptr && *clipboard != '\0' && value.size() < max_length) {
            const unsigned char next = static_cast<unsigned char>(*clipboard);
            if ((next >= 32u && next <= 126u) || (allow_newline && next == '\n')) {
                value.push_back(static_cast<char>(next));
            }
            ++clipboard;
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (select_all) {
            value.clear();
            select_all = false;
        } else if (!value.empty()) {
            value.pop_back();
        }
        backspace_repeat_timer = 0.0f;
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        backspace_repeat_timer += GetFrameTime();
        const float repeat_delay = 0.42f;
        const float repeat_step = 0.035f;
        while (backspace_repeat_timer >= repeat_delay) {
            if (select_all) {
                value.clear();
                select_all = false;
                backspace_repeat_timer = 0.0f;
                break;
            }
            if (!value.empty()) {
                value.pop_back();
            }
            backspace_repeat_timer -= repeat_step;
        }
    } else {
        backspace_repeat_timer = 0.0f;
    }
}

inline bool draw_text_field(Rectangle rect,
                            const std::string& value,
                            const std::string& placeholder,
                            bool active,
                            Color accent,
                            float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    const Color outline = active
        ? accent
        : hot ? with_alpha(accent, 170) : with_alpha(WL::GLASS_BORDER, 120);

    DrawRectangleRounded(rect, 0.10f, 8, {8, 16, 28, 240});
    DrawRectangleRoundedLines(rect, 0.10f, 8, 1.15f, outline);
    if (active) {
        DrawRectangleRounded({rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4},
                             0.10f, 8, with_alpha(accent, 14));
    }

    std::string text = value.empty() ? placeholder : value;
    if (active && (static_cast<int>(GetTime() * 2.0) % 2 == 0)) {
        text += "_";
    }
    draw_text(text,
              {rect.x + 10.0f * scale, rect.y + 9.0f * scale},
              16.0f * scale,
              value.empty() ? WL::TEXT_INACTIVE : WL::TEXT_PRIMARY);
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}
