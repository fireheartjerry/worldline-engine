#pragma once
#include "UniverseAxioms.hpp"
#include <cstdint>
#include <string>

// ============================================================================
// Seed.hpp — the entry point for the seeded-universe system.
//
// YOU implement both functions below.  The rest of the system (DerivedFramework,
// Universe subclasses, main.cpp) calls them and trusts the results.
//
// Design intent
// -------------
// The seed system is intentionally left open so that you can choose your own
// hashing strategy and your own axiom-derivation philosophy.  Two broad
// approaches work well:
//
//   A) Deterministic bit-shuffling
//      hash_seed turns the string into a 64-bit integer.  derive_axioms then
//      picks enumerators by taking (seed >> offset) % NUM_VARIANTS for each
//      categorical field and maps bit ranges to continuous parameters.
//      Every seed maps to exactly one universe; very close seeds can produce
//      wildly different universes.
//
//   B) Name-semantic mapping
//      Parse the string for keywords ("hyperbolic", "chaotic", "memory", …)
//      and bias the axioms accordingly.  hash_seed still produces a number
//      for tie-breaking and noise.
//
// Invariants you must satisfy
// ---------------------------
//   1. Determinism: same input string → same UniverseAxioms, always.
//   2. Full coverage: over a large sample of seeds, every Space, Time, etc.
//      variant should be reachable.
//   3. Parameter ranges: every continuous field in UniverseAxioms must lie
//      within the bounds declared in UniverseAxioms::ParamRanges.  Violating
//      a range will cause undefined behaviour in the physics layer.
//   4. Conservation consistency: if dynamics == STOCHASTIC_DRIFT then
//      conservation.energy must be false.  If dynamics == DISCRETE_MAP then
//      time must be DISCRETE or CONTINUOUS (not RETROCAUSAL).
//      See DerivedFramework.hpp for the full compatibility matrix.
// ============================================================================

namespace Worldline {

// ---------------------------------------------------------------------------
// hash_seed
//
// Convert a human-readable seed string into a 64-bit unsigned integer.
// This integer is stored in UniverseAxioms::seed and is the sole source of
// randomness for derive_axioms — the rest of the pipeline is deterministic.
//
// What to implement:
//   - Any good non-cryptographic hash is fine: FNV-1a, xxHash, MurmurHash3,
//     or even a hand-rolled polynomial hash.
//   - The empty string "" should produce a stable, non-zero value (not 0,
//     which some hash proofs treat as a degenerate case).
//   - Case-sensitivity is your choice; document it.
//
// Example signature you might implement:
//   for each char c in input: h = h * 31 + c;  return h ^ (h >> 33);
// ---------------------------------------------------------------------------
uint64_t hash_seed(const std::string& input);
// TODO: implement in your own source file or inline here.


// ---------------------------------------------------------------------------
// derive_axioms
//
// Map a 64-bit seed to a fully populated UniverseAxioms struct.
//
// Suggested approach:
//   Split the seed into non-overlapping bit windows and use each window to
//   select an enumerator or scale a continuous parameter.  For example:
//
//     uint64_t s = seed;
//     ax.space    = static_cast<Space>(s % NUM_SPACE_VARIANTS);    s >>= 4;
//     ax.time     = static_cast<Time>(s % NUM_TIME_VARIANTS);      s >>= 4;
//     ...
//     ax.gravity_exponent = ParamRanges::GRAVITY_EXPONENT_MIN
//         + (s & 0xFFFF) / 65535.0
//         * (ParamRanges::GRAVITY_EXPONENT_MAX - ParamRanges::GRAVITY_EXPONENT_MIN);
//
//   After populating all fields, apply the compatibility fixups:
//     - Set conservation.energy = false  if dynamics == STOCHASTIC_DRIFT.
//     - Set dynamics = LEAST_ACTION      if time == RETROCAUSAL and dynamics
//       is one of the non-boundary-value types (the solver can't run backwards
//       with a non-symplectic integrator).
//     - Clamp all continuous params to their ParamRanges bounds.
//
//   Store the raw seed in ax.seed before returning.
//
// What NOT to do:
//   - Don't use std::rand / global mutable state — that breaks determinism
//     when called from multiple places.
//   - Don't hardcode specific seeds to specific universes (except for testing).
// ---------------------------------------------------------------------------
UniverseAxioms derive_axioms(uint64_t seed);
// TODO: implement in your own source file or inline here.

} // namespace Worldline
