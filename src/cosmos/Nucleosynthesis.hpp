// Nucleosynthesis.hpp -- the GENERATION STEP for the nuclear tier. Given a
// universe's law genome, synthesize how that universe forges the elements: the
// iron peak where fusion stops paying, the s- and r-process abundance peaks
// pinned to the neutron magic numbers, the fission limit that caps the periodic
// table, the primordial light-element split, the cosmic abundance pattern
// (decline + Oddo-Harkins odd-even + iron bump), and the sites where it all
// happens -- ending in an anthropic verdict on whether the elements of life and
// of heavy chemistry can exist at all.
//
// Anchored so an all-1.0 genome reproduces our universe: iron peak at A~56-62,
// s-process peaks at A ~ 88 / 138 / 208 (Sr, Ba, Pb), the Hoyle-resonance window
// for carbon, and a fission limit in the super-heavy region.
//
// Header-only, pure, deterministic. Depends only on header-only nuclear modules.
//
// Sources:
//   - Burbidge, Burbidge, Fowler & Hoyle (1957) "B2FH": the synthesis of the
//       elements (s-process, r-process, e-process / iron peak).
//   - Magic-number neutron-capture peaks; the Hoyle (1953) carbon resonance.

#ifndef COSMOS_NUCLEOSYNTHESIS_HPP
#define COSMOS_NUCLEOSYNTHESIS_HPP

#include "cosmos/LawGenome.hpp"
#include "cosmos/NuclearData.hpp"
#include "cosmos/NuclearReactions.hpp"
#include "cosmos/NuclearShell.hpp"
#include "cosmos/ParticleData.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace cosmos {
namespace nucleosynth {

// A neutron-capture abundance peak pinned to a magic neutron number.
struct AbundancePeak {
    const char *process; // "s-process" or "r-process"
    int magic_N;         // the closed neutron shell responsible
    int mass_number_A;   // where the peak lands in the abundance curve
    int Z;               // the element at the peak
};

// A site where nucleosynthesis happens, and what it makes.
struct NucleosynthesisSite {
    const char *name;
    const char *process;
    const char *products;
};

// The full synthesized nuclear profile of a universe.
struct NuclearUniverse {
    double binding_scale; // effective strong binding (coupling_strong)
    double coulomb_scale; // effective Coulomb (coupling_em)

    int iron_peak_A;                // endpoint of exothermic fusion
    double max_binding_per_nucleon; // BE/A at the peak [MeV]
    int fission_limit_A;            // heaviest nucleus before x=Z^2/A >= crit
    int fission_limit_Z;

    double primordial_H;
    double primordial_He;

    bool carbon_resonance_ok; // Hoyle-state window for triple-alpha
    bool can_fuse_to_iron;    // a path from H up to the iron peak
    bool valley_of_stability_exists;
    bool r_process_possible;    // room for heavy-element neutron capture
    bool stable_heavy_elements; // periodic table reaches lead/bismuth

    std::array<AbundancePeak, 3> s_process;
    std::array<AbundancePeak, 3> r_process;

