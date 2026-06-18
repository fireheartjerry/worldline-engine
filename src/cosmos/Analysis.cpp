#include "cosmos/Analysis.hpp"

#include <cmath>

namespace cosmos {

std::vector<ConstituentRef> resolve_constituents(
    const std::vector<UniverseObject>& catalog, const UniverseObject& object) {
    std::vector<ConstituentRef> refs;
    refs.reserve(object.constituents.size());
    for (const std::string& cid : object.constituents) {
        const UniverseObject* found = find_object(catalog, cid);
        ConstituentRef ref;
        ref.id = cid;
        ref.name = found ? found->name : cid;
        ref.object = found;
        refs.push_back(std::move(ref));
    }
    return refs;
}

ObjectComparison compare_objects(const UniverseObject& a, const UniverseObject& b) {
    ObjectComparison c;
    const double mb = (b.generated_mass != 0.0) ? b.generated_mass : 1.0e-300;
    c.mass_ratio = a.generated_mass / mb;
    c.mass_orders = static_cast<int>(std::lround(
        std::log10(std::max(a.generated_mass, 1.0e-300)) -
        std::log10(std::max(b.generated_mass, 1.0e-300))));
    const double rb = (b.radius_m != 0.0) ? b.radius_m : 1.0e-300;
    c.radius_ratio = a.radius_m / rb;
    c.stability_delta = a.stability - b.stability;
    c.abundance_delta = a.abundance - b.abundance;
    c.binding_delta = a.binding - b.binding;
    return c;
}

} // namespace cosmos
