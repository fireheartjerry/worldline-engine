//  Offline preview of the live FieldRenderer aesthetic — no GPU/window.
//  Advects the swarm through the *real* generated law (LawSpec + the shared
//  FlowOperator) and writes a tonemapped 24-bit BMP so the flow structure of
//  several seeds can be eyeballed and tuned side by side.
//
//  Usage:  worldline_field_preview [--out file.bmp] [--size N] [seed ...]

#include "physics/LawSpec.hpp"
#include "renderer/FlowOperator.hpp"
#include "seed/MetaSpec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct V3 { float r = 0, g = 0, b = 0; };

V3 hsv2rgb(float h, float s, float v) {
    h = h - 360.0f * std::floor(h / 360.0f);
    const float c = v * s;
    const float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60)      { r = c; g = x; }
    else if (h < 120){ r = x; g = c; }
    else if (h < 180){ g = c; b = x; }
    else if (h < 240){ g = x; b = c; }
    else if (h < 300){ r = x; b = c; }
    else             { r = c; b = x; }
    return {r + m, g + m, b + m};
}

struct Palette { V3 cool, mid, hot; };

Palette make_palette(const MetaSpec& m, const std::string& seed) {
    const double a = m.g[0][0], b = 0.5 * (m.g[0][1] + m.g[1][0]), d = m.g[1][1];
    const double theta = 0.5 * std::atan2(2.0 * b, a - d);
    std::uint64_t hh = 1469598103934665603ull;
    for (char ch : seed) { hh ^= static_cast<unsigned char>(ch); hh *= 1099511628257ull; }
    const float hash_hue = static_cast<float>(hh % 360u);
    const double aniso = std::abs(a - d) / (std::abs(a) + std::abs(d) + 1.0e-9);
    const double pmag = std::min(1.0, std::abs(m.p) / 4.5);
    const float base_hue = static_cast<float>(theta * (180.0 / 3.14159265) * 2.0) + hash_hue;
    const float spread = static_cast<float>(40.0 + 120.0 * aniso + 90.0 * pmag);
    Palette p;
    p.cool = hsv2rgb(base_hue - spread * 0.5f, 0.85f, 0.95f);
    p.mid = hsv2rgb(base_hue + spread * 0.20f, 0.80f, 1.0f);
    p.hot = hsv2rgb(base_hue + spread, 0.80f, 1.0f);
    return p;
}

float probe_radius(const wlfield::FlowOperator& op) {
    float qx = 0.4f, qy = 0.05f, vx = 0.0f, vy = 0.6f, peak = 0.5f;
    for (int i = 0; i < 700; ++i) {
        float ax, ay; op.accel(qx, qy, vx, vy, ax, ay);
        vx += ax * (1.0f / 90.0f); vy += ay * (1.0f / 90.0f);
        qx += vx * (1.0f / 90.0f); qy += vy * (1.0f / 90.0f);
        peak = std::max(peak, std::sqrt(qx * qx + qy * qy));
    }
    return std::clamp(peak * 1.25f, 1.4f, 3.0f);
}

void splat(std::vector<V3>& buf, int W, int H, float x, float y, V3 c, float a) {
    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
    if (ix < 0 || ix >= W || iy < 0 || iy >= H) return;
    V3& px = buf[iy * W + ix];
    px.r += c.r * a; px.g += c.g * a; px.b += c.b * a;
}

void add_line(std::vector<V3>& buf, int W, int H, float x0, float y0, float x1, float y1, V3 c, float a) {
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = std::max(1, static_cast<int>(std::max(std::abs(dx), std::abs(dy))));
    if (steps > 60) return;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        splat(buf, W, H, x0 + dx * t, y0 + dy * t, c, a / (steps + 1) * 2.0f);
    }
}