    double complexity_score; // [0,1]
    std::string verdict;
};

// --- Building blocks --------------------------------------------------------

// The iron peak: the mass number with the maximum binding energy per nucleon
// along the valley of stability (where fusion stops releasing energy).
inline int iron_peak_A(double &bpn_out) {
    int bestA = 56;
    double best = -1e30;
    for (int A = 12; A <= 100; ++A) {
        const int Z = nuclear::most_stable_Z(A);
        const double bpn = nuclear::binding_per_nucleon_mev(Z, A);
        if (bpn > best) {
            best = bpn;
            bestA = A;
        }
    }
    bpn_out = best;
    return bestA;
}

// The valley isotope whose neutron number first reaches a magic value -- the
// mass number where the corresponding s-process abundance peak sits.
inline int valley_isotope_for_N(int magic_N) {
    for (int A = magic_N; A <= magic_N + 220; ++A) {
        const int Z = nuclear::most_stable_Z(A);
        if (A - Z >= magic_N)
            return A;
    }
    return 2 * magic_N;
}

// --- The generation step ----------------------------------------------------

inline NuclearUniverse synthesize(const LawGenome &g) {
    NuclearUniverse u;
    u.binding_scale = g.coupling_strong;
    u.coulomb_scale = g.coupling_em;

    double bpn = 0.0;
    u.iron_peak_A = iron_peak_A(bpn);
    u.max_binding_per_nucleon = bpn;

    // Fission limit: scan up the valley until the (EM-vs-strong-weighted)
    // fissility reaches criticality. Stronger EM lowers the limit; stronger
    // binding raises it.
    u.fission_limit_A = 400;
    u.fission_limit_Z = 0;
    for (int A = 180; A <= 400; ++A) {
        const int Z = nuclear::most_stable_Z(A);
        const double x = reactions::fissility(Z, A) * u.coulomb_scale / u.binding_scale;
        if (x >= 1.0) {
            u.fission_limit_A = A;
            u.fission_limit_Z = Z;
            break;
        }
    }
    if (u.fission_limit_Z == 0)
        u.fission_limit_Z = nuclear::most_stable_Z(u.fission_limit_A);

    // s-process peaks at the neutron magic numbers; r-process peaks sit lower in
    // A (neutron-rich progenitors decay back to stability at smaller A).
    const std::array<int, 3> magic = {50, 82, 126};
    const std::array<int, 3> r_offset = {8, 8, 13};
    for (std::size_t i = 0; i < 3; ++i) {
        const int As = valley_isotope_for_N(magic[i]);
        u.s_process[i] = {"s-process", magic[i], As, nuclear::most_stable_Z(As)};
        const int Ar = As - r_offset[i];
        u.r_process[i] = {"r-process", magic[i], Ar, nuclear::most_stable_Z(Ar)};
    }

    // Primordial split: stronger nuclear binding freezes out a bit more helium.
    const double Y = std::clamp(0.25 * (0.6 + 0.4 * u.binding_scale), 0.0, 1.0);
    u.primordial_He = Y;
    u.primordial_H = 1.0 - Y;

    // Viability gates.
    // The triple-alpha (Hoyle) resonance is fine-tuned to ~a percent; a few-%
    // shift in the strong coupling destroys carbon (and hence oxygen) production.
    u.carbon_resonance_ok = std::abs(u.binding_scale - 1.0) <= 0.04;
    u.can_fuse_to_iron = u.carbon_resonance_ok; // no carbon -> no advanced burning
    u.valley_of_stability_exists =
        nuclear::most_stable_Z(56) >= 20 && nuclear::most_stable_Z(56) <= 30;
    u.r_process_possible = u.fission_limit_A > 200;
    u.stable_heavy_elements = u.fission_limit_A >= 209; // reaches lead/bismuth

    const bool gates[] = {u.carbon_resonance_ok,        u.can_fuse_to_iron,
                          u.valley_of_stability_exists, u.r_process_possible,
                          u.stable_heavy_elements,      u.primordial_H > 0.0};
    int passed = 0;
    for (bool b : gates)
        passed += b ? 1 : 0;
    const double base = static_cast<double>(passed) / 6.0;
    u.complexity_score = std::clamp(base * (0.6 + 0.4 * g.stability_bias), 0.0, 1.0);

    if (!u.valley_of_stability_exists) {
        u.verdict = "No valley of stability: nuclei do not bind into a periodic table.";
    } else if (!u.carbon_resonance_ok) {
        u.verdict = "Hoyle resonance detuned: no carbon or oxygen -- no organic chemistry.";
    } else if (!u.stable_heavy_elements) {
        u.verdict = "Fission limit too low: the periodic table stops short of lead.";
    } else if (!u.r_process_possible) {
        u.verdict = "No heavy-element synthesis: only light elements are forged.";
    } else {
        u.verdict = "Full nucleosynthesis: H to the iron peak, s- and r-process heavies.";
    }
    return u;
}

// The cosmic relative abundance of mass number A: a steep decline with A,
// modulated by the Oddo-Harkins odd-even effect and a bump at the iron peak.
// Deterministic and normalised to ~1 at A=1.
inline double cosmic_abundance(int A, int iron_A = 56) {
    if (A < 1)
        return 0.0;
    const double decline = std::exp(-static_cast<double>(A) / 28.0);
    const double oddo = (A % 2 == 0) ? 1.3 : 0.7; // even-A favoured
    const double d = static_cast<double>(A - iron_A);
    const double iron_bump = 1.0 + 8.0 * std::exp(-d * d / (2.0 * 6.0 * 6.0));
    return decline * oddo * iron_bump;
}

// The major nucleosynthesis sites and what they forge.
inline std::vector<NucleosynthesisSite> sites() {
    return {
        {"Big Bang", "primordial", "H, He-4, traces of D, He-3, Li-7"},
        {"Main-sequence stars", "pp-chain / CNO", "He from H"},
        {"Red giants (AGB)", "triple-alpha + s-process", "C, O and slow-capture heavies"},
        {"Massive stars", "advanced burning", "Ne, O, Si up to the iron peak"},
        {"Core-collapse supernovae", "explosive + r-process", "alpha elements and some heavies"},
        {"Neutron-star mergers", "r-process", "gold, platinum, uranium"},
    };
}

} // namespace nucleosynth
} // namespace cosmos

#endif // COSMOS_NUCLEOSYNTHESIS_HPP
