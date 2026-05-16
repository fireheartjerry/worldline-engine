#pragma once

#include "math/Vec2.hpp"
#include "physics/LawSpec.hpp"
#include "physics/PendulumState.hpp"

#include <string>
#include <vector>

struct UniverseSnapshot {
    double time = 0.0;
    LawState law_state{};
    PendulumState observed{};
    double q_radius = 0.0;
    double speed = 0.0;
    double p_value = 0.0;
};

struct TimelineMarker {
    std::string label;
    double time = 0.0;
    int snapshot_index = -1;
    bool pinned = true;
};

struct WorkspaceState {
    std::string project_id;
    std::string title;
    std::string notes;
    int selected_snapshot = -1;
    int selected_marker = -1;
    bool glossary_open = false;
};

struct UniverseProject {
    std::string id;
    std::string seed;
    std::string title;
    std::string notes;
    std::string descriptor;
    std::string created_at;
    std::string updated_at;
    bool dynamic_p = false;
    double seeded_p = 0.0;
    double linear_gain = 0.0;
    double accel_ceiling = 0.0;
    double max_accel = 0.0;
    double radius_mean = 0.0;
    double radius_peak = 0.0;
    double handedness = 0.0;
    double p_min = 0.0;
    double p_max = 0.0;
    std::vector<Vec2> thumbnail_points;
    std::vector<TimelineMarker> markers;
    WorkspaceState workspace;
    std::vector<std::string> search_tags;
};

struct CatalogIndex {
    std::vector<UniverseProject> projects;
};

struct CatalogQuery {
    std::string text;
};

struct GlossaryEntry {
    const char* term = "";
    const char* short_definition = "";
    const char* detail = "";
};

struct PersistentAppSettings {
    std::string last_seed = "worldline";
    std::string last_project_id;
    std::string atlas_query;
    std::vector<std::string> recent_project_ids;
    std::string last_screen = "GuidedFirstUniverse";
    int window_width = 1480;
    int window_height = 920;
};
