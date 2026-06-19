#pragma once
// A uniform-grid spatial hash for 2D point picking, plus Morton (Z-order) codes
// for cache-coherent iteration. Built once per frame from the children's screen
// positions, it turns nearest-point hover/pick from an O(n) linear scan into an
// O(1)-average 3x3 neighborhood query — so a node with thousands of children
// (a galaxy of stars) stays interactive. Deterministic; no allocation per query.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cosmos {

// Interleave the low 16 bits of x and y into a 32-bit Morton (Z-order) code.
inline std::uint32_t morton2d(std::uint16_t x, std::uint16_t y) {
    auto spread = [](std::uint32_t v) -> std::uint32_t {
        v &= 0x0000FFFFu;
        v = (v | (v << 8)) & 0x00FF00FFu;
        v = (v | (v << 4)) & 0x0F0F0F0Fu;
        v = (v | (v << 2)) & 0x33333333u;
        v = (v | (v << 1)) & 0x55555555u;
        return v;
    };
    return spread(x) | (spread(y) << 1);
}

class SpatialHash {
public:
    // (Re)build from points. `cell` is the grid spacing (use the pick radius).
    void build(const std::vector<float>& xs, const std::vector<float>& ys, float cell) {
        n_ = static_cast<int>(std::min(xs.size(), ys.size()));
        cell_ = (cell > 1e-6f) ? cell : 1.0f;
        px_ = &xs;
        py_ = &ys;
        if (n_ == 0) { cells_.clear(); return; }

        minx_ = miny_ = 1e30f; float maxx = -1e30f, maxy = -1e30f;
        for (int i = 0; i < n_; ++i) {
            minx_ = std::min(minx_, xs[static_cast<std::size_t>(i)]);
            miny_ = std::min(miny_, ys[static_cast<std::size_t>(i)]);
            maxx = std::max(maxx, xs[static_cast<std::size_t>(i)]);
            maxy = std::max(maxy, ys[static_cast<std::size_t>(i)]);
        }
        cols_ = std::max(1, static_cast<int>((maxx - minx_) / cell_) + 1);
        rows_ = std::max(1, static_cast<int>((maxy - miny_) / cell_) + 1);

        // Counting sort of point indices into a CSR-style bucket layout.
        const int nc = cols_ * rows_;
        head_.assign(static_cast<std::size_t>(nc) + 1, 0);
        for (int i = 0; i < n_; ++i) ++head_[static_cast<std::size_t>(cell_index(xs[static_cast<std::size_t>(i)], ys[static_cast<std::size_t>(i)])) + 1];
        for (int c = 0; c < nc; ++c) head_[static_cast<std::size_t>(c) + 1] += head_[static_cast<std::size_t>(c)];
        cells_.resize(static_cast<std::size_t>(n_));
        std::vector<int> cursor(head_.begin(), head_.end() - 1);
        for (int i = 0; i < n_; ++i) {
            const int c = cell_index(xs[static_cast<std::size_t>(i)], ys[static_cast<std::size_t>(i)]);
            cells_[static_cast<std::size_t>(cursor[static_cast<std::size_t>(c)]++)] = i;
        }
    }

    // Nearest point within `radius` of (qx,qy); -1 if none. Scans only the 3x3
    // cell neighborhood around the query.
    int nearest(float qx, float qy, float radius) const {
        if (n_ == 0) return -1;
        const int cx = clampi(static_cast<int>((qx - minx_) / cell_), 0, cols_ - 1);
        const int cy = clampi(static_cast<int>((qy - miny_) / cell_), 0, rows_ - 1);
        int best = -1;
        float best_d2 = radius * radius;
        for (int gy = cy - 1; gy <= cy + 1; ++gy) {
            if (gy < 0 || gy >= rows_) continue;
            for (int gx = cx - 1; gx <= cx + 1; ++gx) {
                if (gx < 0 || gx >= cols_) continue;
                const int c = gy * cols_ + gx;
                for (int k = head_[static_cast<std::size_t>(c)]; k < head_[static_cast<std::size_t>(c) + 1]; ++k) {
                    const int i = cells_[static_cast<std::size_t>(k)];
                    const float dx = (*px_)[static_cast<std::size_t>(i)] - qx;
                    const float dy = (*py_)[static_cast<std::size_t>(i)] - qy;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 < best_d2) { best_d2 = d2; best = i; }
                }
            }
        }
        return best;
    }

    int size() const { return n_; }

private:
    static int clampi(int v, int lo, int hi) { return std::min(hi, std::max(lo, v)); }
    int cell_index(float x, float y) const {
        const int cx = clampi(static_cast<int>((x - minx_) / cell_), 0, cols_ - 1);
        const int cy = clampi(static_cast<int>((y - miny_) / cell_), 0, rows_ - 1);
        return cy * cols_ + cx;
    }

    const std::vector<float>* px_ = nullptr;
    const std::vector<float>* py_ = nullptr;
    std::vector<int> head_;  // CSR bucket offsets (size cols*rows+1)
    std::vector<int> cells_; // point indices grouped by cell
    int n_ = 0, cols_ = 1, rows_ = 1;
    float cell_ = 1.0f, minx_ = 0.0f, miny_ = 0.0f;
};

} // namespace cosmos
