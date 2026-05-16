#include "app/SeededUniverseRuntime.hpp"

#include "app/AppFields.hpp"
#include "seed/SeedDebug.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <utility>

namespace {

std::string format_number(double value, int precision = 3) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

SeededLawPreview build_law_preview(const MetaSpec& meta_spec) {
    SeededLawPreview preview;
    LawSpec law(meta_spec);
    LawState state = law.initial_state();
    constexpr int kSamples = 192;
    constexpr double kDt = 0.02;

    preview.phase_path.reserve(kSamples + 1);
    preview.p_samples.reserve(kSamples + 1);
    preview.phase_path.push_back(state.q);
    preview.p_samples.push_back(state.p);
    preview.linear_gain = law.potential_linear_gain();
    preview.accel_ceiling = law.acceleration_ceiling();
    preview.p_min = state.p;
    preview.p_max = state.p;

    double speed_sum = state.v.length();
    double radius_sum = state.q.length();
    double handed_sum = 0.0;
    preview.radius_peak = state.q.length();
    preview.max_accel = law.acceleration(state).length();

    for (int i = 0; i < kSamples; ++i) {
        const double angular = state.q.x * state.v.y - state.q.y * state.v.x;
        handed_sum += angular;

        state = law.step(state, kDt);
        preview.phase_path.push_back(state.q);
        preview.p_samples.push_back(state.p);
        preview.p_min = std::min(preview.p_min, state.p);
        preview.p_max = std::max(preview.p_max, state.p);

        const double speed = state.v.length();
        const double radius = state.q.length();
        speed_sum += speed;
        radius_sum += radius;
        preview.radius_peak = std::max(preview.radius_peak, radius);
        preview.max_accel = std::max(preview.max_accel, law.acceleration(state).length());
    }

    const double sample_count = static_cast<double>(preview.phase_path.size());
    preview.mean_speed = speed_sum / sample_count;
    preview.radius_mean = radius_sum / sample_count;
    preview.handedness = handed_sum / (static_cast<double>(kSamples) + std::abs(handed_sum) + 1.0e-9);
    return preview;
}

std::string law_readout(const MetaSpec& meta_spec,
                        const SeededLawPreview& preview) {
    std::string out = "LAW WEAVE\n";
    out += "LawSpec turns the tensor vault into a bounded phase flow. ";
    out += meta_spec.p_dynamic
        ? "Here the exponent is allowed to breathe with angular momentum before it is pulled back toward its seed. "
        : "Here the exponent remains pinned to its seeded value, so the flow is structurally stable rather than self-adjusting. ";

    out += "The construction-time potential gain is ";
    out += format_number(preview.linear_gain, 2);
    out += ", the observed acceleration peak in the preview is ";
    out += format_number(preview.max_accel, 2);
    out += " against a ceiling of ";
    out += format_number(preview.accel_ceiling, 2);
    out += ". ";

    out += "The symmetry split is carried explicitly: additive ";
    out += format_number(meta_spec.s_a, 2);
    out += ", filter ";
    out += format_number(meta_spec.s_b, 2);
    out += ", torque ";
    out += format_number(meta_spec.s_c, 2);
    out += ". ";

    out += "Across the preview, the orbit radius averages ";
    out += format_number(preview.radius_mean, 2);
    out += " and the handedness bias is ";
    out += format_number(preview.handedness, 2);
    out += ".";
    return out;
}

int record_trail_sample(SeededUniverseRuntime& runtime,
                        double dt,
                        bool force = false) {
    runtime.trail_sample_timer += dt;
    const Vec2 position = runtime.visual_simulation.bob2_pos();
    const double omega = runtime.visual_simulation.omega2();
    const double sample_interval = 1.0 / 240.0;
    const double min_distance =
        std::max(0.0035, runtime.visual_simulation.reach() * 0.0018);

    if (!force && !runtime.trail.empty()) {
        const Vec2 delta = position - runtime.trail.newest().pos;
        if (runtime.trail_sample_timer < sample_interval
            && delta.length_sq() < min_distance * min_distance) {
            return 0;
        }
    }

    runtime.trail.push(position, omega);
    runtime.trail_sample_timer = 0.0;
    return 1;
}

void record_history_sample(SeededUniverseRuntime& runtime, bool force = false) {
    if (!runtime.ready()) {
        return;
    }

    runtime.history_sample_timer += APP_PHYS_DT;
    constexpr double kHistoryInterval = 1.0 / 60.0;
    if (!force && runtime.history_sample_timer < kHistoryInterval) {
        return;
    }

    runtime.history.push_back(runtime.current_snapshot());
    runtime.history_sample_timer = 0.0;
    runtime.active_snapshot_index = static_cast<int>(runtime.history.size()) - 1;
}

void reset_seeded_ui_state(SeededUniverseUiState& state) {
    state.playback_time = 0.0f;
    state.body_scroll = 0.0f;
    state.descriptor_scroll = 0.0f;
    state.info_modal_scroll = 0.0f;
    state.scrub_active = false;
    state.input_active = false;
    state.input_select_all = false;
    state.title_input_active = false;
    state.title_input_select_all = false;
    state.notes_input_active = false;
    state.notes_input_select_all = false;
    state.focus_kind = SeededFocusKind::LANE;
    state.focus_index = 0;
}

} // namespace

