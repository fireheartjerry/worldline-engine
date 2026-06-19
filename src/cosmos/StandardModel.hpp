// StandardModel.hpp -- the full Standard Model parameter set and its internal
// relations: gauge masses and the electroweak mixing angle, the Higgs mechanism
// (VEV, Yukawa couplings, self-coupling), conserved quantum numbers and the
// Gell-Mann-Nishijima charge formula, the CKM quark-mixing matrix, and the
// one-loop running of the gauge couplings (asymptotic freedom).
//
// Header-only, pure, deterministic. Energies in GeV unless a name says otherwise.
//
// Sources (PDG 2024 unless noted):
//   - M_Z=91.1876, M_W=80.377, m_H=125.25 GeV; G_F=1.1663788e-5 GeV^-2.
//   - On-shell weak mixing: sin^2 theta_W = 1 - M_W^2/M_Z^2.
//   - Higgs VEV v = (sqrt(2) G_F)^(-1/2) = 246.22 GeV; Yukawa y_f = sqrt(2) m_f/v;
//       self-coupling lambda = m_H^2 / (2 v^2).
//   - Gell-Mann-Nishijima Q = I_3 + (B + S)/2 = I_3 + Y/2.
//   - CKM Wolfenstein parameterisation: lambda=0.22500, A=0.826, rhobar=0.159,
//       etabar=0.348 (PDG 2024 global fit).
//   - QCD one-loop: alpha_s(Q) = 1/(b0 ln(Q^2/Lambda^2)), b0=(33-2 n_f)/(12 pi).

#ifndef COSMOS_STANDARDMODEL_HPP
#define COSMOS_STANDARDMODEL_HPP

#include "cosmos/Constants.hpp"
#include "cosmos/ParticleData.hpp"

#include <cmath>

