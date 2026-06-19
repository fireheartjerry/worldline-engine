// NuclearShell.hpp -- the nuclear shell model: the quantum structure that the
// liquid drop misses. Magic numbers from the spin-orbit-split oscillator
// sequence, shell filling, ground-state spin-parity from the last unpaired
// nucleon, even-even 0+ pairing, and doubly-magic nuclei. This is why some nuclei
// (He-4, O-16, Ca-40, Ni-56, Pb-208) are exceptionally tightly bound.
//
// Header-only, pure, deterministic.
//
// Sources:
//   - Mayer & Jensen (1949) shell model; magic numbers 2, 8, 20, 28, 50, 82, 126.
//   - Level ordering 1s1/2, 1p3/2, 1p1/2, 1d5/2, 2s1/2, 1d3/2, 1f7/2, ... with
//       spin-orbit splitting reproducing the magic gaps.
//   - Ground-state spin-parity: extreme single-particle model (last nucleon's j,
//       parity (-1)^l); even-even nuclei are 0+.

#ifndef COSMOS_NUCLEARSHELL_HPP
#define COSMOS_NUCLEARSHELL_HPP

#include <array>
#include <cmath>
#include <cstddef>

namespace cosmos {
namespace shell {

// The canonical magic numbers (closed shells), plus 184 (predicted, super-heavy).
inline constexpr std::array<int, 8> kMagicNumbers = {2, 8, 20, 28, 50, 82, 126, 184};

inline bool is_magic(int n) {
    for (int m : kMagicNumbers)
        if (n == m)
            return true;
    return false;
}

// A nucleus is doubly magic when BOTH Z and N are closed shells -- the most
// tightly bound, spherical nuclei (He-4, O-16, Ca-40/48, Ni-56, Pb-208).
inline bool is_doubly_magic(int Z, int N) {
    return is_magic(Z) && is_magic(N);
}

// --- Shell-model single-particle orbitals -----------------------------------
//
// Each orbital holds 2j+1 identical nucleons. Listed in filling order; the
// cumulative occupancy crosses each magic number exactly at a large shell gap.

struct Orbital {
    const char *label; // spectroscopic label, e.g. "1f7/2"
    int n;             // radial quantum number (1-based)
    int l;             // orbital angular momentum (s=0,p=1,d=2,f=3,g=4,h=5,i=6)
    int two_j;         // 2j (j = l +/- 1/2)
    int capacity;      // 2j + 1
    int cumulative;    // total nucleons once this orbital is filled
};

namespace detail {
inline constexpr Orbital kLevels[] = {
    {"1s1/2", 1, 0, 1, 2, 2},                               // -> magic 2
    {"1p3/2", 1, 1, 3, 4, 6},     {"1p1/2", 1, 1, 1, 2, 8}, // -> magic 8
    {"1d5/2", 1, 2, 5, 6, 14},    {"2s1/2", 2, 0, 1, 2, 16},
    {"1d3/2", 1, 2, 3, 4, 20}, // -> magic 20
    {"1f7/2", 1, 3, 7, 8, 28}, // -> magic 28
    {"2p3/2", 2, 1, 3, 4, 32},    {"1f5/2", 1, 3, 5, 6, 38},
    {"2p1/2", 2, 1, 1, 2, 40},    {"1g9/2", 1, 4, 9, 10, 50}, // -> magic 50
    {"1g7/2", 1, 4, 7, 8, 58},    {"2d5/2", 2, 2, 5, 6, 64},
    {"2d3/2", 2, 2, 3, 4, 68},    {"3s1/2", 3, 0, 1, 2, 70},
    {"1h11/2", 1, 5, 11, 12, 82}, // -> magic 82
    {"1h9/2", 1, 5, 9, 10, 92},   {"2f7/2", 2, 3, 7, 8, 100},
    {"2f5/2", 2, 3, 5, 6, 106},   {"3p3/2", 3, 1, 3, 4, 110},
    {"3p1/2", 3, 1, 1, 2, 112},   {"1i13/2", 1, 6, 13, 14, 126}, // -> magic 126
};
inline constexpr std::size_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);
} // namespace detail

inline const Orbital *shell_levels() {
    return detail::kLevels;
}
inline std::size_t shell_level_count() {
    return detail::kLevelCount;
}

// The orbital that the nucleon numbered `count` (1-based) lands in.
inline const Orbital &orbital_for_nucleon(int count) {
    if (count < 1)
        return detail::kLevels[0];
    for (std::size_t i = 0; i < detail::kLevelCount; ++i)
        if (count <= detail::kLevels[i].cumulative)
            return detail::kLevels[i];
    return detail::kLevels[detail::kLevelCount - 1];
}

// j (as 2j) of the last (valence) nucleon of a single species with `count` of them.
inline int valence_two_j(int count) {
    return orbital_for_nucleon(count).two_j;
}

// --- Ground-state spin and parity (extreme single-particle model) -----------

struct SpinParity {
    int two_j;  // 2j (so half-integer spins stay integers)
    int parity; // +1 or -1
};

// Even-even nuclei are 0+. Odd-A nuclei take the j and parity of the single
// unpaired nucleon. Odd-odd nuclei are left as the coupling of the two valence
// nucleons' parities (a useful approximation; their j needs a coupling rule).
inline SpinParity ground_state_spin_parity(int Z, int N) {
    const bool zEven = (Z % 2) == 0;
    const bool nEven = (N % 2) == 0;
    if (zEven && nEven)
        return {0, +1}; // 0+
    const Orbital &oz = orbital_for_nucleon(Z);
    const Orbital &on = orbital_for_nucleon(N);
    auto par = [](int l) { return (l % 2 == 0) ? +1 : -1; };
    if (!zEven && nEven)
        return {oz.two_j, par(oz.l)}; // odd proton
    if (zEven && !nEven)
        return {on.two_j, par(on.l)};         // odd neutron
    return {oz.two_j, par(oz.l) * par(on.l)}; // odd-odd: combined parity
}

// --- Pairing ----------------------------------------------------------------

// Empirical pairing gap Delta ~ 12 / sqrt(A) MeV: the extra binding of a paired
// (even) nucleon set over an unpaired one.
inline double pairing_gap_mev(int A) {
    if (A <= 0)
        return 0.0;
    return 12.0 / std::sqrt(static_cast<double>(A));
}

// Magic-shell-closure bonus: how many of {Z, N} sit at a closed shell (0, 1, 2).
// A crude "extra stability" index that complements the liquid-drop binding.
inline int shell_closure_count(int Z, int N) {
    return (is_magic(Z) ? 1 : 0) + (is_magic(N) ? 1 : 0);
}

} // namespace shell
} // namespace cosmos

#endif // COSMOS_NUCLEARSHELL_HPP
