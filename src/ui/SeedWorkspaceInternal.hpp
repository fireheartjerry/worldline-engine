#pragma once
// Seed Workspace — shared helpers + panel forward-declarations.
// Split across SeedWorkspaceScene.cpp (main scene) and
// SeedWorkspacePanels.cpp (glossary, timeline, stage, inspector, header).
#include "ui/SeedWorkspaceScene.hpp"
#include "app/SeededUniverseRuntime.hpp"

#include <cstdio>
#include <string>

namespace seed_ws {

inline std::string metric_text(double value, int precision = 2) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

inline std::string first_paragraph(const std::string& text) {
    const std::size_t split = text.find("\n\n");
    if (split == std::string::npos) {
        return text;
    }
    return text.substr(0, split);
}


// Panels — defined in SeedWorkspacePanels.cpp.
void draw_glossary_modal(bool& open, Rectangle viewport, float scale);
void draw_timeline(Rectangle rect, SeededUniverseUiState& seeded, float scale);
void draw_stage_overlay(Rectangle rect, const SeededUniverseUiState& seeded, const SeededUniverseRuntime* runtime, float scale);
void draw_inspector(AppState& app, Rectangle rect, SeededUniverseUiState& seeded, float scale, SeedWorkspaceSceneResult& result);
void draw_header(Rectangle rect, SeededUniverseUiState& seeded, float scale);

} // namespace seed_ws