void SeededUniverseRuntime::clear() {
    law_spec.reset();
    extractor.reset();
    law_state = {};
    visual_draft = {};
    visual_simulation.reset({}, {});
    visual_smoothed = {};
    trail.clear();
    mode = RunMode::STOPPED;
    accumulator = 0.0;
    trail_sample_timer = 0.0;
    elapsed_time = 0.0;
    trail_needs_upload = true;
    history_sample_timer = 0.0;
    history.clear();
    markers.clear();
    scrubbing = false;
    active_snapshot_index = -1;
}

void SeededUniverseRuntime::configure(const MetaSpec& meta_spec) {
    law_spec = std::make_unique<LawSpec>(meta_spec);
    extractor = std::make_unique<ObservableExtractor>(*law_spec);
    visual_draft = extractor->draft();
    restart();
}

void SeededUniverseRuntime::restart() {
    if (!ready()) {
        clear();
        return;
    }

    law_state = law_spec->initial_state();
    extractor->reset_drift_phase(0.0);
    const PendulumState observed = extractor->observe(law_state, 0.0);
    visual_smoothed = observed;
    visual_simulation.reset(observed, AppState::make_params(visual_draft));
    trail.clear();
    mode = RunMode::RUNNING;
    accumulator = 0.0;
    trail_sample_timer = 0.0;
    elapsed_time = 0.0;
    trail_needs_upload = true;
    history_sample_timer = 0.0;
    history.clear();
    markers.clear();
    scrubbing = false;
    active_snapshot_index = -1;
    record_trail_sample(*this, 0.0, true);
    record_history_sample(*this, true);
}

void SeededUniverseRuntime::clear_trail() {
    trail.clear();
    trail_sample_timer = 0.0;
    trail_needs_upload = true;
    if (ready()) {
        record_trail_sample(*this, 0.0, true);
    }
}

UniverseSnapshot SeededUniverseRuntime::current_snapshot() const {
    UniverseSnapshot snapshot;
    snapshot.time = elapsed_time;
    snapshot.law_state = law_state;
    snapshot.p_value = law_state.p;
    snapshot.q_radius = law_state.q.length();
    snapshot.speed = law_state.v.length();
    snapshot.observed = visual_simulation.state;
    return snapshot;
}

void SeededUniverseRuntime::sync_visual_to_snapshot(const UniverseSnapshot& snapshot) {
    law_state = snapshot.law_state;
    elapsed_time = snapshot.time;
    visual_smoothed = snapshot.observed;
    visual_simulation.reset(snapshot.observed, visual_simulation.params);
    trail_needs_upload = true;
}

void SeededUniverseRuntime::restore_markers(const std::vector<TimelineMarker>& restored_markers) {
    markers = restored_markers;
}

