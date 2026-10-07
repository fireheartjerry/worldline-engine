// Persistence edge cases: escaping round-trips for hostile strings, exact
// double round-trips, CRLF/hand-edited files, id handling (copied files, path
// traversal), catalog queries, settings sanitizing, and an unusable data root.
// Every test runs in a private data directory (WORLDLINE_DATA_DIR).

#include "app/WorldlineStorage.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#define WL_GETPID _getpid
#else
#include <unistd.h>
#define WL_GETPID getpid
#endif

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "app_storage_verification failed: " << message << '\n';
        ++g_failures;
    }
}

void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

bool same_bits(double a, double b) {
    std::uint64_t ua = 0;
    std::uint64_t ub = 0;
    std::memcpy(&ua, &a, sizeof(double));
    std::memcpy(&ub, &b, sizeof(double));
    return ua == ub;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file(const fs::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
}

// Strings that stress the key=value line format: separators, escapes, line
// breaks, a trailing backslash, UTF-8, an embedded NUL and a 1 MB blob.
std::vector<std::string> hostile_strings() {
    std::vector<std::string> v = {
        "",
        "plain",
        "key=value=more",
        "pipe|separated|label",
        "back\\slash",
        "trailing backslash\\",
        "\\n literal, not a newline",
        "line one\nline two\n",
        "windows\r\nline\r\nendings\r\n",
        "lone\rcarriage return",
        "\ttabbed\t",
        "  leading and trailing spaces  ",
        "Ünïcødé ✨ 宇宙 🌌",
        std::string("nul\0inside", 10),
    };
    v.push_back(std::string(1 << 20, 'x') + "\n" + std::string(1000, '\\'));
    return v;
}

void test_project_text_round_trip() {
    const std::vector<std::string> strings = hostile_strings();
    for (std::size_t i = 0; i < strings.size(); ++i) {
        const std::string& s = strings[i];
        UniverseProject p;
        p.id = "text-" + std::to_string(i);
        p.seed = s;
        p.title = s;
        p.notes = s + s;
        p.descriptor = s;
        p.created_at = "2026-01-01 00:00:00";
        p.updated_at = "2026-01-01 00:00:00";
        p.workspace.title = s;
        p.workspace.notes = s;
        p.search_tags = {s, "tag=with=equals", s};
        TimelineMarker m;
        m.label = s;
        m.time = 1.25;
        m.snapshot_index = 3;
        m.pinned = true;
        p.markers.push_back(m);
        check(Storage::save_project(p), "save must succeed for hostile string #" + std::to_string(i));

        UniverseProject q;
        check(Storage::load_project(p.id, q), "load must succeed for hostile string #" + std::to_string(i));
        const std::string tag = " (hostile string #" + std::to_string(i) + ")";
        check(q.seed == p.seed, "seed must round-trip exactly" + tag);
        check(q.title == p.title, "title must round-trip exactly" + tag);
        check(q.notes == p.notes, "notes must round-trip exactly" + tag);
        check(q.descriptor == p.descriptor, "descriptor must round-trip exactly" + tag);
        check(q.workspace.title == p.workspace.title && q.workspace.notes == p.workspace.notes,
              "workspace text must round-trip exactly" + tag);
        check(q.search_tags == p.search_tags, "tags must round-trip exactly" + tag);
        check(q.markers.size() == 1 && q.markers[0].label == s && q.markers[0].snapshot_index == 3 &&
                  q.markers[0].pinned,
              "marker (label may contain '|') must round-trip exactly" + tag);
    }

    // The file must stay strictly one record per line: no raw CR/LF from values.
    const std::string raw = read_file(Storage::projects_root() / "text-8.wline");
    check(raw.find('\r') == std::string::npos, "no raw carriage return may reach the file");
}

void test_project_numeric_round_trip() {
    const double values[] = {
        0.0,
        -0.0,
        0.1,
        1.0 / 3.0,
        -2.718281828459045,
        1.0e-300,
        std::numeric_limits<double>::denorm_min(), // subnormal: std::stod rejects these
        2.5e-310,
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        6.02214076e23,
    };
    UniverseProject p;
    p.id = "numeric";
    p.seed = "numeric";
    for (double v : values) {
        p.thumbnail_points.push_back({v, -v});
        TimelineMarker m;
        m.time = v;
        m.snapshot_index = 1;
        m.label = "m";
        p.markers.push_back(m);
    }
    p.seeded_p = 1.0 / 7.0;
    p.linear_gain = std::numeric_limits<double>::denorm_min();
    p.accel_ceiling = 12.000000000000002;
    p.p_min = -0.0;
    p.p_max = 1.0e308;
    check(Storage::save_project(p), "numeric project must save");

    UniverseProject q;
    check(Storage::load_project("numeric", q), "numeric project must load");
    check(q.thumbnail_points.size() == p.thumbnail_points.size(), "every thumbnail point must survive");
    check(q.markers.size() == p.markers.size(), "every marker must survive");
    for (std::size_t i = 0; i < q.thumbnail_points.size() && i < p.thumbnail_points.size(); ++i) {
        check(same_bits(q.thumbnail_points[i].x, p.thumbnail_points[i].x) &&
                  same_bits(q.thumbnail_points[i].y, p.thumbnail_points[i].y),
              "thumbnail point " + std::to_string(i) + " must round-trip bit-exactly");
    }
    for (std::size_t i = 0; i < q.markers.size() && i < p.markers.size(); ++i) {
        check(same_bits(q.markers[i].time, p.markers[i].time),
              "marker time " + std::to_string(i) + " must round-trip bit-exactly");
    }
    check(same_bits(q.seeded_p, p.seeded_p) && same_bits(q.linear_gain, p.linear_gain) &&
              same_bits(q.accel_ceiling, p.accel_ceiling) && same_bits(q.p_min, p.p_min) &&
              same_bits(q.p_max, p.p_max),
          "scalar fields must round-trip bit-exactly");
}

void test_crlf_file_is_tolerated() {
    // A project hand-edited in a Windows editor gets CRLF line endings. The CR
    // must not leak into values (a seed "alpha\r" is a different universe).
    write_file(Storage::projects_root() / "crlf.wline",
               "id=crlf\r\nseed=alpha\r\ntitle=Edited on Windows\r\nseeded_p=0.5\r\n"
               "marker=1.5|2|1|label\r\ntag=t1\r\n");
    UniverseProject q;
    check(Storage::load_project("crlf", q), "CRLF project must load");
    check(q.seed == "alpha", "CRLF line ending must not leak into the seed");
    check(q.title == "Edited on Windows", "CRLF line ending must not leak into the title");
    check(q.seeded_p == 0.5, "numeric field must parse from a CRLF line");
    check(q.markers.size() == 1 && q.markers[0].label == "label", "marker label must not keep the CR");
    check(q.search_tags.size() == 1 && q.search_tags[0] == "t1", "tag must not keep the CR");
}

void test_file_name_is_the_project_id() {
    // Simulate the user duplicating a project file by hand. Saving the copy
    // must write the copy, never overwrite the original.
    UniverseProject original;
    original.id = "orig";
    original.seed = "original-seed";
    original.title = "Original";
    check(Storage::save_project(original), "original must save");
    fs::copy_file(Storage::projects_root() / "orig.wline", Storage::projects_root() / "copy.wline",
                  fs::copy_options::overwrite_existing);

    UniverseProject copy;
    check(Storage::load_project("copy", copy), "copied file must load");
    check(copy.id == "copy", "a copied file's id must be its file name, not the stored id");
    check(copy.workspace.project_id == "copy", "workspace id must follow the file name");
    copy.title = "Edited copy";
    check(Storage::save_project(copy), "copy must save");

    UniverseProject reread;
    check(Storage::load_project("orig", reread) && reread.title == "Original",
          "saving the copy must not overwrite the original project");

    const CatalogIndex catalog = Storage::load_catalog();
    int orig_entries = 0;
    for (const UniverseProject& p : catalog.projects) {
        if (p.id == "orig") ++orig_entries;
    }
    check(orig_entries == 1, "the catalog must not list two projects with the same id");

    // A file with no id line at all is not a project file.
    write_file(Storage::projects_root() / "no-id.wline", "seed=x\n");
    UniverseProject none;
    check(!Storage::load_project("no-id", none), "a file without an id line must be rejected");
}

void test_unsafe_ids_are_rejected() {
    UniverseProject p;
    p.seed = "s";
    for (const char* bad : {"", ".", "..", "../escape", "a/b", "a\\b", "c:evil", "new\nline"}) {
        p.id = bad;
        check(!Storage::save_project(p), std::string("save must reject unsafe id '") + bad + "'");
        UniverseProject q;
        check(!Storage::load_project(bad, q), std::string("load must reject unsafe id '") + bad + "'");
        CosmosBookmark b;
        b.id = bad;
        check(!Storage::save_cosmos_bookmark(b), std::string("bookmark save must reject '") + bad + "'");
        check(!Storage::delete_cosmos_bookmark(bad), std::string("bookmark delete must reject '") + bad + "'");
    }
    check(!fs::exists(Storage::data_root() / "escape.wline"), "no file may be written outside projects/");

    // Ids produced by the app itself are always accepted.
    p.id = Storage::make_project_id("Weird Seed !! ✨ name");
    check(Storage::save_project(p), "generated id '" + p.id + "' must be accepted");
}

void test_make_project_id() {
    const std::string a = Storage::make_project_id("Hello World_x-y");
    check(a.rfind("hello-world-x-y-", 0) == 0, "seed fragment must be lowercased and dash-joined: " + a);
    const std::string b = Storage::make_project_id("!!!");
    check(b.rfind("universe-", 0) == 0, "an all-symbol seed must fall back to 'universe': " + b);
    const std::string c = Storage::make_project_id("");
    check(c.rfind("universe-", 0) == 0, "an empty seed must fall back to 'universe': " + c);
    const std::string d = Storage::make_project_id("trailing---");
    check(d.find("---") == std::string::npos, "trailing separators must be trimmed: " + d);
}

void test_catalog_query() {
    // Fresh directory so the catalog contents are known exactly.
    for (const auto& entry : fs::directory_iterator(Storage::projects_root())) {
        fs::remove(entry.path());
    }
    UniverseProject a;
    a.id = "q-a";
    a.seed = "Nebula-Seven";
    a.title = "Spiral Arms";
    a.updated_at = "2026-01-02 00:00:00";
    a.search_tags = {"dynamic p"};
    UniverseProject b;
    b.id = "q-b";
    b.seed = "quiet";
    b.title = "Ünïcødé Title";
    b.notes = "multi\nline notes";
    b.updated_at = "2026-01-03 00:00:00";
    UniverseProject c = b;
    c.id = "q-c";
    c.seed = "same-time";
    c.title = "Tie";
    c.notes.clear();
    check(Storage::save_project(a) && Storage::save_project(b) && Storage::save_project(c),
          "catalog projects must save");

    const CatalogIndex catalog = Storage::load_catalog();
    check(catalog.projects.size() == 3, "catalog must list exactly the saved projects");
    if (catalog.projects.size() == 3) {
        check(catalog.projects[0].id == "q-b" && catalog.projects[1].id == "q-c" &&
                  catalog.projects[2].id == "q-a",
              "catalog must be newest-first with an id tie-break");
    }
    auto count = [&](const std::string& text) { return Storage::query_catalog(catalog, {text}).size(); };
    check(count("") == 3, "an empty query must match everything");
    check(count("NEBULA") == 1, "query must be case-insensitive");
    check(count("spiral arms") == 1, "query must match titles");
    check(count("dynamic p") == 1, "query must match tags");
    // Case folding is ASCII-only (C locale); non-ASCII bytes match verbatim.
    check(count("Ünïcødé TITLE") == 1, "query must match UTF-8 text, folding only ASCII case");
    check(count("line notes") == 1, "query must match notes");
    check(count("zzz-not-there") == 0, "a non-matching query must match nothing");
    check(count("q-") == 3, "query must match ids");
}

void test_settings_sanitizing() {
    PersistentAppSettings s;
    s.last_seed = "seed\nwith\\escapes\r";
    s.atlas_query = "a=b|c";
    s.recent_project_ids = {"one", "", "three"};
    s.window_width = 1600;
    s.window_height = 900;
    s.gpu_bloom = false;
    Storage::save_settings(s);
    PersistentAppSettings r = Storage::load_settings();
    check(r.last_seed == s.last_seed, "seed with escapes must round-trip through settings");
    check(r.atlas_query == s.atlas_query, "atlas query must round-trip");
    check(r.recent_project_ids == s.recent_project_ids, "recent ids (including empty) must round-trip");
    check(!r.gpu_bloom, "gpu_bloom=false must round-trip");

    const PersistentAppSettings defaults;
    const char* bad_sizes[][2] = {{"0", "0"}, {"-800", "-600"}, {"100000", "50"}, {"1e9", "nan"}};
    for (const auto& size : bad_sizes) {
        write_file(Storage::settings_path(), std::string("window_width=") + size[0] + "\nwindow_height=" +
                                                 size[1] + "\n");
        r = Storage::load_settings();
        check(r.window_width == defaults.window_width && r.window_height == defaults.window_height,
              std::string("implausible window size ") + size[0] + "x" + size[1] +
                  " must fall back to the default");
    }
    write_file(Storage::settings_path(), "window_width=1024\r\nwindow_height=768\r\n");
    r = Storage::load_settings();
    check(r.window_width == 1024 && r.window_height == 768, "plausible sizes (CRLF file) must be kept");
}

void test_bookmarks_edge_cases() {
    CosmosBookmark b;
    b.id = "bm-edge";
    b.title = "Title with\nnewline | pipe = equals ✨";
    b.seed = "seed\twith\ttabs\\";
    b.scale_index = 3;
    b.steps = 123456;
    b.created_at = "2026-02-02 02:02:02";
    check(Storage::save_cosmos_bookmark(b), "edge bookmark must save");
    bool found = false;
    for (const CosmosBookmark& m : Storage::load_cosmos_bookmarks()) {
        if (m.id == "bm-edge") {
            found = true;
            check(m.title == b.title && m.seed == b.seed && m.scale_index == 3 && m.steps == 123456,
                  "bookmark text/number fields must round-trip exactly");
        }
    }
    check(found, "edge bookmark must load back");

    // A renamed bookmark file must be listed (and deletable) under its file name.
    fs::rename(Storage::cosmos_root() / "bm-edge.cosmos", Storage::cosmos_root() / "bm-renamed.cosmos");
    bool listed = false;
    for (const CosmosBookmark& m : Storage::load_cosmos_bookmarks()) {
        if (m.id == "bm-renamed") listed = true;
    }
    check(listed, "a renamed bookmark must be listed under its file name");
    check(Storage::delete_cosmos_bookmark("bm-renamed"), "a renamed bookmark must be deletable");
    check(!fs::exists(Storage::cosmos_root() / "bm-renamed.cosmos"), "deleted bookmark file must be gone");
}

void test_unusable_data_root() {
    // Point the data root at a regular file: nothing can be created under it.
    // Every entry point must degrade gracefully instead of throwing (the app
    // calls load_settings() before it even opens a window).
    const fs::path file_root = Storage::data_root() / "not-a-directory";
    write_file(file_root, "x");
    const std::string saved_root = Storage::data_root().string();
    set_env("WORLDLINE_DATA_DIR", file_root.string());
    try {
        check(!Storage::ensure_storage_dirs(), "ensure_storage_dirs must report failure");
        const PersistentAppSettings s = Storage::load_settings();
        check(s.window_width == PersistentAppSettings{}.window_width, "settings must fall back to defaults");
        Storage::save_settings(s); // must not throw
        check(Storage::load_catalog().projects.empty(), "catalog must be empty");
        UniverseProject p;
        p.id = "x";
        check(!Storage::save_project(p), "save must fail cleanly");
        check(!Storage::load_project("x", p), "load must fail cleanly");
        CosmosBookmark b;
        b.id = "x";
        check(!Storage::save_cosmos_bookmark(b), "bookmark save must fail cleanly");
        check(Storage::load_cosmos_bookmarks().empty(), "bookmark list must be empty");
    } catch (const std::exception& e) {
        check(false, std::string("storage must not throw on an unusable data root: ") + e.what());
    }
    set_env("WORLDLINE_DATA_DIR", saved_root);
}

} // namespace

int main() {
    const fs::path sandbox = fs::temp_directory_path() /
                             ("worldline-storage-test-" + std::to_string(static_cast<long long>(WL_GETPID())));
    fs::remove_all(sandbox);
    set_env("WORLDLINE_DATA_DIR", sandbox.string());
    check(Storage::ensure_storage_dirs(), "a fresh data root must be creatable");

    test_project_text_round_trip();
    test_project_numeric_round_trip();
    test_crlf_file_is_tolerated();
    test_file_name_is_the_project_id();
    test_unsafe_ids_are_rejected();
    test_make_project_id();
    test_catalog_query();
    test_settings_sanitizing();
    test_bookmarks_edge_cases();
    test_unusable_data_root();

    std::error_code ec;
    fs::remove_all(sandbox, ec);
    if (g_failures > 0) {
        std::cerr << g_failures << " storage check(s) failed\n";
        return 1;
    }
    return 0;
}
