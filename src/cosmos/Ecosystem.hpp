#pragma once
// Continuous trait-based ecosystem engine. A species is a point in a continuous
// trait space (NOT a discrete role); roles are soft labels derived from a
// continuous trophic level. A community is grown by adaptive radiation into the
// emptiest niche, wired into a food web by a continuous size-ratio x niche
// kernel, solved for biomass from NPP + Lindeman efficiency + Damuth abundance,
// and stabilized against May's criterion. step() runs a damped consumer-resource
// system so the ecosystem visibly "lives" without diverging. Deterministic:
// same (seed, biome) -> identical community.

#include "cosmos/ObjectCatalog.hpp" // Color8

#include <cstdint>
#include <string>
#include <vector>

namespace cosmos {
namespace eco {

struct BiomeParams {
    double temp_c = 18.0;     // mean surface temperature
    double precip_mm = 1200.0;
    double npp = 1200.0;      // net primary productivity, g/m^2/yr (caps the web)
    double area_m2 = 5.0e14;  // habitable area (for absolute populations)
    bool   aquatic = false;   // ocean biome -> higher transfer efficiency, different PPMR
};

// Continuous trait vector. Trophic level is realized from the diet (emergent
// omnivory); the categorical "role" is only ever a soft label of `tau`.
struct SpeciesTraits {
    double m = -2.0;      // log10 body mass (kg) — the master variable
    double tau = 0.0;     // continuous trophic level (0 producer .. ~4)
    double T_opt = 18.0;  // thermal optimum (C)
    double T_tol = 8.0;   // thermal tolerance half-width (C)
    double omega = 0.6;   // diet breadth (dex of the prey-size kernel)
    double dp = 2.0;      // preferred predator/prey log-mass offset (dex)
    double kappa = 1.0;   // metabolic scalar
    double rho = 0.5;     // r<->K position [0,1] (1 = fast/r)
    double phi = 0.0;     // diel phase [0,1) (0 diurnal, 0.5 nocturnal)
    double defense = 0.3; // anti-predator investment [0,1]
};

struct Species {
    SpeciesTraits t;
    double mass_kg = 0.0;
    double flux = 0.0;       // assimilated energy share (g/m^2/yr equiv.)
    double density = 0.0;    // individuals / m^2 (Damuth)
    double biomass = 0.0;    // kg / m^2
    double population = 0.0; // absolute count over the habitable area
    double x = 0.0;          // live-dynamics biomass-density state
    double xeq = 0.0;        // equilibrium target
    bool   detritivore = false;
    std::string name;
    Color8 color;            // continuous role blend
};

struct Link {
    int pred = 0;
    int prey = 0;
    double pref = 0.0;   // diet fraction (normalized per predator)
    double alpha = 0.0;  // interaction strength (for dynamics/stability)
};

struct CommunityStats {
    int    n_species = 0;
    int    n_links = 0;
    int    n_producers = 0;
    double connectance = 0.0;       // L / S^2
    double n_levels = 0.0;          // max realized trophic level
    double mean_chain = 0.0;        // mean realized tau of consumers
    double stability_margin = 0.0;  // 1 - sigma*sqrt(S*C); >0 stable
    double total_biomass = 0.0;     // kg/m^2
    int    keystone = -1;           // species with the largest removal impact
};

struct Community {
    std::vector<Species> species; // <= ~40
    std::vector<Link>    links;   // ~2S (canonical edge list; tests + UI iterate it)

    // CSR / forward-star predator->prey adjacency, built from `links` for a
    // tight, contiguous O(S+L) step() loop. csr_off has size S+1; the others L.
    std::vector<int>    csr_off;     // per-predator edge offsets
    std::vector<int>    csr_prey;    // prey index of each edge
    std::vector<double> csr_alpha;   // interaction strength of each edge
    std::vector<double> csr_inv_xeq; // precomputed 1/max(eps, xeq[prey])

    std::vector<double> dx_scratch;  // step() derivative buffer (reused, no per-frame alloc)

    CommunityStats       stats;
    BiomeParams          biome;
};

// Build a deterministic, balanced, co-adapted community for a biome.
Community generate_community(std::uint64_t seed, const BiomeParams& biome);

// Advance live population dynamics by dt (bounded, damped — never diverges).
void step_community(Community& community, double dt);

// Soft, continuous role label and a hue for a given trophic level.
const char* role_label(double tau);
double      role_hue(double tau); // green producers -> blue herbivores -> red carnivores

} // namespace eco
} // namespace cosmos