namespace cosmos {
namespace sm {

using namespace cosmos::constants;

// ---------------------------------------------------------------------------
// Electroweak sector
// ---------------------------------------------------------------------------

inline constexpr double kMZ_GeV = 91.1876;         // Z boson mass
inline constexpr double kMW_GeV = 80.377;          // W boson mass
inline constexpr double kMH_GeV = 125.25;          // Higgs boson mass
inline constexpr double kGF_GeV2 = 1.1663788e-5;   // Fermi constant [GeV^-2]
inline constexpr double kHiggsVEV_GeV = 246.21965; // v = (sqrt(2) G_F)^(-1/2)

// On-shell weak mixing angle: sin^2 theta_W = 1 - (M_W/M_Z)^2 ~ 0.2231.
inline double sin2_weak_mixing() {
    return 1.0 - (kMW_GeV * kMW_GeV) / (kMZ_GeV * kMZ_GeV);
}

// The defining tree-level relation M_W = M_Z cos theta_W (returns M_W in GeV).
inline double w_mass_from_z() {
    return kMZ_GeV * std::sqrt(1.0 - sin2_weak_mixing());
}

// Higgs VEV from the Fermi constant: v = (sqrt(2) G_F)^(-1/2) [GeV].
inline double higgs_vev_gev() {
    return 1.0 / std::sqrt(std::sqrt(2.0) * kGF_GeV2);
}

// Yukawa coupling of a fermion of mass m_f (GeV): y_f = sqrt(2) m_f / v. The top
// quark sits at y_t ~ 1 (it is the only "natural" Yukawa).
inline double yukawa_coupling(double mass_gev) {
    return std::sqrt(2.0) * mass_gev / kHiggsVEV_GeV;
}

// Higgs quartic self-coupling lambda = m_H^2 / (2 v^2) ~ 0.13.
inline double higgs_self_coupling() {
    return kMH_GeV * kMH_GeV / (2.0 * kHiggsVEV_GeV * kHiggsVEV_GeV);
}

// ---------------------------------------------------------------------------
// Conserved quantum numbers
// ---------------------------------------------------------------------------

// Baryon number: +1/3 per quark (-1/3 per antiquark), 0 for leptons/bosons.
inline double baryon_number(particles::Particle p) {
    return particles::is_quark(p) ? (1.0 / 3.0) : 0.0;
}

// Lepton number: +1 for leptons (charged + neutrinos), 0 otherwise.
inline double lepton_number(particles::Particle p) {
    return particles::is_lepton(p) ? 1.0 : 0.0;
}

// Weak isospin third component I_3 for left-handed fields: up-type +1/2,
// down-type -1/2 (quarks and leptons alike).
inline double weak_isospin_3(particles::Particle p) {
    using particles::Particle;
    switch (p) {
    case Particle::Up:
    case Particle::Charm:
    case Particle::Top:
    case Particle::NeutrinoE:
    case Particle::NeutrinoMu:
    case Particle::NeutrinoTau:
        return +0.5;
    case Particle::Down:
    case Particle::Strange:
    case Particle::Bottom:
    case Particle::Electron:
    case Particle::Muon:
    case Particle::Tau:
        return -0.5;
    default:
        return 0.0;
    }
}

// Strong hypercharge Y from the inverted Gell-Mann-Nishijima relation, using the
// table's electric charge: Y = 2(Q - I_3).
inline double hypercharge(particles::Particle p) {
    return 2.0 * (particles::particle_info(p).charge_e - weak_isospin_3(p));
}

// Gell-Mann-Nishijima: reconstruct electric charge Q = I_3 + Y/2. Must reproduce
// the tabulated charge for every fermion (a consistency invariant).
inline double gell_mann_nishijima_charge(particles::Particle p) {
    return weak_isospin_3(p) + 0.5 * hypercharge(p);
}

// ---------------------------------------------------------------------------
// CKM quark-mixing matrix (Wolfenstein parameterisation, magnitudes)
// ---------------------------------------------------------------------------

inline constexpr double kWolfLambda = 0.22500; // ~ |V_us| = sin(Cabibbo angle)
inline constexpr double kWolfA = 0.826;
inline constexpr double kWolfRhoBar = 0.159;
inline constexpr double kWolfEtaBar = 0.348;

// |V_ij| magnitudes to O(lambda^3) in the Wolfenstein expansion.
struct CKM {
    double Vud, Vus, Vub;
    double Vcd, Vcs, Vcb;
    double Vtd, Vts, Vtb;
};

inline CKM ckm_magnitudes() {
    const double L = kWolfLambda, A = kWolfA;
    const double L2 = L * L, L3 = L2 * L;
    const double rho = kWolfRhoBar, eta = kWolfEtaBar;
    CKM m;
    m.Vud = 1.0 - 0.5 * L2;
    m.Vus = L;
    m.Vub = A * L3 * std::sqrt(rho * rho + eta * eta);
    m.Vcd = L;
    m.Vcs = 1.0 - 0.5 * L2;
    m.Vcb = A * L2;
    m.Vtd = A * L3 * std::sqrt((1.0 - rho) * (1.0 - rho) + eta * eta);
    m.Vts = A * L2;
    m.Vtb = 1.0 - 0.5 * A * A * L2 * L2;
    return m;
}

// ---------------------------------------------------------------------------
// Running gauge couplings (one loop)
// ---------------------------------------------------------------------------

inline constexpr double kLambdaQCD_GeV = 0.0887; // effective 1-loop Lambda^(5)

// Strong coupling alpha_s(Q) for n_f active flavours (default 5, the b-quark
// region): alpha_s = 1 / (b0 ln(Q^2/Lambda^2)), b0 = (33 - 2 n_f)/(12 pi).
// Exhibits asymptotic freedom: alpha_s -> 0 as Q -> infinity. ~0.118 at M_Z.
inline double alpha_s(double Q_gev, int n_f = 5, double lambda_gev = kLambdaQCD_GeV) {
    if (Q_gev <= lambda_gev)
        return INFINITY; // confinement: coupling blows up
    const double b0 = (33.0 - 2.0 * n_f) / (12.0 * pi);
    const double t = std::log((Q_gev * Q_gev) / (lambda_gev * lambda_gev));
    return 1.0 / (b0 * t);
}

// Inverse EM coupling running (one loop, leptons+quarks): 1/alpha(Q) decreases
// with energy (the coupling grows). Anchored to 1/alpha(m_e)=137.036 with an
// effective slope tuned to reproduce the measured 1/alpha(M_Z)=127.95 (the exact
// slope is set by sum_f N_c Q_f^2 over fermions above threshold plus the hadronic
// vacuum polarisation, which we fold into one effective coefficient).
inline double inverse_alpha_em(double Q_gev) {
    const double me_gev = electron_mass_kg * c2 / (1.0e9 * electron_volt_J);
    const double q = Q_gev > me_gev ? Q_gev : me_gev;
    const double slope = 0.3757; // effective one-loop coefficient (-> 127.95 at M_Z)
    return alpha_inv - 2.0 * slope * std::log(q / me_gev);
}

inline double alpha_em(double Q_gev) {
    return 1.0 / inverse_alpha_em(Q_gev);
}

} // namespace sm
} // namespace cosmos

#endif // COSMOS_STANDARDMODEL_HPP
