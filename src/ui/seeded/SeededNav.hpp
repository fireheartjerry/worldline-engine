#pragma once
// Seeded Universe — keyboard navigation.
#include "SeededCommon.hpp"

namespace SeededUniverseUi {

// ── Keyboard Navigation ──────────────────────────────────────────────────────

inline void handle_keyboard_nav(SeededUniverseUiState& seeded) {
    if (seeded.input_active) return;
    if (seeded.info_topic != SeededInfoTopic::NONE) return;

    int max_count = 0;
    switch (seeded.focus_kind) {
    case SeededFocusKind::CHECKPOINT:
        max_count = static_cast<int>(seeded.result.expansion_trace.checkpoints.size()); break;
    case SeededFocusKind::MUTATION:
        max_count = static_cast<int>(seeded.result.machine_trace.mutation_events.size()); break;
    case SeededFocusKind::LANE:
        max_count = static_cast<int>(seeded.result.lanes.size()); break;
    case SeededFocusKind::REGISTER_SLOT:
        max_count = static_cast<int>(seeded.result.machine_trace.final_state.registers.size()); break;
    case SeededFocusKind::STAGE:
    case SeededFocusKind::TENSOR:
        max_count = 6; break;
    default: return;
    }

    if (max_count <= 0) return;

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) {
        seeded.focus_index = (seeded.focus_index + 1) % max_count;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) {
        seeded.focus_index = (seeded.focus_index - 1 + max_count) % max_count;
    }

    // Tab cycles focus kind
    if (IsKeyPressed(KEY_TAB)) {
        static constexpr SeededFocusKind kCycle[] = {
            SeededFocusKind::LANE, SeededFocusKind::REGISTER_SLOT,
            SeededFocusKind::CHECKPOINT, SeededFocusKind::MUTATION,
            SeededFocusKind::STAGE, SeededFocusKind::TENSOR
        };
        constexpr int n = 6;
        int current = 0;
        for (int i = 0; i < n; ++i) {
            if (kCycle[i] == seeded.focus_kind) { current = i; break; }
        }
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const int next = shift ? (current - 1 + n) % n : (current + 1) % n;
        seeded.focus_kind = kCycle[next];
        seeded.focus_index = 0;
    }
}

} // namespace SeededUniverseUi
