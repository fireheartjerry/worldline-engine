#pragma once
//  ─────────────────────────────────────────────────────────────────────────
//  FlowOperator — a lean, branch-light, alloc-free float realisation of the
//  generated law's force field.  It mirrors LawSpec::derivative closely enough
//  to be visually faithful, but is built for advecting tens of thousands of
//  test masses per frame (SoA-friendly, trivially vectorisable, fast
//  transcendental approximations for the nonlinear potential weights).
//
//  Pure math — no raylib — so it can be reused by the live FieldRenderer and
//  by the offline preview tool alike.
//  ─────────────────────────────────────────────────────────────────────────

#include "physics/LawSpec.hpp"
#include "seed/MetaSpec.hpp"

#include <cmath>
#include <cstdint>

namespace wlfield {

// ── fast math (Mineiro-style approximations) ────────────────────────────────
inline float fast_log2(float x) {
    union { float f; std::uint32_t i; } vx = {x};
    union { std::uint32_t i; float f; } mx = {(vx.i & 0x007FFFFFu) | 0x3f000000u};
    float y = static_cast<float>(vx.i) * 1.1920928955078125e-7f;
    return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

inline float fast_pow2(float p) {
    float clipp = (p < -126.0f) ? -126.0f : p;
    float z = clipp - static_cast<float>(static_cast<int>(clipp)) + (clipp < 0.0f ? 1.0f : 0.0f);
    union { std::uint32_t i; float f; } v = {static_cast<std::uint32_t>(
        (1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))};
    return v.f;
}

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

// xorshift32 — tiny, fast, deterministic.
struct Rng {
    std::uint32_t s = 0x9E3779B9u;
    explicit Rng(std::uint32_t seed) : s(seed ? seed : 0x1234567u) {}
    std::uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float sym() { return unit() * 2.0f - 1.0f; }
};

// Precomputed force operator — mirrors LawSpec::derivative in float.
struct FlowOperator {
    float g[4]   = {1, 0, 0, 1};
    float ginv[4]= {1, 0, 0, 1};
    float Pb[4]  = {0, 0, 0, 0};   // bounded potential (V + s_a*S)
    float C0[4]  = {0, 0, 0, 0};
    float C1[4]  = {0, 0, 0, 0};
    float Tsk[4] = {0, 0, 0, 0};
    float G0[4]  = {0, 0, 0, 0};
    float G1[4]  = {0, 0, 0, 0};
    float W[4]   = {0, 0, 0, 0};
    float S[4]   = {0, 0, 0, 0};
    float gain   = 1.0f;
    float s_b    = 0.0f;
    float s_c    = 0.0f;
    float p      = 0.0f;
    float ceiling = 16.0f;

    static void load4(const double s[2][2], float d[4]) {
        d[0] = static_cast<float>(s[0][0]); d[1] = static_cast<float>(s[0][1]);
        d[2] = static_cast<float>(s[1][0]); d[3] = static_cast<float>(s[1][1]);
    }

    static void mv(const float m[4], float x, float y, float& ox, float& oy) {
        ox = m[0] * x + m[1] * y;
        oy = m[2] * x + m[3] * y;
    }

    void configure(const LawSpec& law) {
        const MetaSpec& m = law.meta_spec();
        load4(m.g, g);
        const double det = static_cast<double>(g[0]) * g[3] - static_cast<double>(g[1]) * g[2];
        const double sd = (std::abs(det) < 1.0e-9) ? (det >= 0 ? 1.0e-9 : -1.0e-9) : det;
        ginv[0] = static_cast<float>(g[3] / sd);
        ginv[1] = static_cast<float>(-g[1] / sd);
        ginv[2] = static_cast<float>(-g[2] / sd);
        ginv[3] = static_cast<float>(g[0] / sd);
        Pb[0] = static_cast<float>(m.V[0][0] + m.s_a * m.S[0][0]);
        Pb[1] = static_cast<float>(m.V[0][1] + m.s_a * m.S[0][1]);
        Pb[2] = static_cast<float>(m.V[1][0] + m.s_a * m.S[1][0]);
        Pb[3] = static_cast<float>(m.V[1][1] + m.s_a * m.S[1][1]);
        load4(m.C[0], C0);
        load4(m.C[1], C1);
        load4(m.T, Tsk);
        load4(m.G[0], G0);
        load4(m.G[1], G1);
        load4(m.W, W);
        load4(m.S, S);
        gain = static_cast<float>(law.potential_linear_gain());
        s_b = static_cast<float>(m.s_b);
        s_c = static_cast<float>(m.s_c);
        p = static_cast<float>(law.seeded_p());
        ceiling = static_cast<float>(law.acceleration_ceiling());
    }

    // a = M^-1 · force(q,v).  Faithful but lean.
    inline void accel(float qx, float qy, float vx, float vy,
                      float& ax, float& ay) const {
        float gqx, gqy, gvx, gvy;
        mv(g, qx, qy, gqx, gqy);
        mv(g, vx, vy, gvx, gvy);
        const float q_sq = qx * gqx + qy * gqy;
        const float v_sq = vx * gvx + vy * gvy;
        const float q_g = std::sqrt(q_sq > 0.0f ? q_sq : 0.0f);
        const float v_g = std::sqrt(v_sq > 0.0f ? v_sq : 0.0f);
        const float q_ratio = q_g / (q_g + 1.0f);
        const float base = 0.75f + 0.5f * q_ratio;
        const float lb = fast_log2(base);
        const float pweight = fast_pow2(p * lb);
        const float posweight = fast_pow2((p < 0.0f ? -p : p) * lb);
        const float velweight = v_g / (v_g + 0.5f + q_g);
        const float ang = qx * vy - qy * vx;
        const float agate = ang / (q_g * v_g + 1.0f + (ang < 0.0f ? -ang : ang));

        float fx = 0.0f, fy = 0.0f;
        float tx, ty;

        mv(Pb, qx, qy, tx, ty);
        const float pk = gain * pweight;
        fx -= tx * pk; fy -= ty * pk;

        mv(C0, qx, qy, tx, ty);
        fx += tx * posweight; fy += ty * posweight;

        mv(C1, vx, vy, tx, ty);
        tx *= velweight; ty *= velweight;
        const float c1power = tx * vx + ty * vy;
        if (c1power > 0.0f) {
            const float veucl = vx * vx + vy * vy + 1.0e-9f;
            const float k = 0.82f * c1power / veucl;
            fx += tx - vx * k; fy += ty - vy * k;
        } else {
            fx += tx; fy += ty;
        }

        mv(Tsk, vx, vy, tx, ty); fx += tx; fy += ty;
        mv(G0, vx, vy, tx, ty);  fx += tx * posweight; fy += ty * posweight;
        mv(G1, vx, vy, tx, ty);  fx += tx * velweight; fy += ty * velweight;
        mv(W, qx, qy, tx, ty);   fx += tx * agate; fy += ty * agate;

        float sqx, sqy;
        mv(S, qx, qy, sqx, sqy);
        fx += sqy * s_c; fy += -sqx * s_c;

        if (s_b > 0.0f) {
            const float axis2 = sqx * sqx + sqy * sqy;
            if (axis2 > 1.0e-9f) {
                const float proj = (fx * sqx + fy * sqy) / axis2;
                const float bx = fx - proj * sqx;
                const float by = fy - proj * sqy;
                fx -= bx * s_b; fy -= by * s_b;
            }
        }

        if (q_sq > 8.0f) { const float c = 0.12f * (q_sq - 8.0f); fx -= gqx * c; fy -= gqy * c; }
        if (v_sq > 16.0f) { const float c = 0.12f * (v_sq - 16.0f); fx -= gvx * c; fy -= gvy * c; }

        mv(ginv, fx, fy, ax, ay);

        const float a2 = ax * ax + ay * ay;
        const float lim = ceiling * 1.6f;
        if (a2 > lim * lim) {
            const float s = lim / std::sqrt(a2);
            ax *= s; ay *= s;
        }
    }
};

} // namespace wlfield
