#pragma once
// Descent-into-instances navigation for the Cosmos Explorer. Drives the
// deterministic procedural engine (cosmos::ProcUniverse): zooming into a child
// enters it (its children generate lazily), zooming out ascends. Supersedes the
// 9-tier archetype flow. Bounded + cached by the engine, so it stays smooth.

#include "ui/CosmosExplorerScene.hpp"

#include "raylib.h"

class Renderer;

namespace cosmos_ui {

// Build/reseed the procedural universe from the genome signature and seat the
// path at the root. Safe to call every frame.
void descent_ensure_init(CosmosState& cosmos);

// Per-frame input + camera + descend/ascend. `interactive` is false while a
// modal is open so navigation never fights an overlay.
void update_descent(CosmosState& cosmos, Rectangle stage, float dt, bool interactive);

// Rendering.
void draw_descent_stage(CosmosState& cosmos, Renderer& renderer, Rectangle stage, float ui_scale);
void draw_descent_breadcrumb(CosmosState& cosmos, Rectangle rect, float ui_scale);
void draw_descent_inspector(CosmosState& cosmos, Rectangle rect, float ui_scale);
void draw_descent_hud(const CosmosState& cosmos, Rectangle stage, float ui_scale);

// Path operations (also the unit of the navigation semantics).
void descent_push(CosmosState& cosmos, int child_index); // enter focus().children[i]
void descent_pop(CosmosState& cosmos);                    // ascend one level
void descent_jump_to_depth(CosmosState& cosmos, int depth);

} // namespace cosmos_ui
