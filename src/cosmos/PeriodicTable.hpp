// PeriodicTable.hpp -- the periodic table as physics: electron configurations
// generated from the Aufbau / Madelung (n+l) ordering, shell filling and valence
// counting, period/group/block assignment, and the measured periodic trends
// (atomic radius, first ionization energy, electronegativity) that make chemistry
// periodic. This is where the quantum atom becomes the chemical element.
//
// Header-only, pure, deterministic.
//
// Sources:
//   - Madelung / Aufbau ordering: subshells fill by increasing (n+l), then n.
//   - Pauling electronegativities, first ionization energies (eV) and empirical
//       atomic radii (pm) from standard reference tables (CRC Handbook).

#ifndef COSMOS_PERIODICTABLE_HPP
#define COSMOS_PERIODICTABLE_HPP

#include "cosmos/AtomicStructure.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace cosmos {
namespace periodic {

// --- Aufbau / Madelung electron configuration -------------------------------

struct Subshell {
    int n;
    int l;
    int capacity; // 2(2l+1)
    int occupancy;
};

// The Madelung filling order (subshells sorted by n+l, then n), enough to reach
// the heaviest natural elements. Each entry is (n, l).
namespace detail {
inline constexpr int kFillOrder[][2] = {
    {1, 0},                         // 1s
    {2, 0}, {2, 1},                 // 2s 2p
    {3, 0}, {3, 1},                 // 3s 3p
    {4, 0}, {3, 2}, {4, 1},         // 4s 3d 4p
    {5, 0}, {4, 2}, {5, 1},         // 5s 4d 5p
    {6, 0}, {4, 3}, {5, 2}, {6, 1}, // 6s 4f 5d 6p
    {7, 0}, {5, 3}, {6, 2}, {7, 1}, // 7s 5f 6d 7p
};
inline constexpr std::size_t kFillCount = sizeof(kFillOrder) / sizeof(kFillOrder[0]);
} // namespace detail

// Fill Z electrons into subshells in Madelung order. Returns the occupied
// subshells in fill order.
inline std::vector<Subshell> electron_configuration(int Z) {
    std::vector<Subshell> cfg;
    int remaining = Z;
    for (std::size_t i = 0; i < detail::kFillCount && remaining > 0; ++i) {
        const int n = detail::kFillOrder[i][0];
        const int l = detail::kFillOrder[i][1];
        const int cap = 2 * (2 * l + 1);
        const int occ = remaining < cap ? remaining : cap;
        cfg.push_back({n, l, cap, occ});
        remaining -= occ;
    }
    return cfg;
}

// The configuration as a string, e.g. Z=11 -> "1s2 2s2 2p6 3s1".
inline std::string configuration_string(int Z) {
    const auto cfg = electron_configuration(Z);
    std::string s;
    for (std::size_t i = 0; i < cfg.size(); ++i) {
        if (i)
            s += ' ';
        s += std::to_string(cfg[i].n);
        s += atom::orbital_letter(cfg[i].l);
        s += std::to_string(cfg[i].occupancy);
    }
    return s;
}

// Highest principal quantum number reached = the period (row) of the element.
inline int period(int Z) {
    const auto cfg = electron_configuration(Z);
    int maxn = 1;
    for (const Subshell &s : cfg)
        maxn = s.n > maxn ? s.n : maxn;
    return maxn;
}

// Valence electrons = electrons in the outermost shell (the highest n), summed
// over its s and p subshells (the chemically active electrons).
inline int valence_electrons(int Z) {
    const auto cfg = electron_configuration(Z);
    const int p = period(Z);
    int v = 0;
    for (const Subshell &s : cfg)
        if (s.n == p && (s.l == 0 || s.l == 1))
            v += s.occupancy;
    return v;
}

// The block (s/p/d/f) from the orbital of the last electron added.
inline char block(int Z) {
    const auto cfg = electron_configuration(Z);
    if (cfg.empty())
        return '?';
    return atom::orbital_letter(cfg.back().l);
}

// Noble gases have a filled outer shell: Z in {2,10,18,36,54,86}.
inline bool is_noble_gas(int Z) {
    for (int g : {2, 10, 18, 36, 54, 86})
        if (Z == g)
            return true;
    return false;
}

// --- Measured element data and periodic trends ------------------------------

struct Element {
    int Z;
    const char *symbol;
    const char *name;
    double electronegativity; // Pauling (0 if undefined, e.g. noble gases)
    double ionization_ev;     // first ionization energy
    double radius_pm;         // empirical atomic radius
};

namespace detail {
inline constexpr Element kElements[] = {
    {1, "H", "Hydrogen", 2.20, 13.598, 53},    {2, "He", "Helium", 0.0, 24.587, 31},
    {3, "Li", "Lithium", 0.98, 5.392, 167},    {4, "Be", "Beryllium", 1.57, 9.323, 112},
    {5, "B", "Boron", 2.04, 8.298, 87},        {6, "C", "Carbon", 2.55, 11.260, 67},
    {7, "N", "Nitrogen", 3.04, 14.534, 56},    {8, "O", "Oxygen", 3.44, 13.618, 48},
    {9, "F", "Fluorine", 3.98, 17.423, 42},    {10, "Ne", "Neon", 0.0, 21.565, 38},
    {11, "Na", "Sodium", 0.93, 5.139, 190},    {12, "Mg", "Magnesium", 1.31, 7.646, 145},
    {13, "Al", "Aluminium", 1.61, 5.986, 118}, {14, "Si", "Silicon", 1.90, 8.152, 111},
    {15, "P", "Phosphorus", 2.19, 10.487, 98}, {16, "S", "Sulfur", 2.58, 10.360, 88},
    {17, "Cl", "Chlorine", 3.16, 12.968, 79},  {18, "Ar", "Argon", 0.0, 15.760, 71},
    {19, "K", "Potassium", 0.82, 4.341, 243},  {20, "Ca", "Calcium", 1.00, 6.113, 194},
    {26, "Fe", "Iron", 1.83, 7.902, 156},      {29, "Cu", "Copper", 1.90, 7.726, 145},
    {47, "Ag", "Silver", 1.93, 7.576, 165},    {79, "Au", "Gold", 2.54, 9.226, 174},
    {82, "Pb", "Lead", 2.33, 7.417, 154},      {92, "U", "Uranium", 1.38, 6.194, 196},
};
inline constexpr std::size_t kElementCount = sizeof(kElements) / sizeof(kElements[0]);
} // namespace detail

inline const Element *element(int Z) {
    for (std::size_t i = 0; i < detail::kElementCount; ++i)
        if (detail::kElements[i].Z == Z)
            return &detail::kElements[i];
    return nullptr;
}

inline const Element *element_table() {
    return detail::kElements;
}
inline std::size_t element_count() {
    return detail::kElementCount;
}

} // namespace periodic
} // namespace cosmos

#endif // COSMOS_PERIODICTABLE_HPP
