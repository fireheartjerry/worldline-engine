#include "seed/SeedMachine.hpp"
#include "seed/CellularExpander.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <vector>

namespace {

constexpr double kPi = 3.1415926535897932385;
constexpr double kTwoPi = 6.2831853071795864769;
constexpr double kPhi = 1.6180339887498948482;
constexpr double kPhiInv = 0.6180339887498948482;
constexpr double kSqrt2 = 1.4142135623730950488;
constexpr double kSqrt3 = 1.7320508075688772935;
constexpr double kSqrt5 = 2.2360679774997896964;
constexpr double kSqrt7 = 2.6457513105737891893;
constexpr double kSqrt11 = 3.3166247903553998491;
constexpr double kEulerE = 2.7182818284590452354;

constexpr std::uint64_t kAccumulatorSeedSqrt2 = 0x6A09E667F3BCC908ULL;
constexpr std::uint64_t kGoldenRatioU64 = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t kMurmurMix1 = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t kInvTauU64 = 0x28BE60DB9391054AULL;
constexpr std::uint64_t kPhiInvOverTauU64 = 0x192E540DD5DE49ADULL;
constexpr std::size_t kFinalizationPasses = SEED_OUTPUT_SIZE;
constexpr std::size_t kPascalRowCount = 32;

static_assert(sizeof(double) == 8, "Seed machine requires 64-bit doubles");
static_assert(std::numeric_limits<double>::is_iec559,
              "Seed machine requires IEEE-754 doubles");

using RegisterArray = std::array<double, SEED_OUTPUT_SIZE>;
using TableArray = std::array<OpEntry, SEED_TABLE_SIZE>;
using ByteTape = std::array<std::uint8_t, SEED_EXPANDED_SIZE>;

double safe_tanh(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    const double bounded = std::max(-64.0, std::min(64.0, value));
    const double result = std::tanh(bounded);
    return result == 0.0 ? 0.0 : result;
}

double sanitize_value(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return value == 0.0 ? 0.0 : value;
}

void sanitize_registers(RegisterArray& registers) {
    for (double& value : registers) {
        value = sanitize_value(value);
    }
}

bool is_little_endian() {
    const std::uint16_t probe = 0x0102u;
    return *reinterpret_cast<const std::uint8_t*>(&probe) == 0x02u;
}

std::uint64_t byteswap64(std::uint64_t value) {
    return ((value & 0x00000000000000FFULL) << 56u)
        | ((value & 0x000000000000FF00ULL) << 40u)
        | ((value & 0x0000000000FF0000ULL) << 24u)
        | ((value & 0x00000000FF000000ULL) << 8u)
        | ((value & 0x000000FF00000000ULL) >> 8u)
        | ((value & 0x0000FF0000000000ULL) >> 24u)
        | ((value & 0x00FF000000000000ULL) >> 40u)
        | ((value & 0xFF00000000000000ULL) >> 56u);
}

std::uint64_t canonical_double_bits(double value) {
    if (value == 0.0) {
        return 0u;
    }

    std::uint64_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    return is_little_endian() ? bits : byteswap64(bits);
}

std::uint32_t popcount64(std::uint64_t value) {
    std::uint32_t count = 0u;
    while (value != 0u) {
        count += static_cast<std::uint32_t>(value & 1u);
        value >>= 1u;
    }
    return count;
}

double u64_to_unit(std::uint64_t value) {
    return std::ldexp(static_cast<double>(value >> 11u), -53);
}

double reduced_phase(std::uint64_t value,
                     std::uint64_t multiplier_u64) {
    return kTwoPi * u64_to_unit(value * multiplier_u64);
}

RegisterArray make_initial_registers() {
    RegisterArray registers{};

    // TODO(Jerry): replace this provisional generated register set with 32
    // hand-curated irrational values in [-1, 1]. The final set should be
    // nonzero, intentionally distributed across positive and negative values,
    // and not follow an obvious sequence. Suitable sources include fractional
    // parts and linear combinations of sqrt(2), sqrt(3), sqrt(5), sqrt(7),
    // sqrt(11), phi, e, pi, Catalan's constant, and Apery's constant.
    constexpr std::array<double, 8> bases{{
        kSqrt2 - 1.0,
        kSqrt3 - 1.0,
        kSqrt5 - 2.0,
        kSqrt7 - 2.0,
        kSqrt11 - 3.0,
        kPhi - 1.0,
        kEulerE - 2.0,
        kPi - 3.0
    }};

    for (std::size_t i = 0; i < registers.size(); ++i) {
        const double phase = static_cast<double>(i + 1u);
        const double base = bases[i % bases.size()];
        double value =
            base * (1.35 + 0.09 * phase)
            + std::sin(phase * kPhi)
            + 0.5 * std::cos(phase * kSqrt2);
        value = safe_tanh(value);
        if (i & 1u) {
            value = -value;
        }
        if (std::abs(value) < 0.05) {
            value += (i & 1u) ? -0.17 : 0.17;
            value = safe_tanh(value);
        }
        registers[i] = value;
    }

    return registers;
}

TableArray make_initial_operation_table() {
    TableArray table{};

    // TODO(Jerry): replace this provisional procedural table with a fully
    // hand-designed 89-entry layout. The intended design space is:
    // - choose primitive frequencies deliberately
    // - keep broad coupling primitives (binomial/Sierpinski) sparse enough
    //   that they do not drown out the local pairwise and unary primitives
    // - align reg_a/reg_b choices with the character you want short seeds to
    //   have when they first hit the machine
    // - choose initial parameters so the machine starts lively but finite
    constexpr std::array<PrimitiveType, 13> motif{{
        PrimitiveType::ROTATE_PAIR,
        PrimitiveType::FIBONACCI_Q,
        PrimitiveType::FOLD,
        PrimitiveType::ACCUMULATOR_INJECT,
        PrimitiveType::SCALE,
        PrimitiveType::REFLECT,
        PrimitiveType::ROTATE_PAIR,
        PrimitiveType::FIBONACCI_Q,
        PrimitiveType::FOLD,
        PrimitiveType::SIERPINSKI_COUPLING,
        PrimitiveType::ACCUMULATOR_INJECT,
        PrimitiveType::SCALE,
        PrimitiveType::BINOMIAL_CASCADE
    }};

    for (std::size_t slot = 0; slot < table.size(); ++slot) {
        OpEntry entry;
        entry.primitive = motif[(slot * 5u + 3u) % motif.size()];
        entry.reg_a = static_cast<std::uint8_t>((slot * 11u + 7u) % SEED_OUTPUT_SIZE);
        entry.reg_b = static_cast<std::uint8_t>((slot * 17u + 3u) % SEED_OUTPUT_SIZE);
        if (entry.reg_a == entry.reg_b) {
            entry.reg_b = static_cast<std::uint8_t>((entry.reg_b + 1u) % SEED_OUTPUT_SIZE);
        }

        double parameter =
            0.55 * std::sin((static_cast<double>(slot) + 1.0) * kPhi)
            + 0.35 * std::cos((static_cast<double>(slot) + 1.0) * kSqrt3);
        if (entry.primitive == PrimitiveType::BINOMIAL_CASCADE
            || entry.primitive == PrimitiveType::SIERPINSKI_COUPLING) {
            parameter *= 0.45;
        }
        entry.parameter = safe_tanh(parameter);
        table[slot] = entry;
    }

    return table;
}

void canonicalize_entry(OpEntry& entry) {
    entry.reg_a = static_cast<std::uint8_t>(entry.reg_a % SEED_OUTPUT_SIZE);
    entry.reg_b = static_cast<std::uint8_t>(entry.reg_b % SEED_OUTPUT_SIZE);
    if ((entry.primitive == PrimitiveType::ROTATE_PAIR
         || entry.primitive == PrimitiveType::FIBONACCI_Q)
        && entry.reg_a == entry.reg_b) {
        entry.reg_b = static_cast<std::uint8_t>((entry.reg_a + 1u) % SEED_OUTPUT_SIZE);
    }
    if (!std::isfinite(entry.parameter)) {
        entry.parameter = 0.0;
    }
}

MachineState make_initial_machine_state() {
    MachineState state;
    state.registers = make_initial_registers();
    state.table = make_initial_operation_table();
    state.accumulator = kAccumulatorSeedSqrt2;
    return state;
}

std::uint64_t register_digest(const RegisterArray& registers) {
    std::uint64_t digest = 0u;
    for (double value : registers) {
        digest ^= canonical_double_bits(value);
    }
    return digest;
}

std::uint64_t mix_accumulator(std::uint64_t accumulator,
                              std::uint8_t byte,
                              std::uint64_t reg_digest) {
    accumulator ^= (static_cast<std::uint64_t>(byte) << 56u) | reg_digest;
    accumulator *= kGoldenRatioU64;
    accumulator ^= accumulator >> 30u;
    accumulator *= kMurmurMix1;
    accumulator ^= accumulator >> 27u;
    return accumulator;
}

const std::array<std::array<double, SEED_OUTPUT_SIZE>, kPascalRowCount>& pascal_weights() {
    static const std::array<std::array<double, SEED_OUTPUT_SIZE>, kPascalRowCount> weights = [] {
        std::array<std::array<double, SEED_OUTPUT_SIZE>, kPascalRowCount> result{};
        for (std::size_t row = 0; row < kPascalRowCount; ++row) {
            std::array<std::uint64_t, kPascalRowCount> coefficients{};
            coefficients[0] = 1u;
            for (std::size_t k = 1; k <= row; ++k) {
                coefficients[k] = coefficients[k - 1u] * (row - k + 1u) / k;
            }
            const double scale = 1.0 / static_cast<double>(1ULL << row);
            for (std::size_t k = 0; k < SEED_OUTPUT_SIZE; ++k) {
                result[row][k] =
                    static_cast<double>(coefficients[k % (row + 1u)]) * scale;
            }
        }
        return result;
    }();
    return weights;
}

bool sierpinski_active(std::size_t lhs,
                       std::size_t rhs) {
    if (lhs == rhs) {
        return false;
    }
    const std::size_t hi = std::max(lhs, rhs);
    const std::size_t lo = std::min(lhs, rhs);
    return (lo & ~hi) == 0u;
}

void apply_rotate_pair(const OpEntry& op,
                       RegisterArray& registers,
                       std::uint64_t accumulator) {
    const double angle = op.parameter * kPi * u64_to_unit(accumulator);
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const double a = registers[op.reg_a];
    const double b = registers[op.reg_b];
    registers[op.reg_a] = sanitize_value(a * c - b * s);
    registers[op.reg_b] = sanitize_value(a * s + b * c);
}

void apply_fibonacci_q(const OpEntry& op,
                       RegisterArray& registers) {
    const double a = registers[op.reg_a];
    const double b = registers[op.reg_b];
    registers[op.reg_a] = safe_tanh(op.parameter * (a + b) * 2.5);
    registers[op.reg_b] = safe_tanh(op.parameter * a * 2.5);
}

void apply_binomial_cascade(const OpEntry& op,
                            RegisterArray& registers,
                            std::uint64_t accumulator) {
    const std::size_t row = static_cast<std::size_t>(accumulator % kPascalRowCount);
    const auto& weights = pascal_weights()[row];
    RegisterArray next{};
    for (std::size_t i = 0; i < registers.size(); ++i) {
        double filtered = 0.0;
        for (std::size_t k = 0; k < registers.size(); ++k) {
            filtered += weights[k] * registers[(i + k) % registers.size()];
        }
        next[i] = safe_tanh(registers[i] + op.parameter * filtered);
    }
    registers = next;
}

void apply_sierpinski_coupling(const OpEntry& op,
                               RegisterArray& registers) {
    RegisterArray next = registers;
    for (std::size_t i = 0; i < registers.size(); ++i) {
        double shear_sum = 0.0;
        std::size_t count = 0u;
        for (std::size_t j = 0; j < registers.size(); ++j) {
            if (!sierpinski_active(i, j)) {
                continue;
            }
            shear_sum += registers[j] - registers[i];
            ++count;
        }
        const double mean_shear = (count == 0u) ? 0.0 : (shear_sum / static_cast<double>(count));
        next[i] = safe_tanh(registers[i] + op.parameter * mean_shear);
    }
    registers = next;
}

void apply_fold(const OpEntry& op,
                RegisterArray& registers) {
    registers[op.reg_a] = sanitize_value(std::sin(registers[op.reg_a] * kPi * op.parameter));
}

void apply_reflect(const OpEntry& op,
                   RegisterArray& registers) {
    const double source = registers[op.reg_a];
    const std::size_t left = (op.reg_a + SEED_OUTPUT_SIZE - 1u) % SEED_OUTPUT_SIZE;
    const std::size_t right = (op.reg_a + 1u) % SEED_OUTPUT_SIZE;
    registers[op.reg_a] = sanitize_value(-source);
    registers[left] = safe_tanh(registers[left] + op.parameter * source * 0.5);
    registers[right] = safe_tanh(registers[right] + op.parameter * source * 0.5);
}

void apply_accumulator_inject(const OpEntry& op,
                              RegisterArray& registers,
                              std::uint64_t accumulator) {
    const double phase = reduced_phase(accumulator, kPhiInvOverTauU64);
    const double perturbation = op.parameter * std::sin(phase) * 3.0;
    registers[op.reg_a] = safe_tanh(registers[op.reg_a] + perturbation);
}

void apply_scale(const OpEntry& op,
                 RegisterArray& registers,
                 std::uint64_t accumulator) {
    const double scale = op.parameter * (1.0 + 0.8 * u64_to_unit(accumulator * kGoldenRatioU64));
    registers[op.reg_a] = safe_tanh(registers[op.reg_a] * scale);
}

void apply_primitive(const OpEntry& op,
                     RegisterArray& registers,
                     std::uint64_t accumulator) {
    switch (op.primitive) {
    case PrimitiveType::ROTATE_PAIR:
        apply_rotate_pair(op, registers, accumulator);
        break;
    case PrimitiveType::FIBONACCI_Q:
        apply_fibonacci_q(op, registers);
        break;
    case PrimitiveType::BINOMIAL_CASCADE:
        apply_binomial_cascade(op, registers, accumulator);
        break;
    case PrimitiveType::SIERPINSKI_COUPLING:
        apply_sierpinski_coupling(op, registers);
        break;
    case PrimitiveType::FOLD:
        apply_fold(op, registers);
        break;
    case PrimitiveType::REFLECT:
        apply_reflect(op, registers);
        break;
    case PrimitiveType::ACCUMULATOR_INJECT:
        apply_accumulator_inject(op, registers, accumulator);
        break;
    case PrimitiveType::SCALE:
        apply_scale(op, registers, accumulator);
        break;
    }

    sanitize_registers(registers);
}

std::vector<std::uint8_t> zeckendorf_indices(std::uint64_t value) {
    std::vector<std::uint64_t> fibonacci{1u, 2u};
    while (fibonacci.back() <= std::numeric_limits<std::uint64_t>::max() - fibonacci[fibonacci.size() - 2u]) {
        fibonacci.push_back(fibonacci.back() + fibonacci[fibonacci.size() - 2u]);
    }

    std::vector<std::uint8_t> indices;
    if (value == 0u) {
        return indices;
    }

    for (std::size_t i = fibonacci.size(); i-- > 0;) {
        if (fibonacci[i] > value) {
            continue;
        }
        value -= fibonacci[i];
        indices.push_back(static_cast<std::uint8_t>(i));
        if (i == 0u) {
            break;
        }
        --i; // intentional double decrement: loop header also decrements,
             // skipping the next Fibonacci number to enforce the
             // Zeckendorf non-consecutive-terms guarantee
    }
    return indices;
}

PrimitiveType remapped_primitive(std::uint64_t accumulator,
                                 const std::vector<std::uint8_t>& zeckendorf) {
    constexpr std::array<PrimitiveType, 8> ring{{
        PrimitiveType::ROTATE_PAIR,
        PrimitiveType::FIBONACCI_Q,
        PrimitiveType::BINOMIAL_CASCADE,
        PrimitiveType::SIERPINSKI_COUPLING,
        PrimitiveType::FOLD,
        PrimitiveType::REFLECT,
        PrimitiveType::ACCUMULATOR_INJECT,
        PrimitiveType::SCALE
    }};

    const std::size_t code = static_cast<std::size_t>(accumulator & 0x3u);
    const std::size_t base = zeckendorf.empty() ? 0u : (zeckendorf.front() % ring.size());
    const std::size_t stride = (zeckendorf.size() < 2u)
        ? 1u
        : (1u + 2u * (zeckendorf[1] % 4u));
    return ring[(base + code * stride) % ring.size()];
}

std::size_t zeckendorf_jump(const std::vector<std::uint8_t>& zeckendorf,
                            std::uint64_t accumulator,
                            std::size_t source_index) {
    std::uint64_t mix = (accumulator >> 32u) ^ static_cast<std::uint64_t>(source_index + 1u);
    for (std::size_t i = 0; i < zeckendorf.size(); ++i) {
        mix += static_cast<std::uint64_t>(zeckendorf[i] + 1u)
            * static_cast<std::uint64_t>(2u * i + 3u);
    }
    return 1u + static_cast<std::size_t>(mix % (SEED_TABLE_SIZE - 1u));
}

bool should_full_mutate(std::uint64_t accumulator,
                        std::size_t source_index,
                        const std::vector<std::uint8_t>& zeckendorf) {
    const std::uint32_t coprime_term =
        (std::gcd(static_cast<std::uint32_t>(accumulator),
                  static_cast<std::uint32_t>(SEED_TABLE_SIZE)) == 1u) ? 1u : 0u;
    const std::uint32_t pop = popcount64(accumulator ^ (accumulator >> 23u));
    const std::uint32_t density = static_cast<std::uint32_t>(zeckendorf.size());
    const std::uint32_t signature =
        (pop + 3u * density + 5u * static_cast<std::uint32_t>(source_index) + 7u * coprime_term) % 11u;
    return signature < 6u; // threshold 6/11 ≈ 54.5% full-mutation rate — intentionally near 50/50
    // so aggressive and gentle mutation are roughly balanced across the tape
}

MutationEvent full_mutate(TableArray& table,
                          std::uint64_t accumulator,
                          const RegisterArray& registers,
                          std::size_t source_index,
                          std::size_t step,
                          bool from_finalization,
                          const std::vector<std::uint8_t>& zeckendorf) {
    const std::size_t jump = zeckendorf_jump(zeckendorf, accumulator, source_index);
    const std::size_t target = (source_index + jump) % SEED_TABLE_SIZE;

    MutationEvent event;
    event.step = step;
    event.from_finalization = from_finalization;
    event.mode = MutationMode::FULL;
    event.source_index = source_index;
    event.target_index = target;
    event.before = table[target];
    event.accumulator = accumulator;

    if (((accumulator >> 2u) & 1u) != 0u) {
        table[target].primitive = remapped_primitive(accumulator, zeckendorf);
    }

    const double blend = 0.5
        * (registers[event.before.reg_a % SEED_OUTPUT_SIZE]
           + registers[event.before.reg_b % SEED_OUTPUT_SIZE]);
    const double delta_phase =
        reduced_phase(accumulator ^ canonical_double_bits(blend), kInvTauU64);
    const double delta = 0.25 * std::sin(delta_phase) + 0.125 * blend;
    table[target].parameter = safe_tanh(table[target].parameter + delta);

    if (((accumulator >> 4u) & 1u) != 0u && !zeckendorf.empty()) {
        table[target].reg_a = static_cast<std::uint8_t>(zeckendorf.front() % SEED_OUTPUT_SIZE);
    }
    if (((accumulator >> 5u) & 1u) != 0u && zeckendorf.size() >= 2u) {
        table[target].reg_b = static_cast<std::uint8_t>(zeckendorf.back() % SEED_OUTPUT_SIZE);
    }

    canonicalize_entry(table[target]);
    event.after = table[target];
    return event;
}

MutationEvent soft_mutate(TableArray& table,
                          std::uint64_t accumulator,
                          std::size_t source_index,
                          std::size_t step,
                          bool from_finalization) {
    MutationEvent event;
    event.step = step;
    event.from_finalization = from_finalization;
    event.mode = MutationMode::SOFT;
    event.source_index = source_index;
    event.target_index = source_index;
    event.before = table[source_index];
    event.accumulator = accumulator;

    const double phase = reduced_phase(accumulator, kInvTauU64);
    table[source_index].parameter *= (1.0 + 0.001 * std::sin(phase));
    if (!std::isfinite(table[source_index].parameter)) {
        table[source_index].parameter = 0.0;
    }

    canonicalize_entry(table[source_index]);
    event.after = table[source_index];
    return event;
}

std::uint8_t synthetic_finalization_byte(const MachineState& state,
                                         std::size_t pass_index) {
    const std::uint64_t reg_bits =
        canonical_double_bits(state.registers[pass_index % SEED_OUTPUT_SIZE]);
    const std::uint64_t lane =
        state.accumulator >> ((pass_index & 7u) * 8u);
    return static_cast<std::uint8_t>(
        reg_bits ^ lane ^ static_cast<std::uint64_t>(pass_index * 0x9Du));
}

ByteTape load_expanded_tape(const std::vector<std::uint8_t>& expanded) {
    ByteTape tape{};
    const std::size_t count = std::min(expanded.size(), tape.size());
    for (std::size_t i = 0; i < count; ++i) {
        tape[i] = expanded[i];
    }
    return tape;
}

std::array<double, SEED_OUTPUT_SIZE> normalize_output(const RegisterArray& registers) {
    std::array<double, SEED_OUTPUT_SIZE> output{};
    for (std::size_t i = 0; i < registers.size(); ++i) {
        output[i] = (safe_tanh(registers[i]) + 1.0) * 0.5;
    }
    return output;
}

MutationEvent run_machine_step(MachineState& state,
                               std::uint8_t byte,
                               std::size_t step,
                               bool from_finalization,
                               std::array<std::uint32_t, SEED_TABLE_SIZE>& full_counts,
                               std::array<std::uint32_t, SEED_TABLE_SIZE>& soft_counts) {
    const std::size_t source_index =
        static_cast<std::size_t>((byte ^ static_cast<std::uint8_t>(state.accumulator)) % SEED_TABLE_SIZE);

    OpEntry op = state.table[source_index];
    canonicalize_entry(op);
    apply_primitive(op, state.registers, state.accumulator);

    const std::uint64_t digest = register_digest(state.registers);
    state.accumulator = mix_accumulator(state.accumulator, byte, digest);

    const std::vector<std::uint8_t> zeckendorf = zeckendorf_indices(state.accumulator);

    if (should_full_mutate(state.accumulator, source_index, zeckendorf)) {
        MutationEvent event = full_mutate(
            state.table,
            state.accumulator,
            state.registers,
            source_index,
            step,
            from_finalization,
            zeckendorf);          // pass cached result
        ++full_counts[event.target_index];
        return event;
    }

    MutationEvent event = soft_mutate(
        state.table,
        state.accumulator,
        source_index,
        step,
        from_finalization);
    ++soft_counts[event.target_index];
    return event;
}

} // namespace

