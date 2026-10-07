#pragma once

#include "UiPrimitives.hpp"

#include <algorithm>
#include <string>
#include <vector>

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

// Single-line text field.  `select_all` paints the "typing replaces
// everything" state that handle_text_input() implements.  Long values never
// spill out of the field: while editing the tail (where the caret is) stays
// visible behind a leading "...", otherwise the head is shown with a trailing
// "...".  Returns true on the frame the field is clicked.
inline bool draw_text_field(Rectangle rect,
                            const std::string& value,
                            const std::string& placeholder,
                            bool active,
                            bool select_all,
                            Color accent,
                            float scale) {
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, rect);
    if (hot) ui_request_cursor(MOUSE_CURSOR_IBEAM);
    const Color outline = active
        ? accent
        : hot ? with_alpha(accent, 170) : with_alpha(WL::GLASS_BORDER, 120);

    if (active) {
        DrawRectangleRounded({rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4},
                             0.10f, 8, with_alpha(accent, 22));
    }
    DrawRectangleRounded(rect, 0.10f, 8, hot && !active ? Color{10, 20, 34, 244} : Color{8, 16, 28, 240});
    DrawRectangleRoundedLines(rect, 0.10f, 8, active ? 1.4f : 1.15f, outline);

    const float size  = std::min(16.0f * scale, rect.height * 0.5f);
    const float pad   = 10.0f * scale;
    const float caret_w = 2.0f * scale;
    const float avail = std::max(0.0f, rect.width - pad * 2.0f - caret_w - 2.0f * scale);
    const float ty    = rect.y + (rect.height - size) * 0.5f;

    if (value.empty()) {
        draw_text(ellipsize_ui_text(placeholder, avail, size), {rect.x + pad, ty}, size, WL::TEXT_INACTIVE);
    }

    std::string shown = value;
    if (!value.empty() && measure_ui_text(value, size).x > avail) {
        if (active) {
            // Keep the end (and the caret) in view: drop the fewest leading
            // codepoints so "..." + tail fits.  Binary search over codepoint
            // starts; suffix width shrinks monotonically as the start moves.
            static const char kLead[] = "...";
            const float sp = ui_text_spacing(size);
            const float lead_w = ui_text_range_width(kLead, kLead + 3, size, sp) + sp;
            const char* s = value.c_str();
            const char* end = s + value.size();
            std::vector<std::size_t> starts;
            for (std::size_t i = 1; i < value.size(); ++i) {
                if ((static_cast<unsigned char>(value[i]) & 0xC0u) != 0x80u) starts.push_back(i);
            }
            std::size_t lo = 0, hi = starts.size();   // first index whose tail fits
            while (lo < hi) {
                const std::size_t mid = (lo + hi) / 2;
                if (lead_w + ui_text_range_width(s + starts[mid], end, size, sp) <= avail) hi = mid;
                else lo = mid + 1;
            }
            shown = std::string(kLead) + (lo < starts.size() ? value.substr(starts[lo]) : std::string());
        } else {
            shown = ellipsize_ui_text(value, avail, size);
        }
    }

    const float text_w = shown.empty() ? 0.0f : measure_ui_text(shown, size).x;
    if (active && select_all && !value.empty()) {
        DrawRectangleRounded({rect.x + pad - 3.0f * scale, ty - 2.0f * scale, text_w + 6.0f * scale, size + 4.0f * scale},
                             0.25f, 6, with_alpha(accent, 70));
    }
    if (!shown.empty()) {
        draw_text(shown, {rect.x + pad, ty}, size, WL::TEXT_PRIMARY);
    }
    if (active && (static_cast<int>(GetTime() * 2.0) % 2 == 0)) {
        const float cx = rect.x + pad + text_w + (shown.empty() ? 0.0f : 2.0f * scale);
        DrawRectangleRec({cx, ty - 1.0f * scale, caret_w, size + 2.0f * scale}, with_alpha(accent, 230));
    }
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

inline bool draw_text_field(Rectangle rect,
                            const std::string& value,
                            const std::string& placeholder,
                            bool active,
                            Color accent,
                            float scale) {
    return draw_text_field(rect, value, placeholder, active, false, accent, scale);
}
