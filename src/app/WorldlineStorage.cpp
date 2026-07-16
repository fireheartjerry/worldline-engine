#include "app/WorldlineStorageInternal.hpp"

namespace Storage {

std::filesystem::path data_root() {
    // Explicit override wins; used by tests to stay hermetic and by users who
    // want a portable data directory.
    if (const char* custom = env_or_null("WORLDLINE_DATA_DIR")) {
        return std::filesystem::path(custom);
    }
#if defined(_WIN32)
    if (const char* appdata = env_or_null("APPDATA")) {
        return std::filesystem::path(appdata) / "Worldline";
    }
#elif defined(__APPLE__)
    if (const char* home = env_or_null("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "Worldline";
    }
#else
    if (const char* xdg = env_or_null("XDG_DATA_HOME")) {
        return std::filesystem::path(xdg) / "worldline";
    }
    if (const char* home = env_or_null("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "worldline";
    }
#endif
    // Last resort: keep data next to the working directory so the app still runs
    // when no home/appdata location is discoverable.
    return std::filesystem::current_path() / "worldline-data";
}

std::filesystem::path projects_root() {
    return data_root() / "projects";
}

std::filesystem::path settings_path() {
    return data_root() / "settings.txt";
}

void ensure_storage_dirs() {
    std::filesystem::create_directories(projects_root());
}

std::string now_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t as_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm calendar{};
#if defined(_WIN32)
    localtime_s(&calendar, &as_time_t);
#else
    localtime_r(&as_time_t, &calendar);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &calendar);
    return buffer;
}

std::string make_project_id(const std::string& seed) {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const long long millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return sanitize_seed_fragment(seed) + "-" + std::to_string(millis);
}

bool save_project(const UniverseProject& project) {
    ensure_storage_dirs();
    if (project.id.empty()) {
        return false;
    }

    const std::filesystem::path path = projects_root() / (project.id + ".wline");
    std::ostringstream out;

    write_key_value(out, "id", project.id);
    write_key_value(out, "seed", project.seed);
    write_key_value(out, "title", project.title);
    write_key_value(out, "notes", project.notes);
    write_key_value(out, "descriptor", project.descriptor);
    write_key_value(out, "created_at", project.created_at);
    write_key_value(out, "updated_at", project.updated_at);
    write_key_value(out, "dynamic_p", project.dynamic_p);
    write_key_value(out, "seeded_p", project.seeded_p);
    write_key_value(out, "linear_gain", project.linear_gain);
    write_key_value(out, "accel_ceiling", project.accel_ceiling);
    write_key_value(out, "max_accel", project.max_accel);
    write_key_value(out, "radius_mean", project.radius_mean);
    write_key_value(out, "radius_peak", project.radius_peak);
    write_key_value(out, "handedness", project.handedness);
    write_key_value(out, "p_min", project.p_min);
    write_key_value(out, "p_max", project.p_max);
    write_key_value(out, "workspace_title", project.workspace.title);
    write_key_value(out, "workspace_notes", project.workspace.notes);
    write_key_value(out, "workspace_selected_snapshot", project.workspace.selected_snapshot);
    write_key_value(out, "workspace_selected_marker", project.workspace.selected_marker);
    write_key_value(out, "workspace_glossary_open", project.workspace.glossary_open);

    write_key_value(out, "thumbnail_count", static_cast<int>(project.thumbnail_points.size()));
    for (const Vec2& point : project.thumbnail_points) {
        out << "thumbnail=" << std::setprecision(17) << point.x << ',' << point.y << '\n';
    }

    write_key_value(out, "marker_count", static_cast<int>(project.markers.size()));
    for (const TimelineMarker& marker : project.markers) {
        out << "marker="
            << std::setprecision(17) << marker.time << '|'
            << marker.snapshot_index << '|'
            << (marker.pinned ? 1 : 0) << '|'
            << escape_text(marker.label) << '\n';
    }

    write_key_value(out, "tag_count", static_cast<int>(project.search_tags.size()));
    for (const std::string& tag : project.search_tags) {
        write_key_value(out, "tag", tag);
    }

    return commit_atomic(path, out.str());
}

bool load_project(const std::string& id, UniverseProject& project) {
    ensure_storage_dirs();
    const std::filesystem::path path = projects_root() / (id + ".wline");
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    UniverseProject loaded;
    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "id=")) loaded.id = unescape_text(read_value(line));
        else if (starts_with(line, "seed=")) loaded.seed = unescape_text(read_value(line));
        else if (starts_with(line, "title=")) loaded.title = unescape_text(read_value(line));
        else if (starts_with(line, "notes=")) loaded.notes = unescape_text(read_value(line));
        else if (starts_with(line, "descriptor=")) loaded.descriptor = unescape_text(read_value(line));
        else if (starts_with(line, "created_at=")) loaded.created_at = unescape_text(read_value(line));
        else if (starts_with(line, "updated_at=")) loaded.updated_at = unescape_text(read_value(line));
        else if (starts_with(line, "dynamic_p=")) loaded.dynamic_p = parse_int(read_value(line), loaded.dynamic_p ? 1 : 0) != 0;
        else if (starts_with(line, "seeded_p=")) loaded.seeded_p = parse_double(read_value(line), loaded.seeded_p);
        else if (starts_with(line, "linear_gain=")) loaded.linear_gain = parse_double(read_value(line), loaded.linear_gain);
        else if (starts_with(line, "accel_ceiling=")) loaded.accel_ceiling = parse_double(read_value(line), loaded.accel_ceiling);
        else if (starts_with(line, "max_accel=")) loaded.max_accel = parse_double(read_value(line), loaded.max_accel);
        else if (starts_with(line, "radius_mean=")) loaded.radius_mean = parse_double(read_value(line), loaded.radius_mean);
        else if (starts_with(line, "radius_peak=")) loaded.radius_peak = parse_double(read_value(line), loaded.radius_peak);
        else if (starts_with(line, "handedness=")) loaded.handedness = parse_double(read_value(line), loaded.handedness);
        else if (starts_with(line, "p_min=")) loaded.p_min = parse_double(read_value(line), loaded.p_min);
        else if (starts_with(line, "p_max=")) loaded.p_max = parse_double(read_value(line), loaded.p_max);
        else if (starts_with(line, "workspace_title=")) loaded.workspace.title = unescape_text(read_value(line));
        else if (starts_with(line, "workspace_notes=")) loaded.workspace.notes = unescape_text(read_value(line));
        else if (starts_with(line, "workspace_selected_snapshot=")) loaded.workspace.selected_snapshot = parse_int(read_value(line), loaded.workspace.selected_snapshot);
        else if (starts_with(line, "workspace_selected_marker=")) loaded.workspace.selected_marker = parse_int(read_value(line), loaded.workspace.selected_marker);
        else if (starts_with(line, "workspace_glossary_open=")) loaded.workspace.glossary_open = parse_int(read_value(line), loaded.workspace.glossary_open ? 1 : 0) != 0;
        else if (starts_with(line, "thumbnail=")) {
            const std::string value = read_value(line);
            const std::size_t split = value.find(',');
            if (split != std::string::npos) {
                const std::string x_text = value.substr(0, split);
                const std::string y_text = value.substr(split + 1);
                const double x = parse_double(x_text, std::nan(""));
                const double y = parse_double(y_text, std::nan(""));
                if (!std::isnan(x) && !std::isnan(y)) {
                    loaded.thumbnail_points.push_back({x, y});
                }
            }
        } else if (starts_with(line, "marker=")) {
            const std::string value = read_value(line);
            const std::size_t a = value.find('|');
            const std::size_t b = value.find('|', a == std::string::npos ? a : a + 1);
            const std::size_t c = value.find('|', b == std::string::npos ? b : b + 1);
            if (a != std::string::npos && b != std::string::npos && c != std::string::npos) {
                TimelineMarker marker;
                marker.time = parse_double(value.substr(0, a), std::nan(""));
                marker.snapshot_index = parse_int(value.substr(a + 1, b - a - 1), -1);
                marker.pinned = parse_int(value.substr(b + 1, c - b - 1), 0) != 0;
                marker.label = unescape_text(value.substr(c + 1));
                if (!std::isnan(marker.time) && marker.snapshot_index >= 0) {
                    loaded.markers.push_back(std::move(marker));
                }
            }
        } else if (starts_with(line, "tag=")) {
            loaded.search_tags.push_back(unescape_text(read_value(line)));
        }
    }

    loaded.workspace.project_id = loaded.id;
    project = std::move(loaded);
    return !project.id.empty();
}

