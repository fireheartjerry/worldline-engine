// Verifies the Cosmography cosmic-web layout module: generation is deterministic
// for a given seed, density classification follows the standard void/wall/
// filament/node bands, a sampled web stays inside its box and contains a real
// mix of morphologies (not a uniform scatter), the void volume fraction is in
// the void-dominated cosmic range, the BAO scale and two-point excess match the
// CosmoStats reference, and cluster masses for nodes are cluster-scale and grow
// with overdensity. All outputs are finite.

#include "cosmos/Cosmography.hpp"
#include "cosmos/CosmoStats.hpp"
#include "cosmos/Spectrum.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cosmos;
using namespace cosmos::cosmoweb;

namespace {
int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "cosmos_cosmography_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
bool finite3(const WebPoint& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
           std::isfinite(p.density_contrast);
}
} // namespace

int main() {
    // --- Determinism: same seed -> identical point set ------------------------
    {
        const auto a = sample_cosmic_web(0xABCDEF123u, 2000, 150.0);
        const auto b = sample_cosmic_web(0xABCDEF123u, 2000, 150.0);
        check(a.size() == b.size(), "determinism: same point count");
        bool identical = (a.size() == b.size());
        for (std::size_t i = 0; identical && i < a.size(); ++i) {
            if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z ||
                a[i].cls != b[i].cls) {
                identical = false;
            }
        }
        check(identical, "determinism: identical positions and classes");

        // A different seed should give a different layout.
        const auto c = sample_cosmic_web(0x999u, 2000, 150.0);
        bool differs = false;
        for (std::size_t i = 0; i < a.size() && i < c.size(); ++i) {
            if (a[i].x != c[i].x || a[i].y != c[i].y) { differs = true; break; }
        }
        check(differs, "different seed -> different layout");
    }

    // --- classify_density: band boundaries and ordering -----------------------
    {
        check(classify_density(-0.8) == WebClass::Void, "delta=-0.8 -> Void");
        check(classify_density(0.0) == WebClass::Wall, "delta=0 -> Wall");
        check(classify_density(2.0) == WebClass::Filament, "delta=2 -> Filament");
        check(classify_density(10.0) == WebClass::Node, "delta=10 -> Node");

        // Monotone ordering of thresholds: as delta increases, class only climbs.
        WebClass prev = classify_density(-5.0);
        bool monotone = true;
        for (double d = -5.0; d <= 20.0; d += 0.05) {
            const WebClass cur = classify_density(d);
            if (static_cast<int>(cur) < static_cast<int>(prev)) monotone = false;
            prev = cur;
        }
        check(monotone, "classify_density is monotone in delta");
        check(static_cast<int>(WebClass::Void) < static_cast<int>(WebClass::Wall) &&
                  static_cast<int>(WebClass::Wall) < static_cast<int>(WebClass::Filament) &&
                  static_cast<int>(WebClass::Filament) < static_cast<int>(WebClass::Node),
              "WebClass enum ordered by density");
    }

    // --- Sampled web: bounded, finite, and a real mix of morphologies ---------
    {
        const double box = 200.0;
        const auto web = sample_cosmic_web(0x5EED01u, 4000, box);
        check(web.size() == 4000u, "sample_cosmic_web returns requested count");

        bool in_box = true, all_finite = true;
        int n_void = 0, n_wall = 0, n_fil = 0, n_node = 0;
        for (const auto& p : web) {
            if (!finite3(p)) all_finite = false;
            if (p.x < 0.0f || p.x > static_cast<float>(box) ||
                p.y < 0.0f || p.y > static_cast<float>(box) ||
                p.z < 0.0f || p.z > static_cast<float>(box)) {
                in_box = false;
            }
            if (p.density_contrast < -1.0 - 1e-9) all_finite = false; // delta >= -1
            switch (p.cls) {
                case WebClass::Void:     ++n_void; break;
                case WebClass::Wall:     ++n_wall; break;
                case WebClass::Filament: ++n_fil;  break;
                case WebClass::Node:     ++n_node; break;
            }
        }
        check(in_box, "all points within the box");
        check(all_finite, "all points finite with delta >= -1");
        check(n_void > 0, "web contains some Void points");
        check(n_fil + n_node > 0, "web contains some Filament/Node points (real web)");

        const double vf = void_volume_fraction(web);
        check(vf >= 0.4 && vf <= 0.9, "void volume fraction in [0.4, 0.9]");
        std::cerr << "info: void fraction = " << vf
                  << "  (void/wall/fil/node = " << n_void << "/" << n_wall << "/"
                  << n_fil << "/" << n_node << ")\n";

        // void_volume_fraction of an all-void / empty set behaves sanely.
        check(void_volume_fraction({}) == 0.0, "empty web -> void fraction 0");
    }

    // --- BAO scale and two-point excess --------------------------------------
    {
        const double bao = bao_scale_mpc();
        check(std::isfinite(bao) && bao > 140.0 && bao < 155.0, "BAO scale ~147 Mpc");
        check(bao == cstat::kRdragMpc, "BAO scale exposes kRdragMpc");

        const double xi5 = two_point_excess(5.0);
        check(std::abs(xi5 - 1.0) < 1e-9, "two_point_excess ~1 at r=5 Mpc");

        // Decreasing with separation.
        bool decreasing = true;
        double prev = two_point_excess(1.0);
        for (double r = 2.0; r <= 100.0; r += 1.0) {
            const double cur = two_point_excess(r);
            if (!std::isfinite(cur) || cur >= prev) decreasing = false;
            prev = cur;
        }
        check(decreasing, "two_point_excess strictly decreasing with r");
    }

    // --- Cluster mass: node-scale and monotone in delta -----------------------
    {
        Stream rng(0xC1057E12u, 0x1u);
        bool in_cluster_range = true;
        for (int i = 0; i < 5000; ++i) {
            const double m = cluster_mass_msun(8.0, rng); // a Node overdensity
            if (!(m >= 1.0e13 && m <= 2.0e15) || !std::isfinite(m)) {
                in_cluster_range = false;
            }
        }
        check(in_cluster_range, "Node cluster mass in [1e13, 2e15] Msun");

        // Larger delta should not give a smaller mass on average (the boost is
        // monotone): compare means at delta=5 vs delta=20 with a fixed stream.
        Stream r1(0xDEADu, 7u), r2(0xDEADu, 7u);
        double sum_low = 0.0, sum_high = 0.0;
        const int N = 20000;
        for (int i = 0; i < N; ++i) sum_low += cluster_mass_msun(5.0, r1);
        for (int i = 0; i < N; ++i) sum_high += cluster_mass_msun(20.0, r2);
        check(sum_high >= sum_low, "larger delta -> not-smaller cluster mass");

        // Non-node points get smaller (group-scale) masses.
        Stream rg(0x4242u, 3u);
        double max_group = 0.0;
        for (int i = 0; i < 5000; ++i) {
            const double m = cluster_mass_msun(0.0, rg); // Wall-class delta
            if (m > max_group) max_group = m;
            if (!std::isfinite(m)) in_cluster_range = false;
        }
        check(max_group < 1.0e14, "non-node masses are sub-cluster scale");
        check(in_cluster_range, "all sampled masses finite");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_cosmography_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_cosmography_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
