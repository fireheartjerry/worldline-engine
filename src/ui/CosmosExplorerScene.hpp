#pragma once

#include "raylib.h"
#include "app/AppTypes.hpp"
#include "cosmos/EcoSim.hpp"
#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/ProcUniverse.hpp"
#include "cosmos/ScaleLadder.hpp"
#include "cosmos/Universe.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class Renderer;

// 2D navigation camera for the sandbox stage: an animated zoom + pan that also
// flows continuously through the scale ladder (zoom in past a tier -> drop to the
// next-smaller tier; zoom out past it -> rise to the next-larger one).
struct CosmosCamera {
    double zoom = 1.0;          // applied zoom (animated toward target)
    double target_zoom = 1.0;   // input target
    Vec2 pan{};                 // applied world-space center offset (animated)
    Vec2 target_pan{};          // input target
    float flash = 0.0f;         // tier-transition highlight, fades to 0
    bool dragging = false;
    bool primed = false;        // whether the stage has been auto-populated once
};

// Descent navigation: a path from the universe root down to the node currently
// in focus, driven by the procedural engine. Path entries are COPIES (the
// engine's node() reference can be invalidated by a later call via LRU eviction).
struct DescentState {
    std::unique_ptr<cosmos::ProcUniverse> universe; // root seed = genome.signature
    std::uint64_t root_seed = 0;                     // for reseed detection
    std::vector<cosmos::ProcNode> path;              // root .. focus (back() = focus)
    int hovered_child = -1;                           // index into focus().children
    CosmosCamera camera;                              // reuse zoom/pan/flash machinery
    float transition = 0.0f;                          // enter/leave animation envelope
    bool initialized = false;

    // Descend targeting: the last cursor-anchored zoom point (screen space,
    // x < 0 when invalid -> fall back to the stage centre), and the child the
    // camera will enter if the zoom-in continues. The same pick drives both the
    // descend trigger and the on-stage "about to enter" highlight, so what you
    // aim at is always what you get.
    Vector2 zoom_anchor{-1.0f, -1.0f};
    int descend_hint = -1;

    // Per-universe render caches (rebuilt only when the focused seed changes):
    // the cosmic-web census shares for the analysis deck, and the 2-nearest-
    // neighbour filament edge list (topology is camera-invariant).
    float web_census[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    std::uint64_t web_census_seed = 0;
    std::vector<std::pair<int, int>> web_edges;
    std::uint64_t web_edges_seed = 0;

    // Live ecosystem simulation: built when entering an Ecosystem node and advanced
    // with a fixed timestep (FPS-independent, deterministic) so the food web breathes
    // smoothly; carries population history for the observability sparklines.
    cosmos::ecosim::LiveSim live;
    std::uint64_t live_seed = 0;

    // Single per-frame layout pass: child screen positions, reused by hover, pick,
    // food-web links, and sprites (avoids 3-4x redundant recomputation).
    std::vector<float> child_px;
    std::vector<float> child_py;
    int hover_locked = -1;   // hover hysteresis (prevents flip-flop near ties)
    bool analysis_open = true; // tier scientific-instrument deck (toggle with G)

    // ── Navigation shell: a keyboard-driven selection cursor (independent of the
    //    mouse) so every node is reachable precisely; arrows move it directionally,
    //    Tab cycles, Enter enters, [ ] cycle siblings, number keys jump, Home roots.
    int selected_child = -1;

    // ── Simulation interior: granular time control so the full behaviour space is
    //    observable — pause to study, scrub speed to reach long-term cycles, step
    //    one tick, perturb a population and watch it recover. A focused species is
    //    cross-highlighted everywhere (web, sparklines, deck).
    bool   sim_paused = false;
    double sim_speed = 1.0;     // 0.25 .. 16x time dilation
    bool   sim_step_once = false;
    int    focus_species = -1;  // selected species for cross-highlight / perturbation
    double perturb_flash = 0.0; // brief visual cue after a perturbation

    const cosmos::ProcNode& focus() const { return path.back(); }
    cosmos::NodeKind focus_kind() const {
        return path.empty() ? cosmos::NodeKind::Universe : path.back().kind;
    }
    int depth() const { return static_cast<int>(path.size()) - 1; } // 0 at universe
};

// All state for the Cosmos Explorer: the universe's law genome, the object
// catalog specialized to it, the currently selected scale tier, and a live
// N-body sandbox. Owned by main() and passed to the scene each frame.
struct CosmosState {
    bool initialized = false;
    std::string seed;
    cosmos::LawGenome genome;
    cosmos::UniverseClassification classification;
    cosmos::UniversePalette palette;
    std::vector<cosmos::UniverseObject> catalog;

    cosmos::Scale scale = cosmos::Scale::STELLAR;
    int selected_object = 0;  // index within the current tier's object list
    int compare_object = -1;  // second object for comparison, -1 = none

    cosmos::NBodySystem system;
    bool has_sim = false;
    bool running = false;
    int step_count = 0;      // fixed steps taken (drives exact reproduction)
    double accumulator = 0.0;
    double elapsed = 0.0;
    bool browser_open = false;  // saved-sandbox browser modal
    bool dossier_open = false;  // generation report overlay
    CosmosCamera camera;        // 2D navigation (zoom/pan) for the legacy tier sandbox
    bool descent_mode = true;   // descend-into-instances navigation (vs legacy tiers)
    DescentState descent;       // procedural descent navigation state

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
