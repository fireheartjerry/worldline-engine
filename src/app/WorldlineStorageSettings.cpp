#include "app/WorldlineStorageInternal.hpp"

namespace Storage {

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

    // Same-second saves would otherwise land in directory-iteration order,
    // which is unspecified — tie-break on id for a stable listing.
    std::sort(bookmarks.begin(), bookmarks.end(),
              [](const CosmosBookmark& lhs, const CosmosBookmark& rhs) {
                  if (lhs.created_at != rhs.created_at) return lhs.created_at > rhs.created_at;
                  return lhs.id < rhs.id;
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
