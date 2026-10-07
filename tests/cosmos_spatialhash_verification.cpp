// SpatialHash nearest-neighbour picking vs brute force across adversarial
// layouts: radius smaller/larger than the cell, clusters and duplicates,
// degenerate (single point, all-identical, collinear) sets, spreads far wider
// than the cell (grid cap / int overflow), non-finite coordinates, queries far
// outside the grid, bad radii, and rebuilds. Plus Morton codes vs a naive
// bit interleave.

#include "cosmos/SpatialHash.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace cosmos;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "cosmos_spatialhash_verification failed: " << message << '\n';
        ++g_failures;
    }
}

float d2_of(const std::vector<float>& xs, const std::vector<float>& ys, int i, float qx, float qy) {
    const float dx = xs[static_cast<std::size_t>(i)] - qx;
    const float dy = ys[static_cast<std::size_t>(i)] - qy;
    return dx * dx + dy * dy;
}

// Reference: nearest point strictly within `radius` (same float arithmetic).
int brute_nearest(const std::vector<float>& xs, const std::vector<float>& ys, float qx, float qy,
                  float radius) {
    if (!(radius > 0.0f)) return -1;
    int best = -1;
    float best_d2 = radius * radius;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const float d2 = d2_of(xs, ys, static_cast<int>(i), qx, qy);
        if (d2 < best_d2) {
            best_d2 = d2;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// Same answer as brute force, allowing a different index only on an exact
// distance tie. Returns the number of disagreements.
int compare_queries(const SpatialHash& hash, const std::vector<float>& xs, const std::vector<float>& ys,
                    const std::vector<float>& qxs, const std::vector<float>& qys, float radius) {
    int bad = 0;
    for (std::size_t q = 0; q < qxs.size(); ++q) {
        const int got = hash.nearest(qxs[q], qys[q], radius);
        const int want = brute_nearest(xs, ys, qxs[q], qys[q], radius);
        if (got == want) continue;
        if (got < 0 || want < 0 || got >= static_cast<int>(xs.size()) ||
            d2_of(xs, ys, got, qxs[q], qys[q]) != d2_of(xs, ys, want, qxs[q], qys[q])) {
            ++bad;
        }
    }
    return bad;
}

struct Layout {
    std::vector<float> xs, ys;
};

void random_queries(std::mt19937& rng, float lo, float hi, int n, std::vector<float>& qx, std::vector<float>& qy) {
    std::uniform_real_distribution<float> U(lo, hi);
    qx.clear();
    qy.clear();
    for (int i = 0; i < n; ++i) {
        qx.push_back(U(rng));
        qy.push_back(U(rng));
    }
}

void test_radius_vs_cell() {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> U(-300.0f, 300.0f);
    Layout L;
    for (int i = 0; i < 1500; ++i) {
        L.xs.push_back(U(rng));
        L.ys.push_back(U(rng));
    }
    std::vector<float> qx, qy;
    random_queries(rng, -340.0f, 340.0f, 600, qx, qy);
    const float cell = 12.0f;
    for (float ratio : {0.25f, 0.5f, 1.0f, 1.01f, 2.5f, 7.0f, 60.0f}) {
        SpatialHash h;
        h.build(L.xs, L.ys, cell);
        const int bad = compare_queries(h, L.xs, L.ys, qx, qy, cell * ratio);
        check(bad == 0, "radius = " + std::to_string(ratio) + " x cell must match brute force (" +
                            std::to_string(bad) + " mismatches)");
    }
}

void test_clusters_and_duplicates() {
    std::mt19937 rng(99);
    std::normal_distribution<float> N(0.0f, 3.0f);
    Layout L;
    const float centers[][2] = {{0, 0}, {50, 50}, {-80, 20}, {10, -60}};
    for (int i = 0; i < 800; ++i) {
        const auto& c = centers[i % 4];
        L.xs.push_back(c[0] + N(rng));
        L.ys.push_back(c[1] + N(rng));
    }
    for (int i = 0; i < 50; ++i) { // exact duplicates
        L.xs.push_back(L.xs[static_cast<std::size_t>(i)]);
        L.ys.push_back(L.ys[static_cast<std::size_t>(i)]);
    }
    std::vector<float> qx, qy;
    random_queries(rng, -100.0f, 80.0f, 800, qx, qy);
    for (std::size_t i = 0; i < 60; ++i) { // queries exactly on points
        qx.push_back(L.xs[i]);
        qy.push_back(L.ys[i]);
    }
    SpatialHash h;
    h.build(L.xs, L.ys, 4.0f);
    check(compare_queries(h, L.xs, L.ys, qx, qy, 4.0f) == 0, "clustered points with duplicates");
    check(compare_queries(h, L.xs, L.ys, qx, qy, 1.5f) == 0, "clustered points, small radius");
}

void test_degenerate_sets() {
    std::mt19937 rng(3);
    std::vector<float> qx, qy;
    random_queries(rng, -20.0f, 20.0f, 300, qx, qy);

    SpatialHash h;
    const std::vector<float> empty;
    h.build(empty, empty, 5.0f);
    check(h.size() == 0 && h.nearest(0.0f, 0.0f, 5.0f) == -1, "empty set returns -1");

    const std::vector<float> one_x = {1.5f}, one_y = {-2.0f};
    h.build(one_x, one_y, 5.0f);
    check(compare_queries(h, one_x, one_y, qx, qy, 5.0f) == 0, "single point");
    check(h.nearest(1.5f, -2.0f, 0.1f) == 0, "a query exactly on the only point finds it");

    const std::vector<float> same_x(100, 3.0f), same_y(100, 3.0f);
    h.build(same_x, same_y, 5.0f);
    check(h.cols() == 1 && h.rows() == 1, "identical points collapse to a single cell");
    check(compare_queries(h, same_x, same_y, qx, qy, 5.0f) == 0, "all-identical points");

    std::vector<float> line_x, line_y;
    for (int i = 0; i < 200; ++i) {
        line_x.push_back(static_cast<float>(i) * 0.37f - 30.0f);
        line_y.push_back(7.0f);
    }
    h.build(line_x, line_y, 2.0f);
    check(h.rows() == 1, "collinear horizontal points use one row");
    check(compare_queries(h, line_x, line_y, qx, qy, 2.0f) == 0, "collinear points");

    // Mismatched lengths: only the common prefix is indexed.
    const std::vector<float> long_x = {0.0f, 1.0f, 2.0f}, short_y = {0.0f, 0.0f};
    h.build(long_x, short_y, 1.0f);
    check(h.size() == 2 && h.nearest(2.0f, 0.0f, 0.5f) == -1, "only min(xs, ys) points are indexed");
}

void test_huge_spread_is_capped() {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> U(-1.0e7f, 1.0e7f);
    Layout L;
    for (int i = 0; i < 300; ++i) {
        L.xs.push_back(U(rng));
        L.ys.push_back(U(rng));
    }
    // Plant tight pairs so small-radius queries have real answers.
    for (int i = 0; i < 40; ++i) {
        L.xs.push_back(L.xs[static_cast<std::size_t>(i)] + 0.5f);
        L.ys.push_back(L.ys[static_cast<std::size_t>(i)] - 0.25f);
    }
    SpatialHash h;
    h.build(L.xs, L.ys, 1.0f); // 2e7 / 1 => 4e14 cells uncapped
    const long long cells = static_cast<long long>(h.cols()) * h.rows();
    check(cells <= 2LL * 4096, "grid must stay bounded for a huge spread (got " + std::to_string(cells) + ")");
    std::vector<float> qx, qy;
    for (int i = 0; i < 40; ++i) {
        qx.push_back(L.xs[static_cast<std::size_t>(i)] + 0.1f);
        qy.push_back(L.ys[static_cast<std::size_t>(i)]);
    }
    random_queries(rng, -1.1e7f, 1.1e7f, 200, qx, qy);
    check(compare_queries(h, L.xs, L.ys, qx, qy, 1.0f) == 0, "huge spread, cell-sized radius");
    check(compare_queries(h, L.xs, L.ys, qx, qy, 2.0e6f) == 0, "huge spread, huge radius");

    // The full float range must not overflow anything.
    const float big = std::numeric_limits<float>::max();
    const std::vector<float> ex = {-big, big, 0.0f, 1.0f}, ey = {-big, big, 0.0f, 0.0f};
    h.build(ex, ey, 1.0f);
    check(static_cast<long long>(h.cols()) * h.rows() <= 2LL * 4096, "float-max spread stays bounded");
    check(h.nearest(0.9f, 0.0f, 0.5f) == 3, "float-max spread still finds near points");
    check(h.nearest(big, big, 1.0f) == 1, "float-max spread finds the extreme point");
}

void test_non_finite_coordinates() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    std::vector<float> xs = {nan, 0.0f, inf, 5.0f, -inf, 2.0f, nan};
    std::vector<float> ys = {0.0f, 0.0f, 0.0f, 5.0f, 1.0f, nan, nan};
    SpatialHash h;
    h.build(xs, ys, 2.0f);
    check(h.nearest(0.2f, 0.0f, 1.0f) == 1, "finite point found next to NaN/inf points");
    check(h.nearest(5.0f, 5.0f, 1.0f) == 3, "second finite point found");
    std::mt19937 rng(5);
    std::vector<float> qx, qy;
    random_queries(rng, -10.0f, 10.0f, 300, qx, qy);
    qx.push_back(inf);
    qy.push_back(0.0f);
    qx.push_back(-inf);
    qy.push_back(1.0f);
    for (std::size_t q = 0; q < qx.size(); ++q) {
        const int got = h.nearest(qx[q], qy[q], 50.0f);
        check(got == -1 || got == 1 || got == 3, "a non-finite point must never be returned");
    }
    check(compare_queries(h, xs, ys, qx, qy, 3.0f) == 0, "mixed finite/non-finite set");
    check(h.nearest(nan, 0.0f, 10.0f) == -1 && h.nearest(0.0f, nan, 10.0f) == -1,
          "a NaN query finds nothing");

    const std::vector<float> all_nan(10, nan);
    h.build(all_nan, all_nan, 1.0f);
    check(h.nearest(0.0f, 0.0f, 100.0f) == -1, "an all-NaN set finds nothing");
}

void test_queries_outside_and_bad_radius() {
    std::mt19937 rng(21);
    std::uniform_real_distribution<float> U(0.0f, 100.0f);
    Layout L;
    for (int i = 0; i < 400; ++i) {
        L.xs.push_back(U(rng));
        L.ys.push_back(U(rng));
    }
    SpatialHash h;
    h.build(L.xs, L.ys, 6.0f);
    std::vector<float> qx = {-5.0f, 105.0f, 50.0f, 50.0f, -1.0e6f, 1.0e6f, -3.0f, 103.0f};
    std::vector<float> qy = {50.0f, 50.0f, -5.0f, 105.0f, 1.0e6f, -1.0e6f, -3.0f, 103.0f};
    check(compare_queries(h, L.xs, L.ys, qx, qy, 6.0f) == 0, "queries just and far outside the grid");
    check(compare_queries(h, L.xs, L.ys, qx, qy, 30.0f) == 0, "outside queries with a large radius");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    check(h.nearest(50.0f, 50.0f, 0.0f) == -1, "zero radius finds nothing");
    check(h.nearest(50.0f, 50.0f, -10.0f) == -1, "negative radius finds nothing");
    check(h.nearest(50.0f, 50.0f, nan) == -1, "NaN radius finds nothing");
}

void test_rebuild_and_bad_cell() {
    std::mt19937 rng(8);
    std::uniform_real_distribution<float> U(-50.0f, 50.0f);
    Layout big;
    for (int i = 0; i < 1000; ++i) {
        big.xs.push_back(U(rng));
        big.ys.push_back(U(rng));
    }
    SpatialHash h;
    h.build(big.xs, big.ys, 3.0f);
    const std::vector<float> few_x = {10.0f, -10.0f}, few_y = {0.0f, 0.0f};
    h.build(few_x, few_y, 3.0f);
    std::vector<float> qx, qy;
    random_queries(rng, -60.0f, 60.0f, 300, qx, qy);
    check(compare_queries(h, few_x, few_y, qx, qy, 3.0f) == 0, "a rebuild must forget the previous points");

    for (float cell : {0.0f, -4.0f, std::numeric_limits<float>::quiet_NaN()}) {
        h.build(big.xs, big.ys, cell);
        check(compare_queries(h, big.xs, big.ys, qx, qy, 2.0f) == 0,
              "a non-positive/NaN cell must fall back to a valid grid");
    }
}

std::uint32_t naive_morton(std::uint16_t x, std::uint16_t y) {
    std::uint32_t code = 0;
    for (int b = 0; b < 16; ++b) {
        code |= static_cast<std::uint32_t>((x >> b) & 1u) << (2 * b);
        code |= static_cast<std::uint32_t>((y >> b) & 1u) << (2 * b + 1);
    }
    return code;
}

void test_morton() {
    std::mt19937 rng(17);
    std::uniform_int_distribution<int> U(0, 65535);
    int bad = 0;
    for (int i = 0; i < 5000; ++i) {
        const auto x = static_cast<std::uint16_t>(U(rng));
        const auto y = static_cast<std::uint16_t>(U(rng));
        if (morton2d(x, y) != naive_morton(x, y)) ++bad;
    }
    check(bad == 0, "morton2d must equal a naive bit interleave");
    check(morton2d(65535, 65535) == 0xFFFFFFFFu && morton2d(0, 0) == 0u, "morton extremes");
    check(morton2d(1, 0) == 1u && morton2d(0, 1) == 2u, "x occupies the even bits, y the odd bits");
}

} // namespace

int main() {
    test_radius_vs_cell();
    test_clusters_and_duplicates();
    test_degenerate_sets();
    test_huge_spread_is_capped();
    test_non_finite_coordinates();
    test_queries_outside_and_bad_radius();
    test_rebuild_and_bad_cell();
    test_morton();
    if (g_failures > 0) {
        std::cerr << g_failures << " spatial hash check(s) failed\n";
        return EXIT_FAILURE;
    }
    return 0;
}