void SeededUniverseRuntime::pin_marker(const std::string& label) {
    TimelineMarker marker;
    marker.label = label;
    marker.time = elapsed_time;
    marker.snapshot_index = active_snapshot_index;
    marker.pinned = true;
    markers.push_back(std::move(marker));
}

void SeededUniverseRuntime::scrub_to_index(int index) {
    if (index < 0 || index >= static_cast<int>(history.size())) {
        return;
    }
    scrubbing = true;
    active_snapshot_index = index;
    sync_visual_to_snapshot(history[static_cast<std::size_t>(index)]);
}

void SeededUniverseRuntime::resume_live() {
    scrubbing = false;
    mode = RunMode::RUNNING;
}

int SeededUniverseRuntime::step(float frame_time) {
    if (!ready() || mode != RunMode::RUNNING || scrubbing) {
        return 0;
    }

    // Run at 2.5× real-time so the trajectory explores richer patterns per second.
    // The APP_MAX_STEPS_PER_FRAME cap (42 steps × 1ms = 42ms) acts as the hard ceiling,
    // which 2.5 × 16.67ms ≈ 41.7ms saturates cleanly at 60 fps.
    constexpr float kTimeScale = 2.5f;
    constexpr double kSmoothTau = 0.025; // 25 ms smoothing time constant for angles

    int new_samples = 0;
    int step_count = 0;
    accumulator += static_cast<double>(kTimeScale * std::min(frame_time, 0.033f));
    while (accumulator >= APP_PHYS_DT && step_count < APP_MAX_STEPS_PER_FRAME) {
        law_spec->step_in_place(law_state, APP_PHYS_DT);
        const PendulumState observed = extractor->observe(law_state, APP_PHYS_DT);

        // Exponential smoothing on angles to kill high-frequency jitter while preserving
        // the large-scale chaotic motion. Omegas are left raw so trail colour stays reactive.
        const double alpha = 1.0 - std::exp(-APP_PHYS_DT / kSmoothTau);
        visual_smoothed.theta1 += alpha * (observed.theta1 - visual_smoothed.theta1);
        visual_smoothed.theta2 += alpha * (observed.theta2 - visual_smoothed.theta2);
        visual_smoothed.omega1 = observed.omega1;
        visual_smoothed.omega2 = observed.omega2;

        visual_simulation.reset(visual_smoothed, visual_simulation.params);
        new_samples += record_trail_sample(*this, APP_PHYS_DT);
        accumulator -= APP_PHYS_DT;
        elapsed_time += APP_PHYS_DT;
        record_history_sample(*this);
        ++step_count;
    }
    return new_samples;
}

SeededUniverseResult build_seeded_universe_result(const std::string& seed) {
    SeededUniverseResult result;
    result.seed = seed;

    try {
        result.expansion_trace = expand_with_trace(result.seed);
        result.machine_trace = compress_with_trace(result.expansion_trace.expanded);
        result.lanes.assign(result.machine_trace.output.begin(), result.machine_trace.output.end());
        result.meta_spec = generate_meta_spec(result.lanes);
        result.descriptor = generate_descriptor(result.meta_spec);
        result.law_preview = build_law_preview(result.meta_spec);
        result.descriptor += "\n\n";
        result.descriptor += law_readout(result.meta_spec, result.law_preview);
        result.ready = true;
    } catch (const std::exception& ex) {
        result.error = ex.what();
    }

    return result;
}

void regenerate_seeded_universe(SeededUniverseUiState& state) {
    if (!state.result.seed.empty() && state.result.seed != state.seed_input) {
        state.workspace.project_id.clear();
        if (state.workspace.title == state.result.seed) {
            state.workspace.title.clear();
        }
        state.workspace.notes.clear();
    }
    state.result = build_seeded_universe_result(state.seed_input);
    reset_seeded_ui_state(state);
    if (state.runtime == nullptr) {
        state.runtime = std::make_unique<SeededUniverseRuntime>();
    }
    if (state.result.ready) {
        state.runtime->configure(state.result.meta_spec);
    } else {
        state.runtime->clear();
    }
}

void replay_seeded_universe_trace(SeededUniverseUiState& state) {
    reset_seeded_ui_state(state);
}
