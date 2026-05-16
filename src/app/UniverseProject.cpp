#include "app/UniverseProject.hpp"

#include "app/SeededUniverseRuntime.hpp"
#include "app/WorldlineStorage.hpp"

#include <algorithm>
#include <cmath>

namespace {

std::vector<Vec2> normalize_thumbnail_points(const std::vector<Vec2>& points) {
    std::vector<Vec2> normalized;
    if (points.empty()) {
        return normalized;
    }

    double min_x = points.front().x;
    double max_x = points.front().x;
    double min_y = points.front().y;
    double max_y = points.front().y;
    for (const Vec2& point : points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }

    const double span_x = std::max(1.0e-6, max_x - min_x);
    const double span_y = std::max(1.0e-6, max_y - min_y);
    normalized.reserve(points.size());
    for (const Vec2& point : points) {
        normalized.push_back({
            (point.x - min_x) / span_x,
            (point.y - min_y) / span_y
        });
    }
    return normalized;
}

std::vector<std::string> derive_tags(const SeededUniverseResult& result) {
    std::vector<std::string> tags;
    tags.push_back(result.meta_spec.p_dynamic ? "dynamic p" : "seed locked p");
    tags.push_back(result.meta_spec.time_varying ? "time varying" : "time static");

    if (result.law_preview.linear_gain >= 0.95) tags.push_back("high gain");
    else if (result.law_preview.linear_gain <= 0.55) tags.push_back("low gain");

    if (result.law_preview.max_accel >= result.law_preview.accel_ceiling * 0.80) {
        tags.push_back("high acceleration");
    }
    if (std::abs(result.law_preview.handedness) > 0.08) {
        tags.push_back(result.law_preview.handedness > 0.0 ? "positive handedness" : "negative handedness");
    }
    if (result.meta_spec.drift_amp > 0.0) tags.push_back("drift");
    if (result.meta_spec.s_c > 0.55) tags.push_back("strong time asymmetry");
    if (result.meta_spec.s_b > 0.60) tags.push_back("strong symmetry filter");
    return tags;
}

} // namespace

UniverseProject make_universe_project(const SeededUniverseUiState& seeded) {
    UniverseProject project;
    if (!seeded.result.ready) {
        return project;
    }

    project.id = seeded.workspace.project_id.empty()
        ? Storage::make_project_id(seeded.result.seed)
        : seeded.workspace.project_id;
    project.seed = seeded.result.seed;
    project.title = seeded.workspace.title.empty() ? seeded.result.seed : seeded.workspace.title;
    project.notes = seeded.workspace.notes;
    project.descriptor = seeded.result.descriptor;
    project.created_at = Storage::now_timestamp();
    project.updated_at = project.created_at;
    project.dynamic_p = seeded.result.meta_spec.p_dynamic;
    project.seeded_p = seeded.result.meta_spec.p;
    project.linear_gain = seeded.result.law_preview.linear_gain;
    project.accel_ceiling = seeded.result.law_preview.accel_ceiling;
    project.max_accel = seeded.result.law_preview.max_accel;
    project.radius_mean = seeded.result.law_preview.radius_mean;
    project.radius_peak = seeded.result.law_preview.radius_peak;
    project.handedness = seeded.result.law_preview.handedness;
    project.p_min = seeded.result.law_preview.p_min;
    project.p_max = seeded.result.law_preview.p_max;
    project.thumbnail_points = normalize_thumbnail_points(seeded.result.law_preview.phase_path);
    project.workspace = seeded.workspace;
    project.workspace.project_id = project.id;
    if (seeded.runtime != nullptr) {
        project.markers = seeded.runtime->markers;
    }
    project.search_tags = derive_tags(seeded.result);
    return project;
}

void apply_universe_project(SeededUniverseUiState& seeded, const UniverseProject& project) {
    seeded.seed_input = project.seed;
    regenerate_seeded_universe(seeded);
    seeded.workspace = project.workspace;
    seeded.workspace.project_id = project.id;
    seeded.workspace.title = project.title;
    seeded.workspace.notes = project.notes;
    if (seeded.runtime != nullptr) {
        seeded.runtime->restore_markers(project.markers);
    }
}
