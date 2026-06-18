#include "cosmos/Universe.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cosmos {
namespace {

double norm(double v, double lo, double hi) {
    return std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
}

bool finite_in(double v, double lo, double hi) {
    return std::isfinite(v) && v >= lo - 1.0e-9 && v <= hi + 1.0e-9;
}

std::string codename_from(std::uint64_t signature) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "WL-%04X",
                  static_cast<unsigned>((signature >> 24) & 0xFFFFull));
    return buf;
}

} // namespace

UniverseClassification classify_universe(const LawGenome& g) {
    const double strongN = norm(g.coupling_strong, 0.35, 1.80);
    const double emN = norm(g.coupling_em, 0.35, 1.80);
    const double gravN = norm(g.coupling_gravity, 0.40, 1.70);
    const double driftN = norm(g.cosmological_drift, 0.70, 1.50);
    const double massN = norm(g.mass_scale, 0.60, 1.60);
    const double stabN = norm(g.stability_bias, 0.15, 0.90);

    UniverseClassification c;
    c.codename = codename_from(g.signature);

    // The dominant deviation names the universe.
    struct Cand { const char* name; double score; };
    const Cand cands[] = {
        {"Hadron-Dominant Cosmos", strongN},
        {"Gravitational Crucible", gravN},
        {"Electrostatic Lattice", emN},
        {"Diffuse Expanse", driftN},
        {"Dense Aggregate", massN},
        {"Sparse Void", 1.0 - massN},
    };
    const Cand* best = &cands[0];
    for (const Cand& cand : cands) {
        if (cand.score > best->score) {
            best = &cand;
        }
    }
    c.class_name = (best->score >= 0.62) ? best->name : "Balanced Continuum";

    // Distinguishing traits (only the notable extremes).
    if (strongN > 0.62) c.traits.push_back("Strong nuclear binding");
    else if (strongN < 0.33) c.traits.push_back("Weak nuclear force");
    if (emN > 0.62) c.traits.push_back("Intense electromagnetism");
    if (gravN > 0.62) c.traits.push_back("Heavy gravitation");
    else if (gravN < 0.33) c.traits.push_back("Feeble gravitation");
    if (massN > 0.62) c.traits.push_back("Heavy matter content");
    else if (massN < 0.33) c.traits.push_back("Light matter content");
    if (driftN > 0.62) c.traits.push_back("Rapid metric expansion");
    else if (driftN < 0.33) c.traits.push_back("Quasi-static metric");
    if (stabN > 0.62) c.traits.push_back("High structural persistence");
    else if (stabN < 0.33) c.traits.push_back("Volatile structures");

    const double e = g.gravity_exponent;
    c.traits.push_back(std::abs(e - 2.0) < 0.06 ? "Inverse-square gravitation"
                       : (e > 2.0)              ? "Steep gravitational falloff"
                                                : "Shallow gravitational falloff");
    if (c.traits.empty()) {
        c.traits.push_back("Unremarkable constants");
    }

    // Complexity: balanced constants, persistent structures, and a metric that
    // neither collapses nor runs away all raise the potential for rich emergence.
    auto centered = [](double n) { return 1.0 - std::min(1.0, std::abs(n - 0.45) * 2.0); };
    const double balance = (centered(strongN) + centered(emN) + centered(gravN)) / 3.0;
    c.complexity = std::clamp(
        0.55 * balance + 0.30 * stabN + 0.15 * (1.0 - std::abs(driftN - 0.35)), 0.0, 1.0);
    return c;
}

namespace {

Color8 hsv8(double h, double s, double v) {
    h = std::fmod(std::fmod(h, 360.0) + 360.0, 360.0);
    s = std::clamp(s, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);
    const double c = v * s;
    const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;
    double r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    auto to8 = [&](double ch) {
        return static_cast<unsigned char>(std::clamp((ch + m) * 255.0, 0.0, 255.0));
    };
    return {to8(r), to8(g), to8(b)};
}

} // namespace

