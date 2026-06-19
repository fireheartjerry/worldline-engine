#pragma once

#include "cosmos/ObjectCatalog.hpp"

#include <string>
#include <vector>

namespace cosmos {

struct ConstituentRef {
    std::string id;
    std::string name;            // resolved display name (id if unresolved)
    const UniverseObject* object; // nullptr if not in the catalog
};

// Resolve an object's constituent ids to catalog entries (for the "what is this
// built from" view, with navigation).
std::vector<ConstituentRef> resolve_constituents(
    const std::vector<UniverseObject>& catalog, const UniverseObject& object);

struct ObjectComparison {
    double mass_ratio = 1.0;        // a.generated_mass / b.generated_mass
    int mass_orders = 0;            // round(log10(mass_ratio))
    double radius_ratio = 1.0;
    double stability_delta = 0.0;   // a.stability - b.stability
    double abundance_delta = 0.0;
    double binding_delta = 0.0;
};

// Compare two objects (typically across or within scales).
ObjectComparison compare_objects(const UniverseObject& a, const UniverseObject& b);

} // namespace cosmos
