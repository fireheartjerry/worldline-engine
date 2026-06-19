// Verifies the scale-infrastructure data structures:
//  - Arena: aligned, reuse-after-reset, grows without freeing mid-use.
//  - SpatialHash: nearest-point query matches brute force; Morton codes order.
//  - Scheduler: binary-heap pops in nondecreasing time; adaptive run is correct.

#include "cosmos/Arena.hpp"
#include "cosmos/Scheduler.hpp"
#include "cosmos/SpatialHash.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace cosmos;

namespace {
int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_scaleinfra_verification FAILED: " << what << "\n"; ++g_failures; }
}
} // namespace

int main() {
    // --- Arena ----------------------------------------------------------------
    {
        Arena a(256);
        double* d = a.alloc<double>(10);
        check(reinterpret_cast<std::uintptr_t>(d) % alignof(double) == 0, "arena: aligned for double");
        for (int i = 0; i < 10; ++i) d[i] = i; // writable, no overlap
        // Force growth well past the initial block.
        char* big = a.alloc<char>(4096);
        check(big != nullptr && a.block_count() >= 2, "arena: grows on overflow");
        const std::size_t handed = a.bytes_handed_out();
        check(handed >= 10 * sizeof(double) + 4096, "arena: tracks bytes handed out");
        a.reset();
        check(a.bytes_handed_out() == 0, "arena: reset reclaims");
        double* d2 = a.alloc<double>(10);
        check(d2 == d, "arena: reuses the same memory after reset");
    }

    // --- SpatialHash nearest == brute force -----------------------------------
    {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> U(-500.0f, 500.0f);
        std::vector<float> xs(2000), ys(2000);
        for (std::size_t i = 0; i < xs.size(); ++i) { xs[i] = U(rng); ys[i] = U(rng); }
        SpatialHash sh;
        const float radius = 40.0f;
        sh.build(xs, ys, radius);

        int mismatches = 0;
        for (int q = 0; q < 400; ++q) {
            const float qx = U(rng), qy = U(rng);
            const int got = sh.nearest(qx, qy, radius);
            // Brute force within radius.
            int want = -1; float best = radius * radius;
            for (std::size_t i = 0; i < xs.size(); ++i) {
                const float dx = xs[i] - qx, dy = ys[i] - qy;
                const float d2 = dx * dx + dy * dy;
                if (d2 < best) { best = d2; want = static_cast<int>(i); }
            }
            if (got != want) {
                // Accept ties at identical distance (different index, same d2).
                if (want < 0 || got < 0) { ++mismatches; continue; }
                const float gdx = xs[static_cast<std::size_t>(got)] - qx, gdy = ys[static_cast<std::size_t>(got)] - qy;
                const float wdx = xs[static_cast<std::size_t>(want)] - qx, wdy = ys[static_cast<std::size_t>(want)] - qy;
                if (std::abs((gdx * gdx + gdy * gdy) - (wdx * wdx + wdy * wdy)) > 1e-3f) ++mismatches;
            }
        }
        check(mismatches == 0, "spatial hash nearest matches brute force");

        // Morton codes are monotone along a diagonal sweep.
        check(morton2d(0, 0) < morton2d(1, 1) && morton2d(1, 1) < morton2d(2, 2),
              "morton codes increase along the diagonal");
    }

    // --- Scheduler: heap order + adaptive cadence ------------------------------
    {
        Scheduler s;
        std::mt19937 rng(999);
        std::uniform_real_distribution<double> T(0.0, 100.0);
        for (std::uint32_t i = 0; i < 500; ++i) s.push(T(rng), i);
        double prev = -1.0;
        bool ordered = true;
        std::size_t popped = 0;
        while (!s.empty()) {
            const Scheduler::Event e = s.pop();
            if (e.time < prev) ordered = false;
            prev = e.time;
            ++popped;
        }
        check(ordered && popped == 500, "scheduler pops in nondecreasing time order");

        // Adaptive run: two processes ticking at different fixed periods.
        Scheduler s2;
        s2.push(0.0, 0); // fast: period 1
        s2.push(0.0, 1); // slow: period 5
        int fast = 0, slow = 0;
        s2.run_until(20.0, [&](std::uint32_t id, double t) {
            if (id == 0) { ++fast; return t + 1.0; }
            ++slow; return t + 5.0;
        });
        check(fast > slow, "adaptive scheduler ticks the fast process more often");
        check(fast >= 20 && slow >= 4, "adaptive cadence reaches expected counts");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_scaleinfra_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_scaleinfra_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
