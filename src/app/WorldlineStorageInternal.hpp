#pragma once
// WorldlineStorage — shared serialization helpers (escaping, key/value I/O,
// parsing, atomic commit). Used by both storage translation units.
#include "app/WorldlineStorage.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace Storage {

inline std::string escape_text(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char ch : text) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break; // escaped, so a raw CR never reaches the file
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

inline std::string unescape_text(const std::string& text) {
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
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case '\\': out.push_back('\\'); break;
        default: out.push_back(ch); break;
        }
        escaping = false;
    }
    return out;
}

inline bool starts_with(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

inline std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline void write_key_value(std::ostream& out,
                     const char* key,
                     const std::string& value) {
    out << key << '=' << escape_text(value) << '\n';
}

inline void write_key_value(std::ostream& out,
                     const char* key,
                     double value) {
    out << key << '=' << std::setprecision(17) << value << '\n';
}

inline void write_key_value(std::ostream& out,
                     const char* key,
                     int value) {
    out << key << '=' << value << '\n';
}

inline void write_key_value(std::ostream& out,
                     const char* key,
                     bool value) {
    out << key << '=' << (value ? 1 : 0) << '\n';
}

inline std::string sanitize_seed_fragment(const std::string& seed) {
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

// getline() that tolerates CRLF files (e.g. hand-edited in a Windows editor).
// The writer escapes every '\r', so a trailing raw CR is always a line-ending
// artifact; left in place it would silently become part of the value (a seed
// "alpha\r" generates a different universe than "alpha").
inline bool read_line(std::istream& in, std::string& line) {
    if (!std::getline(in, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

// Ids double as file names (projects/<id>.wline, cosmos/<id>.cosmos). Reject
// anything that is not a single plain path component so an id read back from a
// hand-edited settings file can never address a file outside the data root.
inline bool is_safe_id(const std::string& id) {
    if (id.empty() || id == "." || id == "..") {
        return false;
    }
    for (char ch : id) {
        if (ch == '/' || ch == '\\' || ch == ':' || static_cast<unsigned char>(ch) < 0x20) {
            return false;
        }
    }
    return true;
}

inline std::string read_value(const std::string& line) {
    const std::size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return {};
    }
    return line.substr(pos + 1);
}

// Saved files may be hand-edited, truncated by a crash mid-write, or carried
// over from an older schema. Parse numbers defensively so a single malformed
// field falls back to a sane default instead of throwing out of the loader.
inline double parse_double(const std::string& text, double fallback) {
    // strtod rather than std::stod: stod throws out_of_range on *underflow*
    // too, so a perfectly valid subnormal written by write_key_value() would
    // silently reset to the fallback on reload. Overflow still falls back.
    const char* begin = text.c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (end == begin) {
        return fallback;
    }
    if (errno == ERANGE && std::isinf(value)) {
        return fallback;
    }
    return value;
}

inline int parse_int(const std::string& text, int fallback) {
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
inline bool commit_atomic(const std::filesystem::path& path,
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

inline void append_search_blob(std::string& blob, const std::string& value) {
    blob += ' ';
    blob += lowercase(value);
}

inline const char* env_or_null(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}


} // namespace Storage
