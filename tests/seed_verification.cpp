#include "seed/CellularExpander.hpp"
#include "seed/SeedDebug.hpp"
#include "seed/SeedMachine.hpp"
#include "seed/Seeder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "seed_verification failed: " << message << '\n';
        std::exit(1);
    }
}

bool in_unit_interval(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

std::size_t count_significant_differences(const std::vector<double>& lhs,
                                          const std::vector<double>& rhs,
                                          double threshold) {
    const std::size_t count = std::min(lhs.size(), rhs.size());
    std::size_t significant = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (std::abs(lhs[i] - rhs[i]) > threshold) {
            ++significant;
        }
    }
    return significant;
}

std::size_t sum_counts(const std::array<std::uint32_t, SEED_TABLE_SIZE>& counts) {
    return std::accumulate(counts.begin(), counts.end(), std::size_t{0});
}

void test_expand_size_and_trace() {
    const CellularExpansionTrace empty = expand_with_trace("");
    require(empty.expanded.size() == SEED_EXPANDED_SIZE,
            "empty seed must expand to 512 bytes");

    require(expand("a").size() == SEED_EXPANDED_SIZE,
            "single byte seed must expand to 512 bytes");
    require(expand("hello").size() == SEED_EXPANDED_SIZE,
            "hello must expand to 512 bytes");

    std::string long_seed(2000, 'x');
    for (std::size_t i = 0; i < long_seed.size(); ++i) {
        long_seed[i] = static_cast<char>((i * 37u + 11u) & 0xFFu);
    }
    require(expand(long_seed).size() == SEED_EXPANDED_SIZE,
            "long seeds must expand to 512 bytes");
}

void test_generate_shape_and_determinism() {
    const std::string seed = "determinism-check";
    const std::vector<double> first = generate(seed);
    const std::vector<double> second = generate(seed);

    require(first.size() == SEED_OUTPUT_SIZE,
            "generate must produce 32 outputs");
    require(first == second,
            "generate must be bitwise deterministic for repeated calls");

    for (double value : first) {
        require(in_unit_interval(value),
                "all normalized outputs must stay in [0, 1]");
    }
}

void test_compress_trace_shape() {
    const std::vector<std::uint8_t> expanded = expand("trace-shape");
    const SeedMachineTrace trace = compress_with_trace(expanded);

    require(trace.output.size() == SEED_OUTPUT_SIZE,
            "trace output must expose 32 normalized values");
    require(trace.mutation_events.size() == SEED_EXPANDED_SIZE + SEED_OUTPUT_SIZE,
            "trace must record 512 byte steps plus 32 finalization passes");

    for (double value : trace.final_state.registers) {
        require(std::isfinite(value),
                "final registers must remain finite");
    }
    for (const OpEntry& entry : trace.final_state.table) {
        require(entry.reg_a < SEED_OUTPUT_SIZE,
                "reg_a must remain in range");
        require(entry.reg_b < SEED_OUTPUT_SIZE,
                "reg_b must remain in range");
        require(std::isfinite(entry.parameter),
                "parameters must remain finite");
    }
}

void test_raw_byte_handling() {
    const std::string raw_with_nul("\x00" "\xFF" "A" "\x7F", 4);
    const std::string raw_without_nul("\xFF" "A" "\x7F", 3);
    const std::vector<double> a = generate(raw_with_nul);
    const std::vector<double> b = generate(raw_with_nul);
    const std::vector<double> c = generate(raw_without_nul);

    require(a == b,
            "embedded NUL seeds must be deterministic");
    require(a != c,
            "embedded NUL bytes must materially affect the seed");
}

void test_long_seed_preservation() {
    std::string long_seed(900, 'a');
    for (std::size_t i = 0; i < long_seed.size(); ++i) {
        long_seed[i] = static_cast<char>((i * 19u + 23u) & 0xFFu);
    }
    const std::string truncated = long_seed.substr(0, SEED_EXPANDED_SIZE);

    const std::vector<double> lhs = generate(long_seed);
    const std::vector<double> rhs = generate(truncated);
    require(output_hamming_distance_16bit(lhs, rhs) > 0u,
            "bytes beyond the first 512 must still influence the output");
}

void test_empty_seed_is_valid_and_nonflat() {
    const std::vector<double> output = generate("");
    require(output.size() == SEED_OUTPUT_SIZE,
            "empty seed output must still have 32 values");
    double min_value = 1.0;
    double max_value = 0.0;
    for (double value : output) {
        require(in_unit_interval(value),
                "empty seed outputs must stay in [0, 1]");
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }
    require(max_value - min_value > 1.0e-6,
            "empty seed output must not collapse to a flat vector");
}

void test_avalanche_for_neighboring_strings() {
    const std::vector<double> a = generate("a");
    const std::vector<double> b = generate("b");
    const std::vector<double> hello = generate("hello");
    const std::vector<double> hellp = generate("hellp");

    require(output_hamming_distance_16bit(a, b) >= 80u,
            "\"a\" and \"b\" should differ substantially");
    require(count_significant_differences(a, b, 1.0e-9) >= 20u,
            "\"a\" and \"b\" should move many output lanes");

    require(output_hamming_distance_16bit(hello, hellp) >= 112u,
            "\"hello\" and \"hellp\" should avalanche");
    require(count_significant_differences(hello, hellp, 1.0e-9) >= 24u,
            "\"hello\" and \"hellp\" should move most output lanes");
}

void test_mutation_branch_is_real() {
    const SeedMachineTrace trace = compress_with_trace(expand("mutation-balance"));
    const std::size_t full_total = sum_counts(trace.full_mutation_counts);
    const std::size_t soft_total = sum_counts(trace.soft_mutation_counts);

    require(full_total > 100u,
            "full mutation branch should trigger regularly");
    require(soft_total > 100u,
            "soft mutation branch should trigger regularly");
    require(full_total + soft_total == trace.mutation_events.size(),
            "mutation counts must match the recorded event count");
}

} // namespace

int main() {
    test_expand_size_and_trace();
    test_generate_shape_and_determinism();
    test_compress_trace_shape();
    test_raw_byte_handling();
    test_long_seed_preservation();
    test_empty_seed_is_valid_and_nonflat();
    test_avalanche_for_neighboring_strings();
    test_mutation_branch_is_real();
    return 0;
}

