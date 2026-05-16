#include "app/WorldlineCopy.hpp"

#include <cstring>

namespace Copy {

const std::vector<GlossaryEntry>& glossary_entries() {
    static const std::vector<GlossaryEntry> entries{
        {
            "Seed",
            "The input string that deterministically defines a generated universe.",
            "A seed is the only user-provided source value. The entire generator pipeline, the assembled law tensors, and the observed pendulum behaviour are deterministic functions of it."
        },
        {
            "Generator",
            "The deterministic pipeline that expands the seed and assembles the law tensors.",
            "Worldline keeps the self-modifying machine, Fibonacci rotation, tensor assembly, and simplex-based construction intact. This screen focuses on what those stages produce rather than hiding the mechanism."
        },
        {
            "Law Assembly",
            "The step where generated tensors become an equation of motion.",
            "The assembled MetaSpec is passed into LawSpec, which defines a bounded phase-space flow and the rule used to advance the generated universe over time."
        },
        {
            "Equation of Motion",
            "The mathematical rule that advances the generated state every frame.",
            "In seeded mode the simulation is not driven by the Newtonian pendulum integrator. LawSpec advances the internal law state, and the renderer only visualizes the observed pendulum output."
        },
        {
            "Observable Mapping",
            "The mapping from the internal law state to pendulum angles and angular velocities.",
            "ObservableExtractor turns the hidden generated state into the pendulum motion shown on screen. This is why the renderer can stay familiar while the governing law changes completely."
        },
        {
            "Reference System",
            "The baseline Newtonian double-pendulum model used for comparison.",
            "The reference system keeps the classic pendulum integrator and direct parameter editing. It exists as a known baseline for comparison, not as the flagship scene."
        },
        {
            "Atlas",
            "The local library of saved universes.",
            "The atlas stores named workspace snapshots with notes, derived metrics, timeline markers, and a compact fingerprint so saved universes can be searched and compared."
        },
        {
            "Trace",
            "The detailed inspection view for the generated pipeline and law preview.",
            "Trace exposes the deterministic build path, generated matrices, preview orbit, and glossary without crowding the main workspace."
        }
    };
    return entries;
}

const GlossaryEntry* find_glossary_entry(const char* term) {
    const auto& entries = glossary_entries();
    for (const GlossaryEntry& entry : entries) {
        if (std::strcmp(entry.term, term) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace Copy
