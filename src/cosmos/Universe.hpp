#pragma once

#include "cosmos/LawGenome.hpp"
#include "cosmos/ObjectCatalog.hpp"

#include <string>
#include <vector>

namespace cosmos {

// Version of the generation pipeline. Bumped when the deterministic output of
// generate_universe changes, so saved artifacts can be reasoned about.
constexpr int kGenerationVersion = 2;

// A sci-fi "dossier" classifying a universe from its law genome.
struct UniverseClassification {
    std::string class_name;          // e.g. "Hadron-Dominant Cosmos"
    std::string codename;            // e.g. "WL-A3F2"
    std::vector<std::string> traits; // human-readable distinguishing properties
    double complexity = 0.0;         // [0,1] potential for rich emergent structure
};

UniverseClassification classify_universe(const LawGenome& genome);

// A per-universe color identity that drives the backdrop, nebulae and accents,
// so each universe looks unmistakably its own. Deterministic from the genome.
struct UniversePalette {
    Color8 void_top;     // background gradient top
    Color8 void_bottom;  // background gradient bottom
    Color8 nebula_a;     // primary nebula bloom
    Color8 nebula_b;     // secondary nebula bloom
    Color8 star;         // starfield tint
    Color8 accent;       // bright primary accent
    double star_density = 0.5; // [0,1]
    double primary_hue = 200.0;
};

UniversePalette derive_palette(const LawGenome& genome, const UniverseClassification& cls);

// A fully generated, self-consistent universe: the single source of truth that
// the Explorer and sandboxes are built from.
struct Universe {
    std::string seed;
    int generation_version = kGenerationVersion;
    LawGenome genome;
    UniverseClassification classification;
    UniversePalette palette;
    std::vector<UniverseObject> catalog; // genome-specialized
};

// Deterministically generate a complete, validated universe from a seed.
Universe generate_universe(const std::string& seed);

// Result of checking a universe against every internal invariant.
struct ValidationReport {
    bool ok = true;
    std::vector<std::string> issues;
};

// Verify genome bounds/finiteness, classification completeness, and full catalog
// integrity (finite + bounded generated/sandbox fields, resolvable constituents).
ValidationReport validate_universe(const Universe& universe);

} // namespace cosmos
