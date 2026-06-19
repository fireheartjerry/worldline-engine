// Verifies cosmos/StandardModel.hpp: electroweak relations (weak mixing angle,
// M_W = M_Z cos theta_W, Higgs VEV from G_F, Yukawa couplings, self-coupling),
// conserved quantum numbers + the Gell-Mann-Nishijima charge formula, CKM
// magnitudes & near-unitarity, and one-loop coupling running (asymptotic freedom,
// alpha_s(M_Z) ~ 0.118, alpha_em growing with energy).

#include "cosmos/ParticleData.hpp"
#include "cosmos/StandardModel.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::sm;
namespace pd = cosmos::particles;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_standardmodel_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double rel, const std::string &what) {
    const double denom = std::abs(want) > 0.0 ? std::abs(want) : 1.0;
    const double err = std::abs(got - want) / denom;
    if (err > rel) {
        std::cerr << "cosmos_standardmodel_verification FAILED: " << what << " got=" << got
                  << " want=" << want << " rel=" << err << "\n";
        ++g_failures;
    }
}
} // namespace

int main() {
    // --- Electroweak sector -------------------------------------------------
    close(sin2_weak_mixing(), 0.2231, 2e-2, "on-shell sin^2 theta_W ~ 0.223");
    close(w_mass_from_z(), kMW_GeV, 1e-9, "M_W = M_Z cos theta_W (self-consistent)");
    close(higgs_vev_gev(), 246.22, 1e-3, "Higgs VEV from G_F");
    close(higgs_vev_gev(), kHiggsVEV_GeV, 1e-4, "VEV constant matches formula");
    // Top Yukawa is ~1 (the only O(1) Yukawa); electron Yukawa is tiny.
    close(yukawa_coupling(172.57), 0.991, 2e-3, "top Yukawa ~ 1");
    check(yukawa_coupling(0.000511) < 1e-5, "electron Yukawa is tiny");
    check(yukawa_coupling(172.57) > yukawa_coupling(4.18), "heavier fermion, larger Yukawa");
    close(higgs_self_coupling(), 0.1293, 1e-2, "Higgs self-coupling lambda ~ 0.13");

    // --- Quantum numbers & Gell-Mann-Nishijima ------------------------------
    close(baryon_number(pd::Particle::Up), 1.0 / 3.0, 1e-12, "quark B = 1/3");
    close(baryon_number(pd::Particle::Electron), 0.0, 1e-12, "lepton B = 0");
    close(lepton_number(pd::Particle::Electron), 1.0, 1e-12, "electron L = 1");
    close(lepton_number(pd::Particle::Up), 0.0, 1e-12, "quark L = 0");
    close(weak_isospin_3(pd::Particle::Up), +0.5, 1e-12, "up I_3 = +1/2");
    close(weak_isospin_3(pd::Particle::Down), -0.5, 1e-12, "down I_3 = -1/2");
    // Gell-Mann-Nishijima must reproduce the tabulated electric charge for EVERY
    // fundamental fermion.
    for (int i = 0; i < static_cast<int>(pd::Particle::Count); ++i) {
        const auto p = static_cast<pd::Particle>(i);
        if (!pd::is_quark(p) && !pd::is_lepton(p))
            continue;
        close(gell_mann_nishijima_charge(p), pd::particle_info(p).charge_e, 1e-9,
              std::string("Q = I_3 + Y/2 for ") + pd::particle_info(p).name);
    }

    // --- CKM matrix ---------------------------------------------------------
    const CKM v = ckm_magnitudes();
    close(v.Vud, 0.9743, 3e-3, "|V_ud| ~ 0.974");
    close(v.Vus, 0.2250, 3e-3, "|V_us| ~ 0.225 (Cabibbo)");
    close(v.Vcb, 0.0418, 1e-1, "|V_cb| ~ 0.041");
    check(v.Vtb > 0.99, "|V_tb| ~ 1 (third generation nearly decoupled)");
    check(v.Vud > v.Vus && v.Vus > v.Vcb && v.Vcb > v.Vub, "CKM hierarchy Vud > Vus > Vcb > Vub");
    // First row is nearly unitary: |Vud|^2 + |Vus|^2 + |Vub|^2 ~ 1.
    close(v.Vud * v.Vud + v.Vus * v.Vus + v.Vub * v.Vub, 1.0, 5e-3, "CKM first row unitarity");

    // --- Running couplings --------------------------------------------------
    // Strong coupling: ~0.118 at M_Z, asymptotic freedom (falls with energy).
    close(alpha_s(kMZ_GeV), 0.118, 6e-2, "alpha_s(M_Z) ~ 0.118");
    check(alpha_s(1000.0) < alpha_s(kMZ_GeV), "asymptotic freedom: alpha_s falls with Q");
    check(alpha_s(2.0) > alpha_s(kMZ_GeV), "alpha_s grows toward low energy (confinement)");
    check(std::isinf(alpha_s(0.05)), "alpha_s diverges below Lambda_QCD");
    // EM coupling grows with energy: 1/alpha drops from 137 to ~128 at M_Z.
    close(inverse_alpha_em(0.000511), 137.036, 1e-3, "1/alpha(m_e) = 137.04");
    close(inverse_alpha_em(kMZ_GeV), 127.95, 5e-2, "1/alpha(M_Z) ~ 128");
    check(alpha_em(kMZ_GeV) > alpha_em(0.000511), "alpha_em grows with energy");

    // --- Determinism --------------------------------------------------------
    check(alpha_s(kMZ_GeV) == alpha_s(kMZ_GeV), "alpha_s deterministic");
    check(higgs_vev_gev() == higgs_vev_gev(), "VEV deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_standardmodel_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_standardmodel_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
