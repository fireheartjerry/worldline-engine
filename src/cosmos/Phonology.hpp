#pragma once
// Continuous generative phonology — procedural names synthesized phoneme by
// phoneme, NOT assembled from a word/syllable bank. A "language" is a point in a
// continuous ~15-float style space; every phoneme at every slot is drawn from a
// distribution whose parameters are smooth functions of that point plus the
// slot's sonority target. Nudging a style float continuously morphs the naming
// aesthetic; a child language is a small continuous drift of its parent, so a
// lineage (galaxy -> system -> planet -> species) shares a phonetic family while
// every name is unique. Deterministic: name = pure function of a 64-bit seed.
//
// Grounded in the Sonority Sequencing Principle (sonority rises to the vocalic
// nucleus, falls after) and a Minimum-Sonority-Distance cluster gate, both turned
// into continuous knobs.

#include "cosmos/Spectrum.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

namespace cosmos {
namespace phon {

// Sonority classes, low -> high sonority (index == sonority rank).
enum Cls { VSTOP, DSTOP, VFRIC, DFRIC, NASAL, LIQUID, GLIDE, VOWEL, CLS_N };

struct Phon {
    const char* g;     // grapheme
    int   cls;         // class (== sonority)
    float hardness;    // 1 hard (stops) .. 0 soft (glides/liquids)
    float sibilance;   // s/sh/z high
    float nasality;    // m/n high
    float openness;    // vowels: a open(1) .. i/u close(0)
    float frontness;   // vowels: i/e front(1) .. o/u back(0)
};

// The fixed alphabet (the ONLY fixed thing). Never indexed as a bank; only ever
// reshaped by continuous weights and sampled.
inline const std::array<Phon, 25>& inventory() {
    static const std::array<Phon, 25> inv = {{
        {"p", VSTOP, 1.0f, 0, 0, 0, 0}, {"t", VSTOP, 1.0f, 0, 0, 0, 0}, {"k", VSTOP, 1.0f, 0, 0, 0, 0},
        {"b", DSTOP, 0.8f, 0, 0, 0, 0}, {"d", DSTOP, 0.8f, 0, 0, 0, 0}, {"g", DSTOP, 0.8f, 0, 0, 0, 0},
        {"f", VFRIC, 0.7f, 0.2f, 0, 0, 0}, {"s", VFRIC, 0.7f, 1, 0, 0, 0}, {"sh", VFRIC, 0.6f, 1, 0, 0, 0},
        {"th", VFRIC, 0.6f, 0.3f, 0, 0, 0}, {"h", VFRIC, 0.4f, 0, 0, 0, 0},
        {"v", DFRIC, 0.5f, 0.2f, 0, 0, 0}, {"z", DFRIC, 0.5f, 1, 0, 0, 0},
        {"m", NASAL, 0.3f, 0, 1, 0, 0}, {"n", NASAL, 0.3f, 0, 1, 0, 0},
        {"l", LIQUID, 0.1f, 0, 0, 0, 0}, {"r", LIQUID, 0.1f, 0, 0, 0, 0},
        {"w", GLIDE, 0.05f, 0, 0, 0, 0}, {"y", GLIDE, 0.05f, 0, 0, 0, 0},
        {"a", VOWEL, 0, 0, 0, 1.0f, 0.5f}, {"e", VOWEL, 0, 0, 0, 0.5f, 0.9f}, {"i", VOWEL, 0, 0, 0, 0.0f, 1.0f},
        {"o", VOWEL, 0, 0, 0, 0.5f, 0.1f}, {"u", VOWEL, 0, 0, 0, 0.0f, 0.0f}, {"ae", VOWEL, 0, 0, 0, 0.85f, 0.8f},
    }};
    return inv;
}
// [begin,end) index ranges per class (prefix sums of the counts above).
inline const std::array<int, CLS_N + 1>& class_span() {
    static const std::array<int, CLS_N + 1> s = {{0, 3, 6, 11, 13, 15, 17, 19, 25}};
    return s;
}

// ── Style vector (the continuous "DNA" of a language) ────────────────────────
enum SP {
    S_VOPEN, S_VFRONT, S_VFOCUS, S_VRICH, S_HARD, S_SIB, S_NAS, S_ONSET,
    S_CODA, S_CODASOFT, S_CLUST, S_SLOPE, S_STRICT, S_LEN, S_RHYTHM, S_N
};

struct Language {
    std::array<float, S_N> v{};
    float operator[](int i) const { return v[static_cast<std::size_t>(i)]; }
    float& operator[](int i) { return v[static_cast<std::size_t>(i)]; }
};

inline Language make_language(std::uint64_t seed) {
    Stream s(seed, 0xA17C5Eull);
    Language L;
    for (int i = 0; i < S_N; ++i) L[i] = static_cast<float>(s.unit());
    // Gentle priors so an "average" language is pleasant + pronounceable.
    L[S_STRICT] = 0.4f + 0.6f * L[S_STRICT];
    L[S_CLUST]  = L[S_CLUST] * L[S_CLUST]; // clusters rarer unless committed
    return L;
}

inline float reflect01(float x) { return x < 0.0f ? -x : (x > 1.0f ? 2.0f - x : x); }

// Child language = small continuous drift of the parent (lineage coherence).
inline Language derive_language(const Language& parent, std::uint64_t child_seed, float drift) {
    Stream s(child_seed, 0xD81F7ull);
    Language c = parent;
    for (int i = 0; i < S_N; ++i) {
        c[i] = reflect01(c[i] + static_cast<float>(s.sym()) * drift);
    }
    return c;
}

namespace detail {

inline float sq(float x) { return x * x; }

// Softmax-sample one phoneme from index range [b,e) under a log-weight functor.
template <typename W>
inline const Phon& sample(Stream& s, int b, int e, W logw) {
    const auto& inv = inventory();
    float w[12];
    int idx[12];
    int n = 0;
    float wmax = -1e30f;
    for (int k = b; k < e && n < 12; ++k) {
        const float lw = logw(inv[static_cast<std::size_t>(k)]);
        if (lw <= -1e29f) continue; // ineligible
        idx[n] = k; w[n] = lw; if (lw > wmax) wmax = lw; ++n;
    }
    if (n == 0) return inv[static_cast<std::size_t>(b)]; // ramp guarantees this is rare
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) { w[i] = std::exp(w[i] - wmax); sum += w[i]; }
    float r = static_cast<float>(s.unit()) * sum, acc = 0.0f;
    for (int i = 0; i < n; ++i) { acc += w[i]; if (r <= acc) return inv[static_cast<std::size_t>(idx[i])]; }
    return inv[static_cast<std::size_t>(idx[n - 1])];
}

inline const Phon& sample_vowel(const Language& L, Stream& s) {
    const auto sp = class_span();
    return sample(s, sp[VOWEL], sp[VOWEL + 1], [&](const Phon& p) {
        const float focus = 1.1f + 1.8f * L[S_VFOCUS];
        return -sq(p.openness - L[S_VOPEN]) * focus - sq(p.frontness - L[S_VFRONT]) * focus;
    });
}

// A consonant near sonority `target`, on the correct side of the ramp vs. prevSon
// (dir +1 ascending onset, -1 descending coda), satisfying the MSD gate.
inline const Phon& sample_consonant(const Language& L, Stream& s, bool coda,
                                    float target, float prev_son, int dir, int msd) {
    const auto sp = class_span();
    return sample(s, sp[VSTOP], sp[GLIDE + 1], [&](const Phon& p) -> float {
        if (prev_son >= 0.0f) {
            const float delta = (static_cast<float>(p.cls) - prev_son) * static_cast<float>(dir);
            if (delta < 0.0f || std::abs(static_cast<float>(p.cls) - prev_son) < static_cast<float>(msd))
                return -1e30f; // ineligible (wrong ramp direction or too-close sonority)
        }
        float w = -sq(static_cast<float>(p.cls) - target) * (0.30f + 1.6f * L[S_STRICT]);
        w += L[S_HARD] * (p.hardness - 0.5f) * 1.6f;
        w += L[S_SIB] * p.sibilance * 2.2f;
        w += L[S_NAS] * p.nasality * 2.2f;
        if (coda) w += (1.0f - p.hardness) * L[S_CODASOFT] * 1.6f;
        return w;
    });
}

} // namespace detail

// Synthesize a name. `klen` (>0) biases syllable count, `kreg` in [0,1] the
// "register" (grand galaxies vs casual species) — both continuous, not switches.
inline std::string generate_name(const Language& L, std::uint64_t seed,
                                 float klen = 1.0f, float kreg = 0.5f) {
    Stream s(seed, 0x4E3A11ull);
    std::string out;
    out.reserve(24);

    int nsyl = 2; // names read better with at least two syllables
    while (nsyl < 3 && s.unit() < clampd(0.22 + 0.40 * L[S_LEN] * klen, 0.0, 0.70)) ++nsyl;

    bool prev_coda = true; // word start: no preceding vowel to collide with
    for (int i = 0; i < nsyl; ++i) {
        if (out.size() > 15) break; // hard length guard
        const float phase = (i & 1) ? 1.0f : -1.0f;
        const float rhythm = 1.0f + (L[S_RHYTHM] - 0.5f) * phase;

        // Onset (0..2 consonants ascending toward the vowel). Non-initial onsets
        // are common, and FORCED when the previous syllable ended in a vowel, so
        // names stay CV.CV and two vowels never collide.
        const bool force_onset = (i > 0 && !prev_coda);
        const bool onset = force_onset ||
                           s.unit() < (i == 0 ? 0.88 + 0.11 * L[S_ONSET] : 0.74 + 0.22 * L[S_ONSET]);
        int onset_k = 0;
        if (onset) { onset_k = 1; if (s.unit() < L[S_CLUST] * L[S_CLUST] * rhythm) onset_k = 2; }
        const int msd = static_cast<int>(std::lround(1.0 + 2.0 * L[S_SLOPE]));
        float prev = -1.0f;
        for (int j = 0; j < onset_k; ++j) {
            // A single onset spans the whole sonority range by hardness (hard
            // language -> stops, soft -> liquids/glides). A cluster ascends from a
            // low outer consonant to a sonorous inner one (stop+liquid: "pl","tr").
            float target;
            if (onset_k == 1) target = 1.0f + 5.0f * (1.0f - L[S_HARD]);
            else target = lerp(1.0f + 1.5f * (1.0f - L[S_HARD]), 5.0f,
                               static_cast<double>(j) / (onset_k - 1));
            const Phon& p = detail::sample_consonant(L, s, false, target, prev, +1, msd);
            out += p.g; prev = static_cast<float>(p.cls);
        }

        // Nucleus (1 vowel, optional diphthong glide).
        const Phon& vow = detail::sample_vowel(L, s);
        out += vow.g;
        if (s.unit() < L[S_VRICH] * 0.14) { out += detail::sample_vowel(L, s).g; }

        // Coda (0..2 consonants descending from the vowel).
        const float decay = (i == nsyl - 1) ? 0.8f : 0.12f; // codas almost only word-final
        const bool coda = s.unit() < L[S_CODA] * decay * rhythm;
        int coda_k = 0;
        if (coda) { coda_k = 1; if (s.unit() < L[S_CLUST] * L[S_CODA] * 0.22) coda_k = 2; }
        prev = 7.0f;
        for (int j = 0; j < coda_k; ++j) {
            const float target = lerp(5.0f, 1.0f + 4.0f * L[S_SLOPE], static_cast<double>(j) / std::max(1, coda_k));
            const Phon& p = detail::sample_consonant(L, s, true, target, prev, -1, msd);
            out += p.g; prev = static_cast<float>(p.cls);
        }

        // Continuous syllable joint: low-register names occasionally take an
        // apostrophe at a real boundary (never mid-cluster).
        if (i + 1 < nsyl && coda_k > 0 && s.unit() < (1.0f - kreg) * 0.16f) out += '\'';
        prev_coda = (coda_k > 0);
    }

    if (out.empty()) out = "a";
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    // Grand register: an occasional internal capital after an apostrophe.
    for (std::size_t k = 1; k + 1 < out.size(); ++k) {
        if (out[k] == '\'' && kreg > 0.7f && s.unit() < (kreg - 0.7f)) {
            out[k + 1] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[k + 1])));
        }
    }
    return out;
}

} // namespace phon
} // namespace cosmos
