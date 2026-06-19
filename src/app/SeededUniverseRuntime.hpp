#pragma once

#include "app/AppTypes.hpp"
#include "app/UniverseModel.hpp"
#include "physics/LawSpec.hpp"
#include "physics/ObservableExtractor.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct SeededUniverseRuntime {
    std::unique_ptr<LawSpec> law_spec;
    std::unique_ptr<ObservableExtractor> extractor;
    LawState law_state{};
    // Bumped on every configure() so the field renderer knows when to rebuild.
    std::uint64_t config_token = 0;
    PendulumDraft visual_draft{};
    Simulation visual_simulation{};
    PendulumState visual_smoothed{};
    Trail<APP_TRAIL_CAPACITY> trail;
    RunMode mode = RunMode::STOPPED;
    double accumulator = 0.0;
    double trail_sample_timer = 0.0;
    double elapsed_time = 0.0;
    bool trail_needs_upload = true;
    double history_sample_timer = 0.0;
    std::vector<UniverseSnapshot> history;
    std::vector<TimelineMarker> markers;
    bool scrubbing = false;
    int active_snapshot_index = -1;

    bool ready() const { return law_spec != nullptr && extractor != nullptr; }
    void clear();
    void configure(const MetaSpec& meta_spec);
    void restart();
    void clear_trail();
    UniverseSnapshot current_snapshot() const;
    void sync_visual_to_snapshot(const UniverseSnapshot& snapshot);
    void restore_markers(const std::vector<TimelineMarker>& restored_markers);
    void pin_marker(const std::string& label);
    void scrub_to_index(int index);
    void resume_live();
    int step(float frame_time);
};

SeededUniverseResult build_seeded_universe_result(const std::string& seed);
void regenerate_seeded_universe(SeededUniverseUiState& state);
void replay_seeded_universe_trace(SeededUniverseUiState& state);