CatalogIndex load_catalog() {
    ensure_storage_dirs();
    CatalogIndex catalog;
    for (const auto& entry : std::filesystem::directory_iterator(projects_root())) {
        if (!entry.is_regular_file() || entry.path().extension() != ".wline") {
            continue;
        }

        UniverseProject project;
        if (load_project(entry.path().stem().string(), project)) {
            catalog.projects.push_back(std::move(project));
        }
    }

    // Timestamps have one-second granularity and directory iteration order is
    // unspecified, so tie-break on id for a stable catalog order everywhere.
    std::sort(catalog.projects.begin(), catalog.projects.end(), [](const UniverseProject& lhs,
                                                                  const UniverseProject& rhs) {
        if (lhs.updated_at != rhs.updated_at) return lhs.updated_at > rhs.updated_at;
        return lhs.id < rhs.id;
    });
    return catalog;
}

std::vector<std::size_t> query_catalog(const CatalogIndex& catalog, const CatalogQuery& query) {
    std::vector<std::size_t> matches;
    const std::string needle = lowercase(query.text);
    for (std::size_t index = 0; index < catalog.projects.size(); ++index) {
        const UniverseProject& project = catalog.projects[index];
        std::string blob;
        append_search_blob(blob, project.id);
        append_search_blob(blob, project.seed);
        append_search_blob(blob, project.title);
        append_search_blob(blob, project.notes);
        append_search_blob(blob, project.descriptor);
        for (const std::string& tag : project.search_tags) {
            append_search_blob(blob, tag);
        }

        if (needle.empty() || blob.find(needle) != std::string::npos) {
            matches.push_back(index);
        }
    }
    return matches;
}

} // namespace Storage
