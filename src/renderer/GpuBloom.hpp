#pragma once
#include "raylib.h"
#include <algorithm>

// GPU-accelerated bloom/glow post-process for the universe trail.
//
// The trail accumulation texture is reduced to a half-resolution emission
// buffer, blurred with a separable Gaussian entirely on the GPU, and composited
// back additively. This replaces stacked CPU circle/line glow draws with a
// fixed set of fragment-shader passes and is the shared hook for future GPU
// effects (color grading, distortion, etc.).
//
// Requires only OpenGL 3.3 / GLSL 330, so it runs on Linux, macOS, and Windows.
// If the shaders fail to compile the pipeline reports itself unavailable and the
// caller is expected to fall back to the plain CPU trail.
class GpuBloom {
public:
    GpuBloom() = default;
    ~GpuBloom() { unload(); }

    GpuBloom(const GpuBloom&) = delete;
    GpuBloom& operator=(const GpuBloom&) = delete;

    // Compile the shader programs. Safe to call only after a GL context exists
    // (i.e. after InitWindow). Returns true if the GPU path is usable.
    bool load() {
        unload_shaders();
        prefilter_ = LoadShaderFromMemory(nullptr, kPrefilterFs);
        blur_ = LoadShaderFromMemory(nullptr, kBlurFs);

        blur_dir_loc_ = GetShaderLocation(blur_, "direction");
        prefilter_threshold_loc_ = GetShaderLocation(prefilter_, "threshold");

        // A failed compile makes raylib return the default shader, which lacks
        // our custom uniforms; checking the locations catches that reliably.
        shaders_ok_ = prefilter_.id != 0 && blur_.id != 0 &&
                      blur_dir_loc_ >= 0 && prefilter_threshold_loc_ >= 0;
        if (!shaders_ok_) {
            unload_shaders();
        }
        return shaders_ok_;
    }

    void unload() {
        unload_buffers();
        unload_shaders();
    }

