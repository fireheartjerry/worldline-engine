#pragma once
// Seeded Universe — formatting, previews, color, and state utilities.
#include "SeededCommon.hpp"

namespace SeededUniverseUi {

// ── Utility ──────────────────────────────────────────────────────────────────

struct PanelHeaderResult {
    bool info_clicked = false;
};

inline std::string format_number(double value, int precision = 3) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

inline constexpr std::array<float, 5> kStageThresholds{{
    0.0f, 0.12f, 1.25f, 2.0f, 2.5f
}};

inline void set_focus(SeededUniverseUiState& seeded,
                      SeededFocusKind kind,
                      int index) {
    seeded.focus_kind = kind;
    seeded.focus_index = std::max(0, index);
}

inline void open_info_modal(SeededUniverseUiState& seeded,
                            SeededInfoTopic topic) {
    seeded.info_topic = topic;
    seeded.info_ignore_mouse_until_release = true;
    seeded.info_modal_scroll = 0.0f;
}

inline int clamped_focus_index(const SeededUniverseUiState& seeded,
                               int count) {
    if (count <= 0) return 0;
    return std::clamp(seeded.focus_index, 0, count - 1);
}

inline float clamp_playback(float time_value) {
    return std::clamp(time_value, 0.0f, 8.0f);
}

inline SeededLawPreview build_law_preview(const MetaSpec& meta_spec) {
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

inline std::string law_mode_label(const MetaSpec& meta_spec) {
    return meta_spec.p_dynamic ? "dynamic p" : "seed-locked p";
}

inline std::string law_readout(const MetaSpec& meta_spec,
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

inline void run_generation(SeededUniverseUiState& state) {
    regenerate_seeded_universe(state);
}

inline float matrix_peak(const double matrix[2][2]) {
    return static_cast<float>(std::max({
        std::abs(matrix[0][0]),
        std::abs(matrix[0][1]),
        std::abs(matrix[1][0]),
        std::abs(matrix[1][1]),
        1.0e-6
    }));
}

inline Color signed_heat_color(double value, float peak, unsigned char alpha = 220) {
    const float normalized = std::clamp(static_cast<float>(value) / peak, -1.0f, 1.0f);
    if (normalized >= 0.0f) {
        const float t = normalized;
        return {
            static_cast<unsigned char>(12 + 52 * (1.0f - t) + WL::CYAN_CORE.r * t),
            static_cast<unsigned char>(24 + 30 * (1.0f - t) + WL::CYAN_CORE.g * t),
            static_cast<unsigned char>(34 + 50 * (1.0f - t) + WL::PLASMA_GREEN.b * t),
            alpha
        };
    }
    const float t = -normalized;
    return {
        static_cast<unsigned char>(18 + 26 * (1.0f - t) + WL::XENON_CORE.r * t),
        static_cast<unsigned char>(18 + 20 * (1.0f - t) + WL::VIOLET_CORE.g * t),
        static_cast<unsigned char>(30 + 44 * (1.0f - t) + WL::VIOLET_CORE.b * t),
        alpha
    };
}

inline std::vector<float> bucketize_cells(const std::vector<std::uint8_t>& cells,
                                          std::size_t bucket_count) {
    std::vector<float> buckets(bucket_count, 0.0f);
    if (cells.empty() || bucket_count == 0u) return buckets;
    const std::size_t stride = std::max<std::size_t>(1u, cells.size() / bucket_count);
    for (std::size_t bucket = 0; bucket < bucket_count; ++bucket) {
        const std::size_t begin = bucket * stride;
        const std::size_t end = std::min(cells.size(), begin + stride);
        float sum = 0.0f;
        for (std::size_t index = begin; index < end; ++index) {
            sum += static_cast<float>(cells[index]) / 255.0f;
        }
        buckets[bucket] = sum / static_cast<float>(std::max<std::size_t>(1u, end - begin));
    }
    return buckets;
}

inline std::array<double, 6> stage_strengths(const MetaSpec& ms) {
    const auto frob = [](const double matrix[2][2]) {
        return std::sqrt(
            matrix[0][0] * matrix[0][0] +
            matrix[0][1] * matrix[0][1] +
            matrix[1][0] * matrix[1][0] +
            matrix[1][1] * matrix[1][1]);
    };
    return {{
        frob(ms.g),
        frob(ms.V),
        frob(ms.S),
        frob(ms.C[0]) + frob(ms.C[1]),
        std::abs(ms.T[0][1]) + std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]),
        std::sqrt(ms.q0[0] * ms.q0[0] + ms.q0[1] * ms.q0[1])
            + std::sqrt(ms.qdot0[0] * ms.qdot0[0] + ms.qdot0[1] * ms.qdot0[1])
    }};
}

inline std::size_t visible_count(float playback_time,
                                 float start_time,
                                 float step_time,
                                 std::size_t total_count,
                                 bool reveal_all) {
    if (reveal_all) return total_count;
    if (playback_time < start_time) return 0u;
    return std::min(total_count,
                    static_cast<std::size_t>(1.0f + std::floor((playback_time - start_time) / std::max(0.001f, step_time))));
}

inline float average_bytes(const std::vector<std::uint8_t>& cells) {
    if (cells.empty()) return 0.0f;
    double sum = 0.0;
    for (std::uint8_t value : cells) sum += static_cast<double>(value);
    return static_cast<float>(sum / (255.0 * static_cast<double>(cells.size())));
}

inline float max_abs_register(const std::array<double, SEED_OUTPUT_SIZE>& values) {
    double peak = 0.0;
    for (double value : values) peak = std::max(peak, std::abs(value));
    return static_cast<float>(std::max(1.0, peak));
}

// Pulse alpha for newly revealed items (fades from bright to normal over 0.4s)
inline unsigned char reveal_alpha(float playback_time, float item_reveal_time, unsigned char base = 200) {
    const float age = playback_time - item_reveal_time;
    if (age < 0.0f) return 0;
    if (age > 0.4f) return base;
    const float pulse = 1.0f - (age / 0.4f);
    return static_cast<unsigned char>(std::min(255.0f, base + 55.0f * pulse));
}


} // namespace SeededUniverseUi
