#include "raylib.h"
#include "physics/Simulation.hpp"
#include "physics/UniverseFactory.hpp"   // Universe::create + make_debug_universe
#include "renderer/Trail.hpp"
#include "renderer/Renderer.hpp"
#include "seed/Seed.hpp"                 // hash_seed / derive_axioms (user implements)
#include "seed/DerivedFramework.hpp"

#include <chrono>
#include <algorithm>
#include <memory>
#include <string>

// ============================================================================
// Application modes
// ============================================================================
enum class Mode { HOME, DEBUG, SEEDED };

// ============================================================================
// Home screen
// Returns the chosen mode and (if SEEDED) populates out_seed.
// ============================================================================
static Mode run_homescreen(std::string& out_seed) {
    constexpr Color BG       = {8,  8, 12, 255};
    constexpr Color TITLE_C  = {220, 225, 255, 255};
    constexpr Color OPTION_C = {160, 165, 200, 220};
    constexpr Color HINT_C   = {90,  90, 120, 200};
    constexpr Color CURSOR_C = {180, 200, 255, 255};
    constexpr Color WARN_C   = {255, 140,  80, 200};

    std::string seed_input;
    bool seed_mode = false;

    while (!WindowShouldClose()) {
        // Key handling
        if (!seed_mode) {
            if (IsKeyPressed(KEY_D)) return Mode::DEBUG;
            if (IsKeyPressed(KEY_S)) seed_mode = true;
        } else {
            // Accept printable chars
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch < 127 && seed_input.size() < 48)
                    seed_input += static_cast<char>(ch);
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !seed_input.empty())
                seed_input.pop_back();
            if (IsKeyPressed(KEY_ENTER) && !seed_input.empty()) {
                out_seed = seed_input;
                return Mode::SEEDED;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                seed_mode = false;
                seed_input.clear();
            }
        }

        BeginDrawing();
        ClearBackground(BG);

        // Title
        DrawText("WORLDLINE", 60, 80, 64, TITLE_C);
        DrawText("double pendulum simulator", 63, 152, 20, HINT_C);

        if (!seed_mode) {
            // Menu
            DrawText("[ D ]  Standard pendulum  (debug / benchmark)", 60, 240, 22, OPTION_C);
            DrawText("[ S ]  Enter seed  \xe2\x80\x94  seeded universe", 60, 278, 22, OPTION_C);
            DrawText("Note: seeded mode requires you to implement Seed.hpp first.",
                     60, 360, 16, WARN_C);
        } else {
            DrawText("Enter seed string:", 60, 240, 22, OPTION_C);

            // Input box
            const std::string display = seed_input + "_";
            DrawText(display.c_str(), 60, 278, 26, CURSOR_C);

            DrawText("ENTER to confirm  \xc2\xb7  ESC to cancel", 60, 340, 16, HINT_C);
        }

        EndDrawing();
    }
    return Mode::HOME;  // window closed
}

// ============================================================================
// Simulation loop — shared by both debug and seeded modes.
// ============================================================================
static void run_simulation(Universe& universe,
                            Renderer& renderer,
                            const std::string& description) {
    constexpr int    SUBSTEPS = 10;
    constexpr double PHYS_DT  = 0.001;

    Trail<8192> trail;

    using Clock = std::chrono::steady_clock;
    auto prev   = Clock::now();

    while (!WindowShouldClose()) {
        const auto   now     = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - prev).count();
        (void)elapsed;
        prev = now;

        for (int i = 0; i < SUBSTEPS; ++i) {
            universe.step(PHYS_DT);
            const RenderState rs = universe.extract();
            trail.push(rs.bob2, rs.angular_velocity);
        }

        const RenderState rs = universe.extract();
        renderer.draw_frame(rs, trail, SUBSTEPS, description);

        // ESC returns to the home screen
        if (IsKeyPressed(KEY_ESCAPE)) break;
    }

    renderer.reset_trail();
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
    constexpr int WIDTH  = 1280;
    constexpr int HEIGHT = 720;

    InitWindow(WIDTH, HEIGHT, "Worldline");
    SetTargetFPS(60);

    Renderer renderer(WIDTH, HEIGHT);

    // Command-line seed bypasses the home screen entirely.
    if (argc >= 2) {
        const std::string seed_str = argv[1];
        const uint64_t    seed_int = Worldline::hash_seed(seed_str);
        const Worldline::UniverseAxioms   ax = Worldline::derive_axioms(seed_int);
        const Worldline::DerivedFramework fw = Worldline::derive_framework(ax);
        auto universe = Universe::create(ax, fw);
        run_simulation(*universe, renderer, "[" + seed_str + "]  " + fw.description);
        CloseWindow();
        return 0;
    }

    // Interactive home screen loop
    while (!WindowShouldClose()) {
        std::string seed_str;
        const Mode mode = run_homescreen(seed_str);

        if (mode == Mode::HOME) break;  // window closed during home screen

        if (mode == Mode::DEBUG) {
            auto universe = make_debug_universe();
            run_simulation(*universe, renderer,
                           "[debug]  Euclidean \xc2\xb7 Verlet RK4 \xc2\xb7 standard gravity");
        } else {
            // SEEDED — user must have implemented hash_seed and derive_axioms.
            const uint64_t                seed_int = Worldline::hash_seed(seed_str);
            const Worldline::UniverseAxioms   ax   = Worldline::derive_axioms(seed_int);
            const Worldline::DerivedFramework fw   = Worldline::derive_framework(ax);
            auto universe = Universe::create(ax, fw);
            run_simulation(*universe, renderer,
                           "[" + seed_str + "]  " + fw.description);
        }
    }

    CloseWindow();
    return 0;
}