    // Allocate the half-resolution ping-pong buffers for a target of (w, h).
    void resize(int w, int h) {
        const int next_w = std::max(1, w / 2);
        const int next_h = std::max(1, h / 2);
        if (has_buffers_ && next_w == buf_w_ && next_h == buf_h_) {
            return;
        }
        unload_buffers();
        buf_w_ = next_w;
        buf_h_ = next_h;
        buf_a_ = LoadRenderTexture(buf_w_, buf_h_);
        buf_b_ = LoadRenderTexture(buf_w_, buf_h_);
        SetTextureFilter(buf_a_.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(buf_b_.texture, TEXTURE_FILTER_BILINEAR);
        has_buffers_ = true;
    }

    bool ready() const { return shaders_ok_ && has_buffers_; }

    // Build the blurred glow from `source` into the internal result buffer.
    // Must be called outside of any active scissor/texture mode.
    void generate(const Texture2D& source) const {
        if (!ready()) {
            return;
        }

        SetShaderValue(prefilter_, prefilter_threshold_loc_, &threshold_, SHADER_UNIFORM_FLOAT);
        BeginTextureMode(buf_a_);
        ClearBackground(BLANK);
        BeginShaderMode(prefilter_);
        blit(source, buf_w_, buf_h_);
        EndShaderMode();
        EndTextureMode();

        for (int pass = 0; pass < kIterations; ++pass) {
            blur_pass(buf_a_, buf_b_, {spread_ / static_cast<float>(buf_w_), 0.0f});
            blur_pass(buf_b_, buf_a_, {0.0f, spread_ / static_cast<float>(buf_h_)});
        }
    }

    // Composite the generated glow additively onto the current render target,
    // scaled to cover `target` (the full canvas/window rectangle).
    void composite(Rectangle target) const {
        if (!ready()) {
            return;
        }
        const Rectangle src = {0.0f, 0.0f,
                               static_cast<float>(buf_w_),
                               -static_cast<float>(buf_h_)};
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(buf_a_.texture, src, target, {0.0f, 0.0f}, 0.0f,
                       ColorAlpha(WHITE, intensity_));
        EndBlendMode();
    }

    void set_intensity(float value) { intensity_ = std::clamp(value, 0.0f, 4.0f); }
    void set_threshold(float value) { threshold_ = std::clamp(value, 0.0f, 1.0f); }
    void set_spread(float value) { spread_ = std::max(0.0f, value); }

private:
    static constexpr int kIterations = 2;

    void blur_pass(const RenderTexture2D& src,
                   const RenderTexture2D& dst,
                   Vector2 direction) const {
        SetShaderValue(blur_, blur_dir_loc_, &direction, SHADER_UNIFORM_VEC2);
        BeginTextureMode(dst);
        ClearBackground(BLANK);
        BeginShaderMode(blur_);
        blit(src.texture, buf_w_, buf_h_);
        EndShaderMode();
        EndTextureMode();
    }

    // Draw a texture stretched to (w, h), flipping vertically because raylib
    // render-texture targets are stored upside down.
    static void blit(const Texture2D& tex, int w, int h) {
        const Rectangle src = {0.0f, 0.0f,
                               static_cast<float>(tex.width),
                               -static_cast<float>(tex.height)};
        const Rectangle dst = {0.0f, 0.0f,
                               static_cast<float>(w),
                               static_cast<float>(h)};
        DrawTexturePro(tex, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
    }

    void unload_shaders() {
        if (prefilter_.id != 0) {
            UnloadShader(prefilter_);
            prefilter_ = Shader{};
        }
        if (blur_.id != 0) {
            UnloadShader(blur_);
            blur_ = Shader{};
        }
        shaders_ok_ = false;
        blur_dir_loc_ = -1;
        prefilter_threshold_loc_ = -1;
    }

    void unload_buffers() {
        if (has_buffers_) {
            UnloadRenderTexture(buf_a_);
            UnloadRenderTexture(buf_b_);
            has_buffers_ = false;
        }
    }

    Shader prefilter_{};
    Shader blur_{};
    int blur_dir_loc_ = -1;
    int prefilter_threshold_loc_ = -1;
    bool shaders_ok_ = false;

    RenderTexture2D buf_a_{};
    RenderTexture2D buf_b_{};
    int buf_w_ = 0;
    int buf_h_ = 0;
    bool has_buffers_ = false;

    float intensity_ = 1.15f;
    float threshold_ = 0.18f;
    float spread_ = 2.2f;

    // Keeps only the brightest part of the trail so the dark background does not
    // wash out when blurred.
    static constexpr const char* kPrefilterFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float threshold;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float br = max(c.r, max(c.g, c.b));
    float k = max(br - threshold, 0.0);
    float scale = (br > 1e-5) ? (k / br) : 0.0;
    finalColor = vec4(c * scale, 1.0);
}
)";

    // One axis of a separable Gaussian; run once per axis per iteration.
    static constexpr const char* kBlurFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 direction;
void main() {
    float w0 = 0.227027;
    float w1 = 0.1945946;
    float w2 = 0.1216216;
    float w3 = 0.054054;
    float w4 = 0.016216;
    vec3 sum = texture(texture0, fragTexCoord).rgb * w0;
    sum += texture(texture0, fragTexCoord + direction * 1.0).rgb * w1;
    sum += texture(texture0, fragTexCoord - direction * 1.0).rgb * w1;
    sum += texture(texture0, fragTexCoord + direction * 2.0).rgb * w2;
    sum += texture(texture0, fragTexCoord - direction * 2.0).rgb * w2;
    sum += texture(texture0, fragTexCoord + direction * 3.0).rgb * w3;
    sum += texture(texture0, fragTexCoord - direction * 3.0).rgb * w3;
    sum += texture(texture0, fragTexCoord + direction * 4.0).rgb * w4;
    sum += texture(texture0, fragTexCoord - direction * 4.0).rgb * w4;
    finalColor = vec4(sum, 1.0);
}
)";
};