// Render one universe into a WxH float buffer.
void render_tile(const std::string& seed, std::vector<V3>& buf, int W, int H) {
    MetaSpec meta = generate_meta_spec(seed);
    LawSpec law(meta);
    wlfield::FlowOperator op; op.configure(law);
    const Palette pal = make_palette(meta, seed);
    const float view = probe_radius(op);
    const float scale = 0.46f * std::min(W, H) / view;
    const float cx = W * 0.5f, cy = H * 0.5f;
    const float speed_norm = std::max(0.4f, op.ceiling * 0.10f);

    const int N = 9000;
    std::vector<float> qx(N), qy(N), vx(N), vy(N), age(N), life(N);
    wlfield::Rng rng(0xC0FFEEu);
    auto spawn = [&](int i) {
        // fill the whole field; a mild tangential bias lets swirl read as spirals
        const float r = view * (0.04f + 1.55f * std::sqrt(rng.unit()));
        const float a = rng.unit() * 6.2831853f;
        qx[i] = r * std::cos(a); qy[i] = r * std::sin(a);
        const float tang = 0.32f;
        vx[i] = -std::sin(a) * tang + 0.12f * rng.sym();
        vy[i] = std::cos(a) * tang + 0.12f * rng.sym();
        age[i] = 0.0f; life[i] = 2.5f + 7.0f * rng.unit();
    };
    for (int i = 0; i < N; ++i) spawn(i);

    const float h = 1.0f / 90.0f;
    const float resp2 = (view * 3.2f) * (view * 3.2f);
    const float damp = std::exp(-0.20f * h);   // gentle viscosity → keep swirl, concentrate slowly

    auto advance = [&](bool draw) {
        for (int i = 0; i < N; ++i) {
            const float px = cx + qx[i] * scale, py = cy + qy[i] * scale;
            float a1x, a1y; op.accel(qx[i], qy[i], vx[i], vy[i], a1x, a1y);
            const float hmx = qx[i] + vx[i] * (0.5f * h), hmy = qy[i] + vy[i] * (0.5f * h);
            const float hvx = vx[i] + a1x * (0.5f * h), hvy = vy[i] + a1y * (0.5f * h);
            float a2x, a2y; op.accel(hmx, hmy, hvx, hvy, a2x, a2y);
            vx[i] = (vx[i] + a2x * h) * damp;
            vy[i] = (vy[i] + a2y * h) * damp;
            qx[i] += hvx * h; qy[i] += hvy * h;
            age[i] += h;
            const float nx = cx + qx[i] * scale, ny = cy + qy[i] * scale;
            if (draw) {
                const float sp = std::sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
                const float t = sp / (sp + speed_norm);
                const V3 col = (t < 0.5f)
                    ? V3{pal.cool.r + (pal.mid.r - pal.cool.r) * (t * 2),
                         pal.cool.g + (pal.mid.g - pal.cool.g) * (t * 2),
                         pal.cool.b + (pal.mid.b - pal.cool.b) * (t * 2)}
                    : V3{pal.mid.r + (pal.hot.r - pal.mid.r) * ((t - 0.5f) * 2),
                         pal.mid.g + (pal.hot.g - pal.mid.g) * ((t - 0.5f) * 2),
                         pal.mid.b + (pal.hot.b - pal.mid.b) * ((t - 0.5f) * 2)};
                const float fade = std::min(1.0f, age[i] * 3.0f);
                // weight by speed so slow, converged particles don't blow out cores
                add_line(buf, W, H, px, py, nx, ny, col, 0.5f * fade * (0.3f + 0.7f * t));
            }
            if ((qx[i] * qx[i] + qy[i] * qy[i]) > resp2 || age[i] > life[i]
                || !std::isfinite(qx[i]) || !std::isfinite(qy[i])) spawn(i);
        }
    };

    for (int s = 0; s < 200; ++s) advance(false);            // settle onto structures
    const float decay = 0.95f;                               // trail persistence (matches live fade)
    for (int s = 0; s < 520; ++s) {
        for (auto& px : buf) { px.r *= decay; px.g *= decay; px.b *= decay; }
        advance(true);
    }

    // signature nebula backdrop (added last so it is not faded away)
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const float dx = (x - cx) / (0.5f * W), dy = (y - cy) / (0.5f * H);
            const float r = std::sqrt(dx * dx + dy * dy);
            const float n = std::max(0.0f, 0.16f - 0.16f * r);
            buf[y * W + x].r += pal.cool.r * n * 0.35f;
            buf[y * W + x].g += pal.cool.g * n * 0.35f;
            buf[y * W + x].b += pal.cool.b * n * 0.5f;
        }
    }
}

void write_bmp(const std::string& path, const std::vector<V3>& buf, int W, int H, float exposure) {
    std::vector<unsigned char> rgb(static_cast<std::size_t>(W) * H * 3);
    for (int i = 0; i < W * H; ++i) {
        auto tm = [&](float v) {
            v = 1.0f - std::exp(-v * exposure);          // Reinhard-ish
            v = std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f / 2.2f); // gamma
            return static_cast<unsigned char>(std::clamp(v * 255.0f, 0.0f, 255.0f));
        };
        rgb[i * 3 + 0] = tm(buf[i].b); // BMP is BGR
        rgb[i * 3 + 1] = tm(buf[i].g);
        rgb[i * 3 + 2] = tm(buf[i].r);
    }
    const int row = W * 3;
    const int pad = (4 - (row % 4)) % 4;
    const int img = (row + pad) * H;
    const int file = 54 + img;
    std::ofstream o(path, std::ios::binary);
    auto u16 = [&](int v) { o.put(v & 0xFF); o.put((v >> 8) & 0xFF); };
    auto u32 = [&](int v) { o.put(v & 0xFF); o.put((v >> 8) & 0xFF); o.put((v >> 16) & 0xFF); o.put((v >> 24) & 0xFF); };
    o.put('B'); o.put('M'); u32(file); u16(0); u16(0); u32(54);
    u32(40); u32(W); u32(H); u16(1); u16(24); u32(0); u32(img); u32(2835); u32(2835); u32(0); u32(0);
    const unsigned char zero[3] = {0, 0, 0};
    for (int y = H - 1; y >= 0; --y) { // bottom-up
        o.write(reinterpret_cast<const char*>(&rgb[y * row]), row);
        o.write(reinterpret_cast<const char*>(zero), pad);
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string out = "field_preview.bmp";
    int tile = 460;
    std::vector<std::string> seeds;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--size" && i + 1 < argc) tile = std::atoi(argv[++i]);
        else seeds.push_back(a);
    }
    if (seeds.empty()) seeds = {"andromeda", "void-current", "helix-nine", "seraphim-tide"};

    const int n = static_cast<int>(seeds.size());
    const int cols = (n <= 1) ? 1 : (n <= 4 ? 2 : 3);
    const int rows = (n + cols - 1) / cols;
    const int W = cols * tile, H = rows * tile;
    std::vector<V3> buf(static_cast<std::size_t>(W) * H);

    for (int i = 0; i < n; ++i) {
        const int cxp = (i % cols) * tile, cyp = (i / cols) * tile;
        std::vector<V3> sub(static_cast<std::size_t>(tile) * tile);
        std::printf("rendering '%s' ...\n", seeds[i].c_str());
        render_tile(seeds[i], sub, tile, tile);
        for (int y = 0; y < tile; ++y)
            for (int x = 0; x < tile; ++x)
                buf[(cyp + y) * W + (cxp + x)] = sub[y * tile + x];
    }

    write_bmp(out, buf, W, H, 1.05f);
    std::printf("wrote %s (%dx%d)\n", out.c_str(), W, H);
    return 0;
}