UniversePalette derive_palette(const LawGenome& g, const UniverseClassification& cls) {
    const double strongN = norm(g.coupling_strong, 0.35, 1.80);
    const double emN = norm(g.coupling_em, 0.35, 1.80);
    const double gravN = norm(g.coupling_gravity, 0.40, 1.70);
    const double driftN = norm(g.cosmological_drift, 0.70, 1.50);

    // Each dominant force pulls the universe toward a signature hue; the genome
    // signature adds a per-universe rotation so no two feel identical.
    const double base_hue = 25.0 * strongN     // strong -> ember orange
                          + 195.0 * emN         // em -> cyan
                          + 280.0 * gravN       // gravity -> violet
                          + 160.0 * driftN;     // drift -> teal
    const double weight = strongN + emN + gravN + driftN + 1.0e-6;
    double hue = base_hue / weight;
    hue += static_cast<double>((g.signature >> 8) & 0x3F); // up to +63 deg jitter

    UniversePalette p;
    p.primary_hue = std::fmod(hue, 360.0);
    const double sec = p.primary_hue + 145.0;

    p.void_top = hsv8(p.primary_hue, 0.55, 0.045);
    p.void_bottom = hsv8(sec, 0.45, 0.03);
    p.nebula_a = hsv8(p.primary_hue, 0.70, 0.55);
    p.nebula_b = hsv8(sec, 0.65, 0.45);
    p.star = hsv8(p.primary_hue, 0.18, 1.0);
    p.accent = hsv8(p.primary_hue, 0.75, 1.0);
    p.star_density = std::clamp(0.35 + 0.6 * cls.complexity, 0.0, 1.0);
    return p;
}

Universe generate_universe(const std::string& seed) {
    Universe u;
    u.seed = seed.empty() ? std::string("worldline") : seed;
    u.generation_version = kGenerationVersion;
    u.genome = generate_law_genome(u.seed);
    u.classification = classify_universe(u.genome);
    u.palette = derive_palette(u.genome, u.classification);
    u.catalog = build_object_catalog();
    apply_law_genome(u.catalog, u.genome);
    return u;
}

ValidationReport validate_universe(const Universe& u) {
    ValidationReport report;
    auto fail = [&](const std::string& msg) {
        report.ok = false;
        report.issues.push_back(msg);
    };

    const LawGenome& g = u.genome;
    if (!finite_in(g.coupling_strong, 0.35, 1.80)) fail("coupling_strong out of range");
    if (!finite_in(g.coupling_em, 0.35, 1.80)) fail("coupling_em out of range");
    if (!finite_in(g.coupling_gravity, 0.40, 1.70)) fail("coupling_gravity out of range");
    if (!finite_in(g.gravity_exponent, 1.75, 2.25)) fail("gravity_exponent out of range");
    if (!finite_in(g.mass_scale, 0.60, 1.60)) fail("mass_scale out of range");
    if (!finite_in(g.cosmological_drift, 0.70, 1.50)) fail("cosmological_drift out of range");
    if (!finite_in(g.stability_bias, 0.15, 0.90)) fail("stability_bias out of range");

    if (u.classification.class_name.empty()) fail("classification has no class name");
    if (u.classification.codename.empty()) fail("classification has no codename");
    if (!finite_in(u.classification.complexity, 0.0, 1.0)) fail("complexity out of range");

    if (u.catalog.empty()) {
        fail("catalog is empty");
        return report;
    }
    for (const UniverseObject& o : u.catalog) {
        if (!finite_in(o.sim_mass, 0.8, 5.0)) fail("sim_mass out of range: " + o.id);
        if (!finite_in(o.sim_radius, 0.3, 1.1)) fail("sim_radius out of range: " + o.id);
        if (!std::isfinite(o.sim_charge)) fail("sim_charge not finite: " + o.id);
        if (!finite_in(o.stability, 0.0, 1.0)) fail("stability out of range: " + o.id);
        if (!finite_in(o.abundance, 0.0, 1.0)) fail("abundance out of range: " + o.id);
        if (!(std::isfinite(o.generated_mass) && o.generated_mass > 0.0))
            fail("generated_mass invalid: " + o.id);
        if (!(std::isfinite(o.mass_factor) && o.mass_factor > 0.0))
            fail("mass_factor invalid: " + o.id);
        if (!(std::isfinite(o.temperature) && o.temperature > 0.0))
            fail("temperature invalid: " + o.id);
        if (o.epoch.empty()) fail("epoch missing: " + o.id);
        for (const std::string& cid : o.constituents) {
            if (find_object(u.catalog, cid) == nullptr)
                fail("unresolved constituent '" + cid + "' in " + o.id);
        }
    }
    return report;
}

} // namespace cosmos