SeedMachineTrace compress_with_trace(const std::vector<std::uint8_t>& expanded) {
    assert(expanded.size() == SEED_EXPANDED_SIZE);

    const ByteTape tape = load_expanded_tape(expanded);
    SeedMachineTrace trace;
    trace.final_state = make_initial_machine_state();
    trace.mutation_events.reserve(SEED_EXPANDED_SIZE + kFinalizationPasses);

    for (std::size_t step = 0; step < tape.size(); ++step) {
        trace.mutation_events.push_back(
            run_machine_step(trace.final_state,
                             tape[step],
                             step,
                             false,
                             trace.full_mutation_counts,
                             trace.soft_mutation_counts));
    }

    for (std::size_t pass = 0; pass < kFinalizationPasses; ++pass) {
        const std::uint8_t synthetic = synthetic_finalization_byte(trace.final_state, pass);
        trace.mutation_events.push_back(
            run_machine_step(trace.final_state,
                             synthetic,
                             SEED_EXPANDED_SIZE + pass,
                             true,
                             trace.full_mutation_counts,
                             trace.soft_mutation_counts));
    }

    trace.output = normalize_output(trace.final_state.registers);
    return trace;
}

std::vector<double> compress(const std::vector<std::uint8_t>& expanded) {
    const SeedMachineTrace trace = compress_with_trace(expanded);
    return std::vector<double>(trace.output.begin(), trace.output.end());
}
