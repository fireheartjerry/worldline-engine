//  Diagnostic: measures the generated-universe corpus the way the metaspec
//  verification test does, and classifies force-topology diversity so the
//  generator can be tuned for both test compliance and visual variety.

#include "seed/MetaSpec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
double sqr(double v) { return v * v; }
double frob2(const double m[2][2]) { return std::sqrt(sqr(m[0][0]) + sqr(m[0][1]) + sqr(m[1][0]) + sqr(m[1][1])); }
double norm2(const double v[2]) { return std::sqrt(sqr(v[0]) + sqr(v[1])); }
double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void eig_sym(const double m[2][2], double& lo, double& hi) {
    const double a = m[0][0], b = 0.5 * (m[0][1] + m[1][0]), d = m[1][1];
    const double tr = a + d, disc = std::sqrt(std::max(0.0, (a - d) * (a - d) + 4 * b * b));
    hi = 0.5 * (tr + disc); lo = 0.5 * (tr - disc);
}
}

int main() {
    std::vector<double> w_n, c_n, s_n, tg_n, gv_n, p_abs, aniso_g, aniso_v;
    int near_zero_w = 0, strong_combo = 0, extreme = 0;
    int well = 0, saddle = 0, repel = 0, strong_rot = 0, strong_warp = 0, strong_aniso = 0;

    const int M = 200;
    for (int i = 0; i < M; ++i) {
        const MetaSpec ms = generate_meta_spec("richness-seed-" + std::to_string(i));
        const double w = frob2(ms.W);
        const double c = frob2(ms.C[0]) + frob2(ms.C[1]);
        const double s = frob2(ms.S);
        const double tg = std::abs(ms.T[0][1]) + std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]);
        const double gv = frob2(ms.g) + frob2(ms.V);
        const double qd = norm2(ms.qdot0);
        const double extreme_score =
            0.32 * std::min(1.0, w / 0.24) + 0.20 * std::min(1.0, tg / 0.42) +
            0.20 * std::min(1.0, c / 1.35) + 0.14 * std::min(1.0, qd / 1.30) +
            0.14 * std::min(1.0, std::abs(ms.p) / 4.5);
        w_n.push_back(w); c_n.push_back(c); s_n.push_back(s); tg_n.push_back(tg);
        gv_n.push_back(gv); p_abs.push_back(std::abs(ms.p));
        if (w < 0.02) ++near_zero_w;
        if (c + tg + w > 2.9) ++strong_combo;
        if (extreme_score > 0.40) ++extreme;

        // topology proxy from the potential V eigenvalues + rotation/warp/anisotropy
        double vlo, vhi; eig_sym(ms.V, vlo, vhi);
        double glo, ghi; eig_sym(ms.g, glo, ghi);
        if (vlo > 0.12 && vhi > 0.12) ++well;
        else if (vlo < -0.12 && vhi < -0.12) ++repel;
        else if (vlo < -0.12 && vhi > 0.12) ++saddle;
        const double ag = std::abs(ghi - glo) / (std::abs(ghi) + std::abs(glo) + 1e-9);
        const double av = std::abs(vhi - vlo) / (std::abs(vhi) + std::abs(vlo) + 1e-9);
        aniso_g.push_back(ag); aniso_v.push_back(av);
        if (tg > 0.9) ++strong_rot;
        if (w > 0.18) ++strong_warp;
        if (ag > 0.35) ++strong_aniso;
    }

    std::printf("=== corpus (%d seeds) ===\n", M);
    std::printf("median w  = %.4f   (need >0.02, < median c)\n", median(w_n));
    std::printf("median c  = %.4f\n", median(c_n));
    std::printf("median s  = %.4f   (need >0.03)\n", median(s_n));
    std::printf("median tg = %.4f   (need < median gv)\n", median(tg_n));
    std::printf("median gv = %.4f\n", median(gv_n));
    std::printf("median|p| = %.4f\n", median(p_abs));
    std::printf("near_zero_w  = %d    (need <60)\n", near_zero_w);
    std::printf("strong_combo = %d    (need <30)\n", strong_combo);
    std::printf("EXTREME      = %d    (need in [1,80])\n", extreme);
    std::printf("--- topology variety ---\n");
    std::printf("wells=%d  saddles=%d  repellers=%d  | strong_rot=%d  strong_warp=%d  strong_aniso=%d\n",
                well, saddle, repel, strong_rot, strong_warp, strong_aniso);
    std::printf("median aniso_g=%.3f  aniso_v=%.3f\n", median(aniso_g), median(aniso_v));

    // per-term breakdown of the extreme score so tuning is precise, not guessed
    std::vector<double> tw, tt, tc, tq, tp, ts;
    for (int i = 0; i < M; ++i) {
        const MetaSpec ms = generate_meta_spec("richness-seed-" + std::to_string(i));
        const double w = frob2(ms.W), c = frob2(ms.C[0]) + frob2(ms.C[1]);
        const double tg = std::abs(ms.T[0][1]) + std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]);
        const double qd = norm2(ms.qdot0);
        tw.push_back(0.32 * std::min(1.0, w / 0.24));
        tt.push_back(0.20 * std::min(1.0, tg / 0.42));
        tc.push_back(0.20 * std::min(1.0, c / 1.35));
        tq.push_back(0.14 * std::min(1.0, qd / 1.30));
        tp.push_back(0.14 * std::min(1.0, std::abs(ms.p) / 4.5));
        ts.push_back(tw.back() + tt.back() + tc.back() + tq.back() + tp.back());
    }
    std::printf("--- extreme_score term medians (threshold 0.40) ---\n");
    std::printf("w=%.3f tg=%.3f c=%.3f qd=%.3f p=%.3f | TOTAL median=%.3f\n",
                median(tw), median(tt), median(tc), median(tq), median(tp), median(ts));
    return 0;
}
