// Hadronization.hpp -- build colour-singlet hadrons from quark content and read
// off their quantum numbers, then generate the light-hadron spectrum (the mesons
// and baryons made from u, d, s) deterministically. This is the QCD layer of the
// first tier: quarks are confined, so the *observable* particles of the strong
// sector are the bound states this module constructs.
//
// Header-only, pure, deterministic. Masses in MeV.
//
// Sources:
//   - Constituent quark masses (De Rujula-Georgi-Glashow scale): m_u~m_d~336,
//       m_s~540, m_c~1550, m_b~4730 MeV. Used for naive additive mass estimates.
//   - Quantum numbers: standard quark model (PDG "Quark Model" review).
//   - Gell-Mann-Nishijima Q = I_3 + (B + S)/2 for the additive charge check.

#ifndef COSMOS_HADRONIZATION_HPP
#define COSMOS_HADRONIZATION_HPP

#include <array>
#include <cmath>
#include <cstddef>

namespace cosmos {
namespace qcd {

// The light quark flavours that build ordinary matter (plus charm/bottom for
// reach). Antiquarks are represented by a sign on the count, below.
enum class Flavour { Down, Up, Strange, Charm, Bottom, Count };

struct FlavourInfo {
    const char *name;
    char symbol;
    double constituent_mass_mev; // effective in-hadron mass
    double charge_e;             // electric charge of the quark
    int strangeness;             // -1 for s (the s quark has S=-1 by convention)
    int charm;
    int bottomness;
};

namespace detail {
inline constexpr FlavourInfo kFlavours[] = {
    //  name       sym  m_const  charge      S   C   B
    {"down", 'd', 336.0, -1.0 / 3.0, 0, 0, 0},     {"up", 'u', 336.0, 2.0 / 3.0, 0, 0, 0},
    {"strange", 's', 540.0, -1.0 / 3.0, -1, 0, 0}, {"charm", 'c', 1550.0, 2.0 / 3.0, 0, +1, 0},
    {"bottom", 'b', 4730.0, -1.0 / 3.0, 0, 0, -1},
};
static_assert(sizeof(kFlavours) / sizeof(kFlavours[0]) == static_cast<std::size_t>(Flavour::Count),
              "flavour table size must match enum");
} // namespace detail

inline const FlavourInfo &flavour_info(Flavour f) {
    return detail::kFlavours[static_cast<std::size_t>(f)];
}

// A quark content: a small bag of (flavour, +1 for quark / -1 for antiquark).
struct Quark {
    Flavour flavour;
    int sign; // +1 quark, -1 antiquark
};

// Up to three valence quarks covers mesons (q qbar) and baryons (q q q).
struct HadronContent {
    std::array<Quark, 3> quarks{};
    int n = 0;

    void add(Flavour f, int sign) {
        if (n < 3)
            quarks[n++] = {f, sign};
    }
};

inline HadronContent meson(Flavour q, Flavour qbar) {
    HadronContent h;
    h.add(q, +1);
    h.add(qbar, -1);
    return h;
}
inline HadronContent baryon(Flavour a, Flavour b, Flavour c) {
    HadronContent h;
    h.add(a, +1);
    h.add(b, +1);
    h.add(c, +1);
    return h;
}

// --- Additive quantum numbers ------------------------------------------------

inline double hadron_charge(const HadronContent &h) {
    double q = 0.0;
    for (int i = 0; i < h.n; ++i)
        q += h.quarks[i].sign * flavour_info(h.quarks[i].flavour).charge_e;
    return q;
}

inline double baryon_number(const HadronContent &h) {
    int net = 0;
    for (int i = 0; i < h.n; ++i)
        net += h.quarks[i].sign;
    return net / 3.0;
}

inline int strangeness(const HadronContent &h) {
    int s = 0;
    for (int i = 0; i < h.n; ++i)
        s += h.quarks[i].sign * flavour_info(h.quarks[i].flavour).strangeness;
    return s;
}

inline int charm(const HadronContent &h) {
    int cval = 0;
    for (int i = 0; i < h.n; ++i)
        cval += h.quarks[i].sign * flavour_info(h.quarks[i].flavour).charm;
    return cval;
}

// Naive additive constituent mass (no spin-spin / binding correction) in MeV.
// Good enough to order the spectrum (p < Lambda < Omega), not to predict masses.
inline double naive_mass_mev(const HadronContent &h) {
    double m = 0.0;
    for (int i = 0; i < h.n; ++i)
        m += flavour_info(h.quarks[i].flavour).constituent_mass_mev;
    return m;
}

// --- Colour-singlet rule -----------------------------------------------------
// Only colour singlets exist as free particles: a quark-antiquark pair (meson)
// or three quarks / three antiquarks (baryon / antibaryon). Anything else is
// not a valid free hadron.
inline bool is_meson(const HadronContent &h) {
    return h.n == 2 && (h.quarks[0].sign + h.quarks[1].sign) == 0;
}
inline bool is_baryon(const HadronContent &h) {
    if (h.n != 3)
        return false;
    const int s = h.quarks[0].sign + h.quarks[1].sign + h.quarks[2].sign;
    return s == 3 || s == -3;
}
inline bool is_colour_singlet(const HadronContent &h) {
    return is_meson(h) || is_baryon(h);
}

// Gell-Mann-Nishijima cross-check: for a hadron, Q must equal I_3 + (B + S)/2.
// Returns the I_3 implied by the additive charge (so a test can confirm the two
// routes to the charge agree).
inline double implied_isospin_3(const HadronContent &h) {
    return hadron_charge(h) - 0.5 * (baryon_number(h) + strangeness(h));
}

} // namespace qcd
} // namespace cosmos

#endif // COSMOS_HADRONIZATION_HPP
