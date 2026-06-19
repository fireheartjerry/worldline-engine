#pragma once
// Seeded Universe — contextual info modal.
#include "SeededCommon.hpp"

namespace SeededUniverseUi {

// ── Info Modal ───────────────────────────────────────────────────────────────

inline const char* info_title(SeededInfoTopic topic) {
    switch (topic) {
    case SeededInfoTopic::COMMAND_DECK: return "Seed Drive";
    case SeededInfoTopic::PIPELINE: return "The Pipeline";
    case SeededInfoTopic::EXPANSION: return "Seed Spreading";
    case SeededInfoTopic::MACHINE: return "Machine Edits";
    case SeededInfoTopic::LANES: return "Lane Spectrum";
    case SeededInfoTopic::REGISTERS: return "Register Residue";
    case SeededInfoTopic::ORBIT: return "Assembly Map";
    case SeededInfoTopic::TENSORS: return "Tensor Vault";
    case SeededInfoTopic::LAW_WEAVE: return "Law Weave";
    case SeededInfoTopic::DESCRIPTOR: return "Universe Readout";
    default: return "";
    }
}

inline const char* info_body(SeededInfoTopic topic) {
    switch (topic) {
    case SeededInfoTopic::COMMAND_DECK:
        return "This is your control panel for the entire seed-to-universe process. The seed text is the only input you give. Everything else is generated deterministically from it.\n\nGenerate runs the full pipeline from scratch. Replay resets the animation so you can watch it build step by step. Debug Trace toggles between instant results and the animated stage-by-stage reveal.\n\nThe presets are interesting seeds we picked because they produce visually distinct universes. Try your own name, a word, or anything you like.";
    case SeededInfoTopic::PIPELINE:
        return "The pipeline shows the five stages your seed goes through to become a universe.\n\n1. Fold: Your text gets converted into raw bytes.\n2. Expand: Those bytes are spread across a 512-cell tape using cellular automata, so each byte's influence reaches every part of the tape.\n3. Mutate: A self-modifying machine reads the tape and rewrites its own rules as it goes, creating complex behavior from simple input.\n4. Emit: The machine produces 32 output values between 0 and 1.\n5. Assemble: Those 32 numbers become the matrices and initial conditions that define your universe's physics.";
    case SeededInfoTopic::EXPANSION:
        return "Each row is a snapshot of the 512-byte tape at a different point in time. The initial fold shows your raw seed laid out on the tape. Each generation after that shows the tape after the cellular automaton has mixed it further.\n\nBrighter bars mean more activity at that position. Early snapshots show the seed's fingerprint clearly. Later snapshots should look more uniform as information spreads everywhere.\n\nIf late snapshots still show sharp patterns, the seed has a strong structural signature that survives mixing.";
    case SeededInfoTopic::MACHINE:
        return "The seed machine reads the expanded tape and uses it to process 32 internal registers. The twist: as it runs, it rewrites its own instruction table.\n\nFULL edits are major rewrites that swap what kind of math operation a slot performs. SOFT edits are gentle tweaks to an operation's parameters.\n\nThe mix of FULL and SOFT edits (roughly 55%/45%) is intentional. Too many FULL edits and the machine is chaotic. Too few and every seed produces similar output.\n\nEach card shows which operation was inserted, which slot was changed, and how the parameter shifted.";
    case SeededInfoTopic::LANES:
        return "The lane spectrum is the machine's final output: 32 values, each between 0 and 1. These are the raw building blocks for your universe's physics.\n\nThe bar chart shows each lane's value. The line underneath connects neighboring lanes so you can spot patterns: smooth gradients, sudden jumps, or repeating clusters.\n\nClick any bar to inspect it. The readout panel at the bottom will show you exact values and neighbors.";
    case SeededInfoTopic::REGISTERS:
        return "After the machine finishes processing, its 32 internal registers still hold values. Unlike the lanes (which are normalized to 0-1), registers can be positive or negative.\n\nTeal-colored cells are positive. Violet-colored cells are negative. Brighter colors mean larger values.\n\nLarge positive or negative outliers usually mean the machine developed a strong bias in that register, which often corresponds to a distinctive feature in the final universe.";
    case SeededInfoTopic::ORBIT:
        return "The orbit map arranges the six main components of your universe's physics in a circle. Each node represents one family of physical laws.\n\nMetric: How distances work in this universe.\nPotential: The energy landscape that pulls things around.\nSymmetry: How balanced the physics is between the two pendulum arms.\nCoupling: How strongly the two pendulums influence each other.\nArrow: Whether time has directional effects.\nLaunch: The starting position and velocity.\n\nBigger, brighter nodes have more influence on the final simulation.";
    case SeededInfoTopic::TENSORS:
        return "Each tile shows a 2x2 matrix: four numbers arranged in a grid. These matrices are the actual mathematical objects that define your universe's physics.\n\nTeal cells are positive values. Violet cells are negative values. Brighter = larger magnitude.\n\nThink of each matrix as a set of knobs. The diagonal values (top-left and bottom-right) affect each pendulum arm independently. The off-diagonal values (top-right and bottom-left) create cross-effects between the arms.\n\nClick any tile to inspect it in the readout panel below.";
    case SeededInfoTopic::LAW_WEAVE:
        return "Law Weave is the first place the seed stops looking like ingredients and starts looking like motion. The MetaSpec tensors are passed into LawSpec, which builds a vector field on phase space and then samples it with RK4.\n\nThe phase portrait on the left is not a decorative path. It is a short preview orbit generated directly from the seeded initial condition. The sparkline on the right shows how the nonlinear exponent p behaves across that preview. If p is dynamic, the line flexes with angular momentum but stays clamped near its seed.\n\nThe gain and ceiling metrics tell you how the law engine was stabilized: the linear potential term is spectrally bounded at construction time, and large accelerations trigger runtime step refinement rather than explosive drift. The three S bars show how symmetry affects the law after assembly: additive bias, symmetry filtering, and antisymmetric torque.";
    case SeededInfoTopic::DESCRIPTOR:
        return "The readout panel adapts to whatever you've clicked. It shows detailed information about the selected element: a lane value, a register, a mutation event, a tensor matrix, or an orbit node.\n\nThe left side shows your current selection with a value gauge. The right side shows a summary of the whole universe including the key physics parameters and a natural-language description of what this particular universe is like.\n\nThe strength chips at the bottom right let you quickly jump between the six physics families.";
    default:
        return "";
    }
}

inline void draw_info_modal(SeededUniverseUiState& seeded, Rectangle viewport, float scale) {
    if (seeded.info_topic == SeededInfoTopic::NONE) return;

    DrawRectangleRec(viewport, {2, 8, 14, 200});
    DrawCircleGradient(static_cast<int>(viewport.x + viewport.width * 0.5f),
                       static_cast<int>(viewport.y + viewport.height * 0.5f),
                       std::max(viewport.width, viewport.height) * 0.6f,
                       {0, 0, 0, 0},
                       {2, 6, 10, 140});

    const float modal_w = std::min(760.0f * scale, viewport.width * 0.78f);
    const float modal_h = std::min(620.0f * scale, viewport.height * 0.78f);
    const Rectangle modal = {
        viewport.x + (viewport.width - modal_w) * 0.5f,
        viewport.y + (viewport.height - modal_h) * 0.5f,
        modal_w,
        modal_h
    };

    const Vector2 mouse = GetMousePosition();
    if (seeded.info_ignore_mouse_until_release) {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            seeded.info_ignore_mouse_until_release = false;
        }
    } else if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, modal)) || IsKeyPressed(KEY_ESCAPE)) {
        seeded.info_topic = SeededInfoTopic::NONE;
        seeded.info_ignore_mouse_until_release = false;
    }

    draw_card(modal, {8, 16, 28, 248}, with_alpha(WL::CYAN_CORE, 100));
    DrawRectangleGradientEx(modal,
                            {16, 58, 78, 50},
                            {10, 18, 32, 10},
                            {0, 0, 0, 0},
                            {48, 16, 82, 35});
    draw_corner_brackets({modal.x + 4, modal.y + 4, modal.width - 8, modal.height - 8},
                         14.0f * scale, 1.2f, with_alpha(WL::CYAN_CORE, 60));

    draw_text("INFO", {modal.x + 18.0f * scale, modal.y + 16.0f * scale}, 12.0f * scale, with_alpha(WL::CYAN_CORE, 160));
    draw_text(info_title(seeded.info_topic), {modal.x + 18.0f * scale, modal.y + 32.0f * scale}, 28.0f * scale, WL::TEXT_PRIMARY);
    draw_separator_h(modal.x + 18.0f * scale, modal.y + 64.0f * scale, modal.width - 36.0f * scale, with_alpha(WL::GLASS_BORDER, 60));

    const Rectangle info_view = {
        modal.x + 18.0f * scale,
        modal.y + 72.0f * scale,
        modal.width - 28.0f * scale,
        modal.height - 122.0f * scale
    };
    const float info_text_size = 16.0f * scale;
    const float info_line_gap = 5.5f * scale;
    const float info_content_h =
        measure_wrapped_ui_text_height(info_body(seeded.info_topic), info_view.width - 12.0f * scale, info_text_size, info_line_gap);
    const float info_max_scroll = std::max(0.0f, info_content_h - info_view.height);
    seeded.info_modal_scroll = std::clamp(seeded.info_modal_scroll, 0.0f, info_max_scroll);
    if (CheckCollisionPointRec(mouse, info_view)) {
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.0f) {
            seeded.info_modal_scroll = std::clamp(
                seeded.info_modal_scroll - wheel * 44.0f * scale,
                0.0f,
                info_max_scroll);
        }
    }

    BeginScissorMode(static_cast<int>(info_view.x), static_cast<int>(info_view.y),
                     static_cast<int>(info_view.width), static_cast<int>(info_view.height));
    draw_text_block(info_body(seeded.info_topic),
                    {info_view.x, info_view.y - seeded.info_modal_scroll, info_view.width - 12.0f * scale, info_content_h + info_line_gap},
                    info_text_size,
                    WL::TEXT_SECONDARY,
                    info_line_gap);
    EndScissorMode();
    draw_scrollbar(info_view, seeded.info_modal_scroll, info_max_scroll);

    draw_text("Click outside or press Escape to close",
              {modal.x + 18.0f * scale, modal.y + modal.height - 28.0f * scale},
              12.0f * scale,
              WL::TEXT_INACTIVE);

    if (draw_button({modal.x + modal.width - 90.0f * scale, modal.y + 14.0f * scale, 72.0f * scale, 26.0f * scale},
                    "CLOSE",
                    {20, 36, 60, 235},
                    {32, 54, 88, 255},
                    WL::TEXT_PRIMARY,
                    true,
                    scale * 0.85f)) {
        seeded.info_topic = SeededInfoTopic::NONE;
        seeded.info_ignore_mouse_until_release = false;
    }
}


} // namespace SeededUniverseUi
