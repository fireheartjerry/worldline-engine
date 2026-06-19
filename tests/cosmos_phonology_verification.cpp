// Verifies the continuous name synthesizer: deterministic, high-variety (not a
// word bank), pronounceable, and lineage-coherent (children drift continuously).

#include "cosmos/Phonology.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace cosmos::phon;

namespace {

int g_failures = 0;
void check(bool cond, const std::string& what) {
    if (!cond) { std::cerr << "cosmos_phonology_verification FAILED: " << what << "\n"; ++g_failures; }
}

bool has_vowel(const std::string& s) {
    for (char c : s) {
        const char l = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (l == 'a' || l == 'e' || l == 'i' || l == 'o' || l == 'u' || l == 'y') return true;
    }
    return false;
}

} // namespace

int main() {
    // --- Determinism ----------------------------------------------------------
    {
        const Language L = make_language(0xABCDEFull);
        for (std::uint64_t s = 1; s <= 50; ++s) {
            check(generate_name(L, s) == generate_name(L, s), "same (lang,seed) -> same name");
        }
        check(make_language(7ull).v == make_language(7ull).v, "language is deterministic");
    }

    // --- Well-formed: non-empty, capitalized, has a vowel, bounded length -----
    {
        for (std::uint64_t s = 1; s <= 4000; ++s) {
            const Language L = make_language(s * 2654435761ull);
            const std::string n = generate_name(L, s);
            check(!n.empty(), "name non-empty");
            check(std::isupper(static_cast<unsigned char>(n[0])) != 0, "name capitalized");
            check(has_vowel(n), "name has a vowel (pronounceable)");
            check(n.size() <= 22, "name bounded in length");
        }
    }

    // --- Variety: NOT a word bank. Many seeds -> mostly distinct names. --------
    {
        std::set<std::string> names;
        const int N = 3000;
        for (int i = 0; i < N; ++i) {
            const std::uint64_t s = static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ull + 1;
            names.insert(generate_name(make_language(s), s));
        }
        // A finite syllable bank would collide heavily; continuous synthesis keeps
        // the vast majority unique (the few collisions are very short names).
        check(static_cast<int>(names.size()) > static_cast<int>(0.80 * N),
              "names are highly varied (not drawn from a small bank)");
    }

    // --- A single language still yields varied names across seeds --------------
    {
        const Language L = make_language(0x1234ull);
        std::set<std::string> names;
        for (std::uint64_t s = 1; s <= 500; ++s) names.insert(generate_name(L, s));
        check(names.size() > 400, "one language produces many distinct names");
    }

    // --- Different languages sound different -----------------------------------
    {
        const Language a = make_language(11ull);
        const Language b = make_language(999983ull);
        int diff = 0;
        for (std::uint64_t s = 1; s <= 100; ++s) {
            if (generate_name(a, s) != generate_name(b, s)) ++diff;
        }
        check(diff > 80, "different languages give mostly different names");
    }

    // --- Lineage coherence: a child language is a small continuous drift -------
    {
        const Language parent = make_language(42ull);
        const Language child = derive_language(parent, 1001ull, 0.08f);
        float max_delta = 0.0f;
        for (int i = 0; i < S_N; ++i) max_delta = std::max(max_delta, std::abs(child[i] - parent[i]));
        check(max_delta <= 0.08f + 1.0e-5f, "child style stays within the drift radius");
        check(!(child.v == parent.v), "child differs from parent");
        // Determinism of derivation.
        check(derive_language(parent, 1001ull, 0.08f).v == child.v, "child derivation deterministic");
    }

    if (g_failures == 0) {
        std::cout << "cosmos_phonology_verification: all checks passed\n";
        return 0;
    }
    std::cerr << "cosmos_phonology_verification: " << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
}
