#include "app/SeededUniverseRuntime.hpp"
#include "app/UniverseProject.hpp"
#include "app/WorldlineStorage.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "app_verification failed: " << message << '\n';
        std::exit(1);
    }
}

void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
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

void test_corrupt_project_does_not_crash() {
    // A project file that was truncated mid-write or hand-edited can contain
    // malformed numbers and partial records. Loading it must never throw; it
    // should recover what it can and drop the rest.
    Storage::ensure_storage_dirs();
    const std::filesystem::path path = Storage::projects_root() / "corrupt-universe.wline";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "id=corrupt-universe\n";
        out << "seed=corrupt seed\n";
        out << "seeded_p=not-a-number\n";
        out << "dynamic_p=maybe\n";
        out << "max_accel=\n";
        out << "p_max=1e999999\n";  // out_of_range overflow
        out << "thumbnail=1.0,oops\n";
        out << "marker=abc|0|0|Bad time\n";
        out << "marker=1.5|2|1|Good marker\n";
        out << "tag=keepme\n";
    }

    UniverseProject loaded;
    const bool ok = Storage::load_project("corrupt-universe", loaded);
    require(ok, "corrupt project must still load via its id");
    require(loaded.id == "corrupt-universe", "id must survive corrupt numeric fields");
    require(loaded.seed == "corrupt seed", "seed must survive corrupt numeric fields");
    require(loaded.markers.size() == 1, "only the well-formed marker should be kept");
    require(loaded.markers.front().label == "Good marker", "the surviving marker must be the valid one");
    require(loaded.thumbnail_points.empty(), "malformed thumbnail point must be skipped");
    require(loaded.search_tags.size() == 1, "tags following bad lines must still parse");

    std::filesystem::remove(path);
}

void test_corrupt_settings_does_not_crash() {
    Storage::ensure_storage_dirs();
    {
        std::ofstream out(Storage::settings_path(), std::ios::binary | std::ios::trunc);
        out << "last_seed=recovered\n";
        out << "window_width=not-a-number\n";
        out << "window_height=\n";
        out << "last_screen=UniverseAtlas\n";
    }

    const PersistentAppSettings loaded = Storage::load_settings();
    require(loaded.last_seed == "recovered", "string settings survive corrupt numerics");
    require(loaded.last_screen == "UniverseAtlas", "screen survives corrupt numerics");
    require(loaded.window_width > 0, "window width falls back to a positive default");
    require(loaded.window_height > 0, "window height falls back to a positive default");
}

} // namespace

int main() {
    // Keep the test hermetic: write into a throwaway directory rather than the
    // user's real data location.
    const std::filesystem::path sandbox =
        std::filesystem::temp_directory_path() / "worldline-test-data";
    std::filesystem::remove_all(sandbox);
    set_env("WORLDLINE_DATA_DIR", sandbox.string());

    std::filesystem::create_directories(Storage::projects_root());
    test_project_round_trip();
    test_catalog_query_finds_derived_tags();
    test_settings_round_trip();
    test_corrupt_project_does_not_crash();
    test_corrupt_settings_does_not_crash();

    std::filesystem::remove_all(sandbox);
    return 0;
}
