// StellarBurning.hpp -- the thermonuclear reaction networks that power stars and
// forge the elements: the proton-proton chain and the CNO cycle (hydrogen
// burning), the triple-alpha process (helium burning), and the advanced burning
// stages (carbon, neon, oxygen, silicon) that build a star up to the iron peak.
// Q-values, ignition temperatures, and the steep temperature sensitivities that
// decide which process dominates.
//
// Header-only, pure, deterministic. Energies in MeV, temperatures in K.
//
// Sources:
//   - pp-chain net 4p -> He-4 + 2e+ + 2nu, Q = 26.73 MeV (incl. annihilation).
//   - CNO cycle net 4p -> He-4, Q ~ 26.73 MeV (more neutrino loss).
//   - Triple-alpha 3 He-4 -> C-12, Q = 7.275 MeV (Hoyle-state resonance).
//   - Ignition temperatures and burning stages: stellar-structure textbooks.
//   - Temperature sensitivities: epsilon_pp ~ T^4, epsilon_CNO ~ T^17.

#ifndef COSMOS_STELLARBURNING_HPP
#define COSMOS_STELLARBURNING_HPP

#include <array>
#include <cmath>
#include <cstddef>

namespace cosmos {
namespace burning {

// --- Hydrogen burning -------------------------------------------------------

inline constexpr double kQ_pp_chain_mev = 26.73;        // 4p -> He-4 (total)
inline constexpr double kQ_pp_neutrino_loss_mev = 0.59; // ppI neutrino losses
inline constexpr double kQ_cno_mev = 26.73;             // CNO net (same fuel/ash)
inline constexpr double kQ_cno_neutrino_loss_mev = 1.71;

// Effective energy actually deposited as heat (Q minus neutrino losses). [MeV]
inline double pp_chain_heat_mev() {
    return kQ_pp_chain_mev - kQ_pp_neutrino_loss_mev;
}
inline double cno_heat_mev() {
    return kQ_cno_mev - kQ_cno_neutrino_loss_mev;
}

// Power-law temperature sensitivity epsilon ~ T^nu near a reference temperature.
// The pp-chain is gentle (nu ~ 4); the CNO cycle is ferociously steep (nu ~ 17),
// which is why massive (hot) stars burn by CNO and have convective cores.
inline constexpr double kPP_temperature_exponent = 4.0;
inline constexpr double kCNO_temperature_exponent = 17.0;

// Above the crossover temperature (~1.8e7 K) the CNO cycle overtakes the pp-chain
// as the dominant hydrogen-burning channel.
inline constexpr double kCNO_crossover_K = 1.8e7;
inline bool cno_dominates(double T_K) {
    return T_K > kCNO_crossover_K;
}

// --- Helium burning ---------------------------------------------------------

inline constexpr double kQ_triple_alpha_mev = 7.275; // 3 He-4 -> C-12
inline constexpr double kQ_c12_alpha_mev = 7.162;    // C-12 + He-4 -> O-16

// The triple-alpha rate is extraordinarily temperature sensitive (~T^40 near
// 1e8 K) because it proceeds through the finely-tuned Hoyle resonance.
inline constexpr double kTripleAlpha_temperature_exponent = 40.0;

// --- Burning stages ---------------------------------------------------------

struct BurningStage {
    const char *name;
    const char *fuel;
    const char *main_ash;
    double ignition_T_K;       // approximate ignition temperature
    double q_per_reaction_mev; // representative energy release
};

namespace detail {
inline constexpr BurningStage kStages[] = {
    {"Hydrogen", "H", "He", 1.5e7, 26.73},
    {"Helium", "He", "C, O", 1.0e8, 7.275},
    {"Carbon", "C", "Ne, Na, Mg", 6.0e8, 13.93},
    {"Neon", "Ne", "O, Mg", 1.2e9, 4.59},
    {"Oxygen", "O", "Si, S", 1.5e9, 16.54},
    {"Silicon", "Si", "Fe, Ni", 2.7e9, 0.0}, // photodisintegration -> NSE iron peak
};
inline constexpr std::size_t kStageCount = sizeof(kStages) / sizeof(kStages[0]);
} // namespace detail

inline const BurningStage *burning_stages() {
    return detail::kStages;
}
inline std::size_t burning_stage_count() {
    return detail::kStageCount;
}

// The advanced burning stages ignite in strict order of increasing temperature.
inline bool stages_temperature_ordered() {
    for (std::size_t i = 1; i < detail::kStageCount; ++i)
        if (detail::kStages[i].ignition_T_K <= detail::kStages[i - 1].ignition_T_K)
            return false;
    return true;
}

// Silicon burning ends at the iron peak: fusion past Fe/Ni costs energy rather
// than releasing it, so the star can no longer support itself by fusion.
inline constexpr int kIronPeakA = 56; // Fe-56 / Ni-56

} // namespace burning
} // namespace cosmos

#endif // COSMOS_STELLARBURNING_HPP
