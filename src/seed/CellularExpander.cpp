#include "seed/CellularExpander.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

using ByteArray = std::array<std::uint8_t, SEED_EXPANDED_SIZE>;

constexpr std::size_t kCellMask = SEED_EXPANDED_SIZE - 1;
constexpr std::size_t kMixingGenerations = 128;
constexpr double kUniformLambda = 0.2;
constexpr double kGoldenRatio = 1.6180339887498948482;
constexpr double kEulerE = 2.7182818284590452354;
constexpr double kPi = 3.1415926535897932385;
constexpr double kSqrt2 = 1.4142135623730950488;
constexpr double kSqrt3 = 1.7320508075688772935;
constexpr double kSqrt5 = 2.2360679774997896964;

static_assert((SEED_EXPANDED_SIZE & kCellMask) == 0,
              "Expanded seed size must stay a power of two");

constexpr std::array<std::array<double, 4>, 5> kSimplexVertices{{
    {{ 1.0,  1.0,  1.0, -1.0 / kSqrt5 }},
    {{ 1.0, -1.0, -1.0, -1.0 / kSqrt5 }},
    {{-1.0,  1.0, -1.0, -1.0 / kSqrt5 }},
    {{-1.0, -1.0,  1.0, -1.0 / kSqrt5 }},
    {{ 0.0,  0.0,  0.0,  4.0 / kSqrt5 }}
}};

std::uint8_t rotl8(std::uint8_t value, unsigned amount) {
    amount &= 7u;
    return static_cast<std::uint8_t>((value << amount) | (value >> ((8u - amount) & 7u)));
}

std::uint8_t absorb_fold(std::uint8_t prior,
                         std::uint8_t value,
                         std::uint64_t position,
                         std::uint8_t salt) {
    const std::uint8_t phase =
        static_cast<std::uint8_t>((position * 29u + static_cast<std::uint64_t>(salt) * 17u) & 0xFFu);
    const std::uint8_t mixed = rotl8(
        static_cast<std::uint8_t>(value + phase),
        static_cast<unsigned>((position + salt) & 7u));
    return static_cast<std::uint8_t>(
        rotl8(prior, 1u)
        ^ mixed
        ^ static_cast<std::uint8_t>((position * 13u + salt) & 0xFFu));
}

void fold_input(const std::string& input,
                ByteArray& current,
                ByteArray& previous) {
    for (std::size_t i = 0; i < input.size(); ++i) {
        const std::uint8_t value = static_cast<std::uint8_t>(input[i]);
        const std::size_t current_slot = i & kCellMask;
        const std::size_t previous_slot = (i * 89u + 233u) & kCellMask;
        current[current_slot] = absorb_fold(current[current_slot], value, i, 0x3Du);
        previous[previous_slot] = absorb_fold(previous[previous_slot], value, i, 0xA7u);
    }
}

std::array<double, 5> normalize_barycentric(const std::array<std::uint8_t, 5>& cells) {
    std::array<double, 5> lambda{};
    std::uint32_t sum = 0;
    for (std::uint8_t value : cells) {
        sum += value;
    }

    if (sum == 0u) {
        lambda.fill(kUniformLambda);
        return lambda;
    }

    const double inverse_sum = 1.0 / static_cast<double>(sum);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        lambda[i] = static_cast<double>(cells[i]) * inverse_sum;
    }
    return lambda;
}

std::array<double, 4> simplex_point(const std::array<double, 5>& lambda) {
    std::array<double, 4> point{};
    for (std::size_t vertex = 0; vertex < kSimplexVertices.size(); ++vertex) {
        for (std::size_t axis = 0; axis < point.size(); ++axis) {
            point[axis] += lambda[vertex] * kSimplexVertices[vertex][axis];
        }
    }
    return point;
}

std::array<double, 4> quaternion_exp(const std::array<double, 4>& q) {
    const double w = q[0];
    const double x = q[1];
    const double y = q[2];
    const double z = q[3];
    const double vector_norm = std::sqrt(x * x + y * y + z * z);
    const double exp_w = std::exp(w);

    if (vector_norm <= 1.0e-15) {
        return {{exp_w, 0.0, 0.0, 0.0}};
    }

    const double scale = exp_w * std::sin(vector_norm) / vector_norm;
    return {{
        exp_w * std::cos(vector_norm),
        x * scale,
        y * scale,
        z * scale
    }};
}

double fract_positive(double value) {
    double integral = 0.0;
    double fractional = std::modf(value, &integral);
    if (fractional < 0.0) {
        fractional += 1.0;
    }
    if (fractional >= 1.0) {
        fractional = std::nextafter(1.0, 0.0);
    }
    if (fractional <= 0.0) {
        return 0.0;
    }
    return fractional;
}

std::uint8_t local_rule_byte(const std::array<std::uint8_t, 5>& cells) {
    const std::array<double, 5> lambda = normalize_barycentric(cells);
    const std::array<double, 4> q = simplex_point(lambda);
    const std::array<double, 4> qexp = quaternion_exp(q);

    // Use the full quaternion exponential, not only its norm. The cross term
    // makes the vector part materially affect the byte stream.
    const double mixed =
        qexp[0] * kGoldenRatio
        + qexp[1] * kEulerE
        + qexp[2] * kSqrt2
        + qexp[3] * kSqrt3
        + (qexp[1] * qexp[2] - qexp[0] * qexp[3]) * kSqrt5
        + lambda[4] * kPi;
    const double fractional = fract_positive(mixed);
    const double scaled = std::floor(256.0 * fractional);
    return static_cast<std::uint8_t>(std::min(255.0, scaled));
}

ByteArray step_ca(const ByteArray& current,
                  const ByteArray& previous) {
    ByteArray next{};
    for (std::size_t i = 0; i < SEED_EXPANDED_SIZE; ++i) {
        const std::array<std::uint8_t, 5> cells{{
            current[(i - 2u) & kCellMask],
            current[(i - 1u) & kCellMask],
            current[i],
            current[(i + 1u) & kCellMask],
            current[(i + 2u) & kCellMask]
        }};
        next[i] = static_cast<std::uint8_t>(local_rule_byte(cells) ^ previous[i]);
    }
    return next;
}

void push_checkpoint(CellularExpansionTrace& trace,
                     std::size_t generation,
                     const ByteArray& state) {
    CellularCheckpoint checkpoint;
    checkpoint.generation = generation;
    checkpoint.cells.assign(state.begin(), state.end());
    trace.checkpoints.push_back(checkpoint);
}

} // namespace

CellularExpansionTrace expand_with_trace(const std::string& input) {
    CellularExpansionTrace trace;
    trace.expanded.reserve(SEED_EXPANDED_SIZE);

    ByteArray current{};
    ByteArray previous{};
    fold_input(input, current, previous);
    push_checkpoint(trace, 0u, current);

    constexpr std::array<std::size_t, 8> kCheckpointGenerations{{
        1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u
    }};
    std::size_t checkpoint_index = 0u;

    for (std::size_t generation = 0; generation < kMixingGenerations; ++generation) {
        const ByteArray next = step_ca(current, previous);
        previous = current;
        current = next;

        const std::size_t completed_generation = generation + 1u;
        if (checkpoint_index < kCheckpointGenerations.size()
            && completed_generation == kCheckpointGenerations[checkpoint_index]) {
            push_checkpoint(trace, completed_generation, current);
            ++checkpoint_index;
        }
    }

    trace.expanded.assign(current.begin(), current.end());
    return trace;
}

std::vector<std::uint8_t> expand(const std::string& input) {
    return expand_with_trace(input).expanded;
}
