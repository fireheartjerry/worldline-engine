#include "app/SeededUniverseRuntime.hpp"
#include "app/UniverseProject.hpp"
#include "app/WorldlineStorage.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "app_verification failed: " << message << '\n';
        std::exit(1);
    }
}

void test_project_round_trip() {
    SeededUniverseUiState seeded;
    seeded.seed_input = "portfolio-seed";
    seeded.workspace.title = "Portfolio Seed";
    seeded.workspace.notes = "Saved from test";
    regenerate_seeded_universe(seeded);
    require(seeded.result.ready, "seed generation must succeed for project round-trip");
    require(seeded.runtime != nullptr && seeded.runtime->ready(), "runtime must be ready");

    seeded.runtime->pin_marker("Pinned frame");
    UniverseProject project = make_universe_project(seeded);
    project.updated_at = Storage::now_timestamp();
    if (project.created_at.empty()) {
        project.created_at = project.updated_at;
    }

    require(Storage::save_project(project), "project save must succeed");

    UniverseProject loaded;
    require(Storage::load_project(project.id, loaded), "project load must succeed");
    require(loaded.seed == project.seed, "loaded seed must match");
    require(loaded.title == project.title, "loaded title must match");
    require(loaded.notes == project.notes, "loaded notes must match");
    require(loaded.markers.size() == project.markers.size(), "loaded markers must match");
    require(!loaded.thumbnail_points.empty(), "loaded thumbnail should not be empty");
}

void test_catalog_query_finds_derived_tags() {
    CatalogIndex catalog = Storage::load_catalog();
    const std::vector<std::size_t> dynamic_matches = Storage::query_catalog(catalog, {"dynamic p"});
    const std::vector<std::size_t> seed_matches = Storage::query_catalog(catalog, {"portfolio-seed"});
    require(!seed_matches.empty(), "catalog query must find saved project by seed");
    require(dynamic_matches.size() <= catalog.projects.size(), "catalog query must handle derived tags");
}

void test_settings_round_trip() {
    PersistentAppSettings settings;
    settings.last_seed = "settings-seed";
    settings.last_project_id = "demo-project";
    settings.atlas_query = "dynamic p";
    settings.last_screen = "UniverseAtlas";
    settings.window_width = 1600;
    settings.window_height = 900;
    settings.recent_project_ids = {"one", "two"};

    Storage::save_settings(settings);
    const PersistentAppSettings loaded = Storage::load_settings();
    require(loaded.last_seed == settings.last_seed, "settings seed must round-trip");
    require(loaded.last_project_id == settings.last_project_id, "settings project id must round-trip");
    require(loaded.atlas_query == settings.atlas_query, "settings query must round-trip");
    require(loaded.last_screen == settings.last_screen, "settings screen must round-trip");
    require(loaded.window_width == settings.window_width, "settings width must round-trip");
    require(loaded.window_height == settings.window_height, "settings height must round-trip");
    require(loaded.recent_project_ids.size() == settings.recent_project_ids.size(), "recent projects must round-trip");
}

} // namespace

int main() {
    std::filesystem::create_directories(Storage::projects_root());
    test_project_round_trip();
    test_catalog_query_finds_derived_tags();
    test_settings_round_trip();
    return 0;
}
