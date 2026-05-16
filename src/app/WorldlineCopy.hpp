#pragma once

#include "app/UniverseModel.hpp"

#include <vector>

namespace Copy {

inline constexpr const char* kAppTitle = "Worldline Universe Studio";
inline constexpr const char* kAppSubtitle =
    "Deterministic universe generation, live law simulation, and a reference pendulum model.";
inline constexpr const char* kReferenceLabel = "Reference System";
inline constexpr const char* kTraceLabel = "Trace";
inline constexpr const char* kAtlasLabel = "Atlas";

const std::vector<GlossaryEntry>& glossary_entries();
const GlossaryEntry* find_glossary_entry(const char* term);

} // namespace Copy
