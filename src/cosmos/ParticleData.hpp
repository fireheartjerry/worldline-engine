// ParticleData.hpp -- physics-data lookup tables and scoring functions for the
// SUBATOMIC..ATOMIC scale tiers of the Worldline engine.
//
// Header-only, pure, deterministic. All values are sourced from the standard
// reference literature and cited inline:
//   - Particle masses/charges/spins: PDG 2024 (Review of Particle Physics).
//   - Fundamental constants: CODATA 2022.
//   - Bethe-Weizsaecker SEMF coefficients: standard "textbook" fit (Krane,
//     Introductory Nuclear Physics; a_V=15.8, a_S=18.3, a_C=0.71, a_A=23.2,
//     a_P=11.2 MeV).
//   - Primordial (BBN) abundances: standard Big Bang nucleosynthesis +
//     Planck 2018 baryon density (Y_p ~ 0.247-0.25, D/H ~ 2.5e-5).
//   - Solar/cosmic photospheric abundances: Asplund, Grevesse, Sauval & Scott
//     (2009), Annu. Rev. Astron. Astrophys. 47, 481 (log scale, log N_H = 12).
//
// Namespace: cosmos::particles.

#ifndef COSMOS_PARTICLEDATA_HPP
#define COSMOS_PARTICLEDATA_HPP

#include <cmath>
#include <cstddef>
#include <cstring>

