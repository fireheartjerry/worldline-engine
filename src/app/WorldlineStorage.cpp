#include "app/WorldlineStorage.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace Storage {
namespace {

std::string escape_text(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char ch : text) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string unescape_text(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool escaping = false;
    for (char ch : text) {
        if (!escaping) {
            if (ch == '\\') {
                escaping = true;
            } else {
                out.push_back(ch);
            }
            continue;
        }

        switch (ch) {
        case 'n': out.push_back('\n'); break;
        case 't': out.push_back('\t'); break;
        case '\\': out.push_back('\\'); break;
        default: out.push_back(ch); break;
        }
        escaping = false;
    }
    return out;
}

bool starts_with(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void write_key_value(std::ostream& out,
                     const char* key,
                     const std::string& value) {
    out << key << '=' << escape_text(value) << '\n';
}

void write_key_value(std::ostream& out,
                     const char* key,
                     double value) {
    out << key << '=' << std::setprecision(17) << value << '\n';
}

void write_key_value(std::ostream& out,
                     const char* key,
                     int value) {
    out << key << '=' << value << '\n';
}

void write_key_value(std::ostream& out,
                     const char* key,
                     bool value) {
    out << key << '=' << (value ? 1 : 0) << '\n';
}

std::string sanitize_seed_fragment(const std::string& seed) {
    std::string out;
    out.reserve(seed.size());
    for (char ch : seed) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            out.push_back('-');
        }
    }
    if (out.empty()) {
        out = "universe";
    }
    while (!out.empty() && out.back() == '-') {
        out.pop_back();
    }
    return out;
}

std::string read_value(const std::string& line) {
    const std::size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return {};
    }
    return line.substr(pos + 1);
}

// Saved files may be hand-edited, truncated by a crash mid-write, or carried
// over from an older schema. Parse numbers defensively so a single malformed
// field falls back to a sane default instead of throwing out of the loader.
double parse_double(const std::string& text, double fallback) {
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        return consumed == 0 ? fallback : value;
    } catch (const std::exception&) {
        return fallback;
    }
}

int parse_int(const std::string& text, int fallback) {
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        return consumed == 0 ? fallback : value;
    } catch (const std::exception&) {
        return fallback;
    }
}

// Write through a sibling temp file and atomically rename it over the target so
// a crash mid-write can never leave a half-written project or settings file.
bool commit_atomic(const std::filesystem::path& path,
                   const std::string& contents) {
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << contents;
        out.flush();
        if (!out) {
            out.close();
            std::error_code remove_ec;
            std::filesystem::remove(temp, remove_ec);
            return false;
        }
    }
    std::error_code rename_ec;
    std::filesystem::rename(temp, path, rename_ec);
    if (rename_ec) {
        std::error_code remove_ec;
        std::filesystem::remove(temp, remove_ec);
        return false;
    }
    return true;
}

void append_search_blob(std::string& blob, const std::string& value) {
    blob += ' ';
    blob += lowercase(value);
}

} // namespace

namespace {

const char* env_or_null(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

} // namespace

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

    std::sort(catalog.projects.begin(), catalog.projects.end(), [](const UniverseProject& lhs,
                                                                  const UniverseProject& rhs) {
        return lhs.updated_at > rhs.updated_at;
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

PersistentAppSettings load_settings() {
    ensure_storage_dirs();
    PersistentAppSettings settings;
    std::ifstream in(settings_path(), std::ios::binary);
    if (!in.is_open()) {
        return settings;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "last_seed=")) settings.last_seed = unescape_text(read_value(line));
        else if (starts_with(line, "last_project_id=")) settings.last_project_id = unescape_text(read_value(line));
        else if (starts_with(line, "atlas_query=")) settings.atlas_query = unescape_text(read_value(line));
        else if (starts_with(line, "last_screen=")) settings.last_screen = unescape_text(read_value(line));
        else if (starts_with(line, "window_width=")) settings.window_width = parse_int(read_value(line), settings.window_width);
        else if (starts_with(line, "window_height=")) settings.window_height = parse_int(read_value(line), settings.window_height);
        else if (starts_with(line, "gpu_bloom=")) settings.gpu_bloom = parse_int(read_value(line), settings.gpu_bloom ? 1 : 0) != 0;
        else if (starts_with(line, "recent_project=")) settings.recent_project_ids.push_back(unescape_text(read_value(line)));
    }
    return settings;
}

void save_settings(const PersistentAppSettings& settings) {
    ensure_storage_dirs();
    std::ostringstream out;

    write_key_value(out, "last_seed", settings.last_seed);
    write_key_value(out, "last_project_id", settings.last_project_id);
    write_key_value(out, "atlas_query", settings.atlas_query);
    write_key_value(out, "last_screen", settings.last_screen);
    write_key_value(out, "window_width", settings.window_width);
    write_key_value(out, "window_height", settings.window_height);
    write_key_value(out, "gpu_bloom", settings.gpu_bloom);
    for (const std::string& id : settings.recent_project_ids) {
        write_key_value(out, "recent_project", id);
    }

    commit_atomic(settings_path(), out.str());
}

std::filesystem::path cosmos_root() {
    return data_root() / "cosmos";
}

bool save_cosmos_bookmark(const CosmosBookmark& bookmark) {
    if (bookmark.id.empty()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(cosmos_root(), ec);

    std::ostringstream out;
    write_key_value(out, "id", bookmark.id);
    write_key_value(out, "title", bookmark.title);
    write_key_value(out, "seed", bookmark.seed);
    write_key_value(out, "scale_index", bookmark.scale_index);
    write_key_value(out, "steps", bookmark.steps);
    write_key_value(out, "created_at", bookmark.created_at);

    return commit_atomic(cosmos_root() / (bookmark.id + ".cosmos"), out.str());
}

std::vector<CosmosBookmark> load_cosmos_bookmarks() {
    std::vector<CosmosBookmark> bookmarks;
    std::error_code ec;
    std::filesystem::create_directories(cosmos_root(), ec);
    if (ec) {
        return bookmarks;
    }

    for (const auto& entry : std::filesystem::directory_iterator(cosmos_root(), ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cosmos") {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in.is_open()) {
            continue;
        }
        CosmosBookmark b;
        std::string line;
        while (std::getline(in, line)) {
            if (starts_with(line, "id=")) b.id = unescape_text(read_value(line));
            else if (starts_with(line, "title=")) b.title = unescape_text(read_value(line));
            else if (starts_with(line, "seed=")) b.seed = unescape_text(read_value(line));
            else if (starts_with(line, "scale_index=")) b.scale_index = parse_int(read_value(line), b.scale_index);
            else if (starts_with(line, "steps=")) b.steps = parse_int(read_value(line), b.steps);
            else if (starts_with(line, "created_at=")) b.created_at = unescape_text(read_value(line));
        }
        if (!b.id.empty()) {
            bookmarks.push_back(std::move(b));
        }
    }

    std::sort(bookmarks.begin(), bookmarks.end(),
              [](const CosmosBookmark& lhs, const CosmosBookmark& rhs) {
                  return lhs.created_at > rhs.created_at;
              });
    return bookmarks;
}

bool delete_cosmos_bookmark(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::remove(cosmos_root() / (id + ".cosmos"), ec) && !ec;
}

} // namespace Storage
