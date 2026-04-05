#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

static constexpr std::size_t SEED_OUTPUT_SIZE = 32;
static constexpr std::size_t SEED_TABLE_SIZE = 89;

enum class PrimitiveType : std::uint8_t {
    ROTATE_PAIR,
    FIBONACCI_Q,
    BINOMIAL_CASCADE,
    SIERPINSKI_COUPLING,
    FOLD,
    REFLECT,
    ACCUMULATOR_INJECT,
    SCALE
};

struct OpEntry {
    PrimitiveType primitive = PrimitiveType::ROTATE_PAIR;
    std::uint8_t reg_a = 0;
    std::uint8_t reg_b = 1;
    double parameter = 0.0;
};

struct MachineState {
    std::array<double, SEED_OUTPUT_SIZE> registers{};
    std::array<OpEntry, SEED_TABLE_SIZE> table{};
    std::uint64_t accumulator = 0;
};

enum class MutationMode : std::uint8_t {
    FULL,
    SOFT
};

struct MutationEvent {
    std::size_t step = 0;
    bool from_finalization = false;
    MutationMode mode = MutationMode::SOFT;
    std::size_t source_index = 0;
    std::size_t target_index = 0;
    OpEntry before{};
    OpEntry after{};
    std::uint64_t accumulator = 0;
};

struct SeedMachineTrace {
    MachineState final_state{};
    std::array<double, SEED_OUTPUT_SIZE> output{};
    std::vector<MutationEvent> mutation_events;
    std::array<std::uint32_t, SEED_TABLE_SIZE> full_mutation_counts{};
    std::array<std::uint32_t, SEED_TABLE_SIZE> soft_mutation_counts{};
};

std::vector<double> compress(const std::vector<std::uint8_t>& expanded);
SeedMachineTrace compress_with_trace(const std::vector<std::uint8_t>& expanded);
