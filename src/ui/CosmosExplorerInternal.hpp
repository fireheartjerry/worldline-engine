#pragma once
// Shared helpers and panel forward-declarations for the Cosmos Explorer scene.
// The scene is split across two translation units:
//   CosmosExplorerScene.cpp   — CosmosState methods, stepping, main scene
//   CosmosExplorerPanels.cpp  — ladder, inspector, observables, modals, backdrop
// Small pure helpers live here (inline) so both units share them.
#include "ui/CosmosExplorerScene.hpp"
#include "ui/UiPrimitives.hpp"
#include "app/UniverseModel.hpp"
#include "cosmos/Constants.hpp"
#include "cosmos/ObjectCatalog.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace cosmos_ui {
using namespace cosmos;

constexpr double kWorldHalf = 12.0;

inline Color palette_color(Color8 c, unsigned char a = 255) {
    return Color{c.r, c.g, c.b, a};
}

inline Color to_raylib(Color8 c) {
    return Color{c.r, c.g, c.b, 255};
}

inline std::string fmt_sci(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.2e", v);
    return buf;
}

inline std::string fmt_fixed(double v, int precision) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, v);
    return buf;
}

inline bool clicked(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

inline bool right_clicked(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
}

// Jump the library to a specific object by id (constituent navigation),
// switching tiers if needed.
inline void select_object_by_id(CosmosState& cosmos, const std::string& id) {
    const UniverseObject* target = find_object(cosmos.catalog, id);
    if (target == nullptr) {
        return;
    }
    cosmos.set_scale(target->scale);
    const auto objs = objects_for_scale(cosmos.catalog, target->scale);
    for (std::size_t i = 0; i < objs.size(); ++i) {
        if (objs[i]->id == id) {
            cosmos.selected_object = static_cast<int>(i);
            break;
        }
    }
}

// Panels / modals — defined in CosmosExplorerPanels.cpp.
void draw_universe_backdrop(const UniversePalette& pal, std::uint64_t sig, Rectangle rect, float t);
void draw_ladder(CosmosState& cosmos, Rectangle rect, float scale);
void draw_inspector(const CosmosState& cosmos, Rectangle rect, float scale);
void draw_observables(const CosmosState& cosmos, Rectangle rect, float scale);
void draw_browser_modal(AppState& app, CosmosState& cosmos, Rectangle viewport, float scale);
void draw_dossier_modal(CosmosState& cosmos, Rectangle viewport, float scale);
void save_current_sandbox(const CosmosState& cosmos);
void restore_bookmark(AppState& app, CosmosState& cosmos, const CosmosBookmark& b);

} // namespace cosmos_ui
