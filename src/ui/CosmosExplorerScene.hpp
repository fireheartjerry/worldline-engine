#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"
#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/ScaleLadder.hpp"

#include <string>
#include <vector>

class Renderer;

// All state for the Cosmos Explorer: the universe's law genome, the object
// catalog specialized to it, the currently selected scale tier, and a live
// N-body sandbox. Owned by main() and passed to the scene each frame.
struct CosmosState {
    bool initialized = false;
    std::string seed;
    cosmos::LawGenome genome;
    std::vector<cosmos::UniverseObject> catalog;

    cosmos::Scale scale = cosmos::Scale::STELLAR;
    int selected_object = 0; // index within the current tier's object list

    cosmos::NBodySystem system;
    bool has_sim = false;
    bool running = false;
    double elapsed = 0.0;

    // Configure (or reconfigure) for a seed: build the genome + specialized
    // catalog. Cheap and deterministic.
    void configure(const std::string& seed_text);
    void set_scale(cosmos::Scale next);
    // Populate the sandbox with a representative mix of the current tier.
    void spawn();
    void clear_sim();
};

struct CosmosExplorerResult {
    bool back_requested = false;
};

// Advance the sandbox (call once per frame from the main loop while running).
void step_cosmos(CosmosState& cosmos, float frame_time);

CosmosExplorerResult draw_cosmos_explorer_scene(AppState& app,
                                                CosmosState& cosmos,
                                                Renderer& renderer,
                                                Rectangle viewport);
