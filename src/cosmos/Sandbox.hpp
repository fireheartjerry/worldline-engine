#pragma once

#include "cosmos/LawGenome.hpp"
#include "cosmos/NBodySystem.hpp"
#include "cosmos/ObjectCatalog.hpp"
#include "cosmos/ScaleLadder.hpp"

#include <vector>

namespace cosmos {

// Canonical sandbox timestep. Fixed (not frame-rate dependent) so a sandbox is
// exactly reproducible from (seed, scale, step_count) — which is what lets a
// saved sandbox be reopened to the same evolved state.
constexpr double kSandboxDt = 1.0 / 120.0;
constexpr int kSandboxSubsteps = 4;
constexpr int kDefaultBodyCount = 72;

// Deterministically fill `sys` with a representative population of `scale`,
// using the universe's genome for both the force law and the spawn pattern.
// Same (catalog, genome, scale, body_count) always yields identical bodies.
void populate_sandbox(NBodySystem& sys,
                      const std::vector<UniverseObject>& catalog,
                      const LawGenome& genome,
                      Scale scale,
                      int body_count = kDefaultBodyCount);

// Advance the sandbox by `steps` fixed steps. Deterministic.
void advance_sandbox(NBodySystem& sys, int steps);

// Snapshot of derived quantities for the "analyze" surface and for tests.
struct SandboxStats {
    int bodies = 0;
    int bound_pairs = 0;
    double energy = 0.0;
    double virial = 0.0;
    double rms_radius = 0.0;
    bool finite = true; // all positions/velocities finite and bounded
};

SandboxStats sandbox_stats(const NBodySystem& sys);

} // namespace cosmos