namespace cosmos::particles {

// ---------------------------------------------------------------------------
// Standard Model particles
// ---------------------------------------------------------------------------

enum class Particle {
    // Quarks (generation 1, 2, 3)
    Up,
    Down,
    Strange,
    Charm,
    Bottom,
    Top,
    // Charged leptons
    Electron,
    Muon,
    Tau,
    // Neutrinos
    NeutrinoE,
    NeutrinoMu,
    NeutrinoTau,
    // Gauge / scalar bosons
    Photon,
    Gluon,
    WBoson,
    ZBoson,
    Higgs,
    Count // sentinel
};

struct ParticleInfo {
    const char* name;   // human-readable name
    const char* symbol; // standard symbol
    double mass_mev;    // mass in MeV/c^2
    double charge_e;    // electric charge in units of the elementary charge e
    double spin;        // intrinsic spin (in units of hbar)
    int generation;     // 1/2/3 for fermions; 0 for bosons
};

namespace detail {

// PDG 2024 values. Quark masses are the MSbar masses (u,d,s at mu=2 GeV;
// c,b,t at their own scale / pole-equivalent as quoted by PDG). Neutrino
// masses are unmeasured and ~0 at this scale, so stored as 0.0.
inline constexpr ParticleInfo kParticleTable[] = {
    // name        symbol  mass_mev      charge   spin  gen
    {"up",        "u",       2.16,     2.0 / 3.0, 0.5, 1}, // PDG 2024
    {"down",      "d",       4.67,    -1.0 / 3.0, 0.5, 1}, // PDG 2024
    {"strange",   "s",      93.4,     -1.0 / 3.0, 0.5, 2}, // PDG 2024
    {"charm",     "c",    1270.0,      2.0 / 3.0, 0.5, 2}, // PDG 2024
    {"bottom",    "b",    4180.0,     -1.0 / 3.0, 0.5, 3}, // PDG 2024
    {"top",       "t",  172570.0,      2.0 / 3.0, 0.5, 3}, // PDG 2024
    {"electron",  "e",       0.511,   -1.0,       0.5, 1}, // PDG 2024 (0.5109989 MeV)
    {"muon",      "mu",    105.66,    -1.0,       0.5, 2}, // PDG 2024 (105.6584 MeV)
    {"tau",       "tau",  1776.9,     -1.0,       0.5, 3}, // PDG 2024 (1776.86 MeV)
    {"neutrino_e","nu_e",     0.0,     0.0,       0.5, 1}, // ~0
    {"neutrino_mu","nu_mu",   0.0,     0.0,       0.5, 2}, // ~0
    {"neutrino_tau","nu_tau", 0.0,     0.0,       0.5, 3}, // ~0
    {"photon",    "gamma",    0.0,     0.0,       1.0, 0}, // massless gauge boson
    {"gluon",     "g",        0.0,     0.0,       1.0, 0}, // massless gauge boson
    {"W boson",   "W",    80369.0,     1.0,       1.0, 0}, // PDG 2024 (80.3692 GeV); charge +-1
    {"Z boson",   "Z",    91188.0,     0.0,       1.0, 0}, // PDG 2024 (91.1880 GeV)
    {"Higgs",     "H",   125200.0,     0.0,       0.0, 0}, // PDG 2024 (125.20 GeV), scalar
};

static_assert(sizeof(kParticleTable) / sizeof(kParticleTable[0]) ==
                  static_cast<std::size_t>(Particle::Count),
              "particle table size must match enum");

} // namespace detail

// Accessor for the immutable particle table.
inline const ParticleInfo& particle_info(Particle p) {
    return detail::kParticleTable[static_cast<std::size_t>(p)];
}

inline const ParticleInfo* particle_table() { return detail::kParticleTable; }
inline std::size_t particle_count() {
    return static_cast<std::size_t>(Particle::Count);
}

// ---------------------------------------------------------------------------
// Standard Model classification -- pure predicates over the enum / spin.
// ---------------------------------------------------------------------------
//
// The Standard Model splits cleanly: fermions (half-integer spin: quarks +
// leptons) build matter; bosons (integer spin) carry forces or, for the Higgs,
// give mass. These predicates let the quantum-tier UI and generator reason about
// a particle's role without hard-coding indices everywhere.

inline bool is_quark(Particle p) {
    return p >= Particle::Up && p <= Particle::Top;
}
inline bool is_charged_lepton(Particle p) {
    return p >= Particle::Electron && p <= Particle::Tau;
}
inline bool is_neutrino(Particle p) {
    return p >= Particle::NeutrinoE && p <= Particle::NeutrinoTau;
}
inline bool is_lepton(Particle p) {
    return is_charged_lepton(p) || is_neutrino(p);
}
inline bool is_gauge_boson(Particle p) {
    return p >= Particle::Photon && p <= Particle::ZBoson;
}
inline bool is_scalar_boson(Particle p) { return p == Particle::Higgs; }

// Fermion <=> half-integer spin; boson <=> integer spin. Derived from the spin
// value so the predicate can't disagree with the table.
inline bool is_fermion(Particle p) {
    const double s = particle_info(p).spin;
    const double frac = s - std::floor(s);
    return std::fabs(frac - 0.5) < 0.25; // 1/2, 3/2, ... -> fermion
}
inline bool is_boson(Particle p) { return !is_fermion(p); }

// A particle carries the strong (colour) charge -- and so is confined inside
// hadrons -- iff it is a quark or a gluon.
inline bool carries_colour(Particle p) {
    return is_quark(p) || p == Particle::Gluon;
}

// Effectively stable on laboratory timescales (no Standard-Model decay channel):
// the lightest charged lepton, the lightest quarks, the neutrinos, and the
// massless gauge bosons. Everything heavier in the table decays.
inline bool is_stable(Particle p) {
    return p == Particle::Up || p == Particle::Down || p == Particle::Electron ||
           is_neutrino(p) || p == Particle::Photon || p == Particle::Gluon;
}

// ---------------------------------------------------------------------------
// Nuclear binding energy -- semi-empirical mass formula (Bethe-Weizsaecker)
// ---------------------------------------------------------------------------
//
// B(Z,A) = a_V*A - a_S*A^(2/3) - a_C*Z(Z-1)/A^(1/3) - a_A*(A-2Z)^2/A + delta
// with the standard textbook coefficients (MeV).

inline constexpr double kSemf_aV = 15.8; // volume
inline constexpr double kSemf_aS = 18.3; // surface
inline constexpr double kSemf_aC = 0.71; // Coulomb
inline constexpr double kSemf_aA = 23.2; // asymmetry
inline constexpr double kSemf_aP = 11.2; // pairing (scaled by 1/sqrt(A))

// Pairing term sign: +even-even, -odd-odd, 0 for odd-A.
inline double semf_pairing_mev(int Z, int A) {
    const int N = A - Z;
    const bool zEven = (Z % 2) == 0;
    const bool nEven = (N % 2) == 0;
    const double delta = kSemf_aP / std::sqrt(static_cast<double>(A));
    if (zEven && nEven) return +delta;      // even-even
    if (!zEven && !nEven) return -delta;     // odd-odd
    return 0.0;                              // odd-A
}

// Total nuclear binding energy in MeV. Returns 0 for non-physical inputs.
inline double semf_binding_mev(int Z, int A) {
    if (A <= 0 || Z < 0 || Z > A) return 0.0;
    const double Ad = static_cast<double>(A);
    const double Zd = static_cast<double>(Z);
    const double volume = kSemf_aV * Ad;
    const double surface = kSemf_aS * std::cbrt(Ad * Ad); // A^(2/3)
    const double coulomb = kSemf_aC * Zd * (Zd - 1.0) / std::cbrt(Ad);
    const double asymmetry = kSemf_aA * (Ad - 2.0 * Zd) * (Ad - 2.0 * Zd) / Ad;
    return volume - surface - coulomb - asymmetry + semf_pairing_mev(Z, A);
}

// Binding energy per nucleon (MeV). Returns 0 for non-physical inputs.
inline double binding_per_nucleon(int Z, int A) {
    if (A <= 0) return 0.0;
    return semf_binding_mev(Z, A) / static_cast<double>(A);
}

// The binding-energy-per-nucleon curve peaks in the "iron group" near A=56-62
// (62Ni / 58Fe / 56Fe). This helper scans physically reasonable, near-stable
// (N ~ Z * 1.0..1.1) nuclei and returns the mass number A with maximum BE/A.
inline int most_bound_A() {
    int bestA = 1;
    double best = -1.0e30;
    for (int A = 2; A <= 250; ++A) {
        // Pick Z near the SEMF valley of stability for this A.
        int bestZ = 1;
        double bestForA = -1.0e30;
        for (int Z = 1; Z < A; ++Z) {
            const double bpn = binding_per_nucleon(Z, A);
            if (bpn > bestForA) {
                bestForA = bpn;
                bestZ = Z;
            }
        }
        (void)bestZ;
        if (bestForA > best) {
            best = bestForA;
            bestA = A;
        }
    }
    return bestA; // expected to land in the iron group (A ~ 56-62)
}

// ---------------------------------------------------------------------------
// Big Bang nucleosynthesis -- primordial abundances
// ---------------------------------------------------------------------------
//
// Mass fractions: X (hydrogen) ~ 0.75, Y (helium-4) ~ 0.25. Trace number ratios
// relative to hydrogen from standard BBN + Planck baryon density.

inline constexpr double kPrimordialH_massfrac = 0.75;  // X
inline constexpr double kPrimordialHe_massfrac = 0.25; // Y (He-4)
inline constexpr double kDH = 2.5e-5;                  // D/H  (number ratio)
inline constexpr double kHe3H = 1.1e-5;                // 3He/H (number ratio)
inline constexpr double kLi7H = 5.0e-10;               // 7Li/H (number ratio)

// ---------------------------------------------------------------------------
// Solar / cosmic photospheric abundances (Asplund et al. 2009, log N_H = 12)
// ---------------------------------------------------------------------------

struct SolarAbundance {
    const char* symbol;
    int Z;
    double log_abundance; // log10(N_X) with log10(N_H) = 12
};

namespace detail {

inline constexpr SolarAbundance kSolarTable[] = {
    {"H",   1, 12.00}, // Asplund 2009
    {"He",  2, 10.93}, // Asplund 2009
    {"C",   6,  8.43}, // Asplund 2009
    {"N",   7,  7.83}, // Asplund 2009
    {"O",   8,  8.69}, // Asplund 2009
    {"Ne", 10,  7.93}, // Asplund 2009
    {"Mg", 12,  7.60}, // Asplund 2009
    {"Si", 14,  7.51}, // Asplund 2009
    {"S",  16,  7.12}, // Asplund 2009
    {"Fe", 26,  7.50}, // Asplund 2009
};

inline constexpr std::size_t kSolarCount =
    sizeof(kSolarTable) / sizeof(kSolarTable[0]);

} // namespace detail

// Returns the log abundance for a chemical symbol, or -1.0 if unknown.
inline double solar_log_abundance(const char* symbol) {
    if (symbol == nullptr) return -1.0;
    for (std::size_t i = 0; i < detail::kSolarCount; ++i) {
        if (std::strcmp(detail::kSolarTable[i].symbol, symbol) == 0) {
            return detail::kSolarTable[i].log_abundance;
        }
    }
    return -1.0;
}

inline const SolarAbundance* solar_abundance_table() {
    return detail::kSolarTable;
}
inline std::size_t solar_abundance_count() { return detail::kSolarCount; }

// ---------------------------------------------------------------------------
// Oddo-Harkins rule: elements with even atomic number Z are generally more
// abundant than their odd-Z neighbours (paired nucleons -> more stable nuclei).
// ---------------------------------------------------------------------------

inline bool oddo_harkins_even_favored(int Z) {
    (void)Z;
    return true; // even-Z elements are generally more cosmically abundant
}

// Given two adjacent atomic numbers, return the one expected to be more
// abundant under the Oddo-Harkins rule (the even one). If the two are not
// adjacent or are equal, falls back to the even one (or za if both even/odd).
inline int more_abundant_adjacent(int za, int zb) {
    const bool aEven = (za % 2) == 0;
    const bool bEven = (zb % 2) == 0;
    if (aEven && !bEven) return za;
    if (bEven && !aEven) return zb;
    return za; // tie-break (both even or both odd)
}

// ---------------------------------------------------------------------------
// Electron shell / subshell capacities
// ---------------------------------------------------------------------------

// Maximum electrons in principal shell n: 2 n^2.
inline int shell_capacity(int n) {
    if (n <= 0) return 0;
    return 2 * n * n;
}

// Maximum electrons in a subshell with azimuthal quantum number l: 2(2l+1).
// l = 0,1,2,3 -> s,p,d,f -> 2,6,10,14.
inline int subshell_capacity(int l) {
    if (l < 0) return 0;
    return 2 * (2 * l + 1);
}

} // namespace cosmos::particles

#endif // COSMOS_PARTICLEDATA_HPP
