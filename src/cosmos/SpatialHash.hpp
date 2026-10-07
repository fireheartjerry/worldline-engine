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
    // The hash keeps pointers to `xs`/`ys` (no copy): they must outlive, and not
    // be modified between, build() and the nearest() queries that follow it.
    //
    // Robust to hostile layouts: non-finite coordinates are ignored for the
    // bounds (and can never be returned), and when the points are spread far
    // wider than `cell` the cell is grown so the grid stays at most
    // max(4096, 4n) cells -- bounded memory, no int overflow in cols*rows.
    // Growing the cell never affects correctness: nearest() scans as many cell
    // rings as the radius needs.
    void build(const std::vector<float>& xs, const std::vector<float>& ys, float cell) {
        n_ = static_cast<int>(std::min(xs.size(), ys.size()));
        cell_ = (cell > 1e-6f) ? cell : 1.0f; // also rejects NaN
        px_ = &xs;
        py_ = &ys;
        cols_ = rows_ = 1;
        minx_ = miny_ = 0.0f;
        if (n_ == 0) { head_.assign(2, 0); cells_.clear(); return; }

        float maxx = 0.0f, maxy = 0.0f;
        bool any_finite = false;
        for (int i = 0; i < n_; ++i) {
            const float x = xs[static_cast<std::size_t>(i)];
            const float y = ys[static_cast<std::size_t>(i)];
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            if (!any_finite) { minx_ = maxx = x; miny_ = maxy = y; any_finite = true; continue; }
            minx_ = std::min(minx_, x);
            miny_ = std::min(miny_, y);
            maxx = std::max(maxx, x);
            maxy = std::max(maxy, y);
        }

        // Spans in double: float max - (-float max) overflows float.
        const double span_x = static_cast<double>(maxx) - static_cast<double>(minx_);
        const double span_y = static_cast<double>(maxy) - static_cast<double>(miny_);
        const double max_cells = static_cast<double>(std::max(4096, 4 * n_));
        double c = static_cast<double>(cell_);
        while ((std::floor(span_x / c) + 1.0) * (std::floor(span_y / c) + 1.0) > max_cells) c *= 2.0;
        cell_ = static_cast<float>(c);
        // Same float expression as the cell mapping uses (so the last column is
        // exactly where col_of() puts the max point); the double span is only a
        // fallback when the float one overflows.
        const float fcols = (maxx - minx_) / cell_;
        const float frows = (maxy - miny_) / cell_;
        const auto extent = [&](float f, double span) {
            const double v = (std::isfinite(f) && static_cast<double>(f) < max_cells)
                                 ? static_cast<double>(f)
                                 : std::min(max_cells, span / static_cast<double>(cell_));
            return std::max(1, static_cast<int>(v) + 1);
        };
        cols_ = extent(fcols, span_x);
        rows_ = extent(frows, span_y);

        // Counting sort of point indices into a CSR-style bucket layout.
        const int nc = cols_ * rows_;
        head_.assign(static_cast<std::size_t>(nc) + 1, 0);
        for (int i = 0; i < n_; ++i) ++head_[static_cast<std::size_t>(cell_index(xs[static_cast<std::size_t>(i)], ys[static_cast<std::size_t>(i)])) + 1];
        for (int c2 = 0; c2 < nc; ++c2) head_[static_cast<std::size_t>(c2) + 1] += head_[static_cast<std::size_t>(c2)];
        cells_.resize(static_cast<std::size_t>(n_));
        cursor_.assign(head_.begin(), head_.end() - 1);
        for (int i = 0; i < n_; ++i) {
            const int c2 = cell_index(xs[static_cast<std::size_t>(i)], ys[static_cast<std::size_t>(i)]);
            cells_[static_cast<std::size_t>(cursor_[static_cast<std::size_t>(c2)]++)] = i;
        }
    }

    // Nearest point strictly within `radius` of (qx,qy); -1 if none (or if the
    // radius is not positive). Scans only the cells within ceil(radius / cell)
    // rings of the query cell -- the classic 3x3 neighborhood when the radius
    // does not exceed the cell size, as in normal use.
    int nearest(float qx, float qy, float radius) const {
        if (n_ == 0 || !(radius > 0.0f)) return -1;
        const int cx = col_of(qx);
        const int cy = row_of(qy);
        const float ring_f = std::ceil(radius / cell_);
        const int span = std::max(cols_, rows_);
        const int rings = (ring_f >= static_cast<float>(span)) ? span : std::max(1, static_cast<int>(ring_f));
        int best = -1;
        float best_d2 = radius * radius;
        const int gy0 = std::max(0, cy - rings), gy1 = std::min(rows_ - 1, cy + rings);
        const int gx0 = std::max(0, cx - rings), gx1 = std::min(cols_ - 1, cx + rings);
        for (int gy = gy0; gy <= gy1; ++gy) {
            for (int gx = gx0; gx <= gx1; ++gx) {
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
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    float cell_size() const { return cell_; }

private:
    // Grid column/row of a coordinate, clamped to the grid. The comparison is
    // done in float *before* the int conversion, so NaN/inf/huge coordinates
    // cannot hit an out-of-range float->int cast (undefined behavior).
    int col_of(float x) const {
        const float f = (x - minx_) / cell_;
        if (!(f > 0.0f)) return 0;
        if (f >= static_cast<float>(cols_)) return cols_ - 1;
        return static_cast<int>(f);
    }
    int row_of(float y) const {
        const float f = (y - miny_) / cell_;
        if (!(f > 0.0f)) return 0;
        if (f >= static_cast<float>(rows_)) return rows_ - 1;
        return static_cast<int>(f);
    }
    int cell_index(float x, float y) const { return row_of(y) * cols_ + col_of(x); }

    const std::vector<float>* px_ = nullptr;
    const std::vector<float>* py_ = nullptr;
    std::vector<int> head_;   // CSR bucket offsets (size cols*rows+1)
    std::vector<int> cells_;  // point indices grouped by cell
    std::vector<int> cursor_; // build() scratch, kept to avoid a per-frame allocation
    int n_ = 0, cols_ = 1, rows_ = 1;
    float cell_ = 1.0f, minx_ = 0.0f, miny_ = 0.0f;
};

} // namespace cosmos
