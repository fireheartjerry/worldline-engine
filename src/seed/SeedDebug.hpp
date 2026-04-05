#pragma once
#include "CellularExpander.hpp"
#include "SeedMachine.hpp"
#include "Seeder.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iosfwd>
#include <ostream>
#include <string>
#include <vector>

inline const char* primitive_name(PrimitiveType primitive) {
    switch (primitive) {
    case PrimitiveType::ROTATE_PAIR: return "ROTATE_PAIR";
    case PrimitiveType::FIBONACCI_Q: return "FIBONACCI_Q";
    case PrimitiveType::BINOMIAL_CASCADE: return "BINOMIAL_CASCADE";
    case PrimitiveType::SIERPINSKI_COUPLING: return "SIERPINSKI_COUPLING";
    case PrimitiveType::FOLD: return "FOLD";
    case PrimitiveType::REFLECT: return "REFLECT";
    case PrimitiveType::ACCUMULATOR_INJECT: return "ACCUMULATOR_INJECT";
    case PrimitiveType::SCALE: return "SCALE";
    }
    return "UNKNOWN";
}

inline const char* mutation_mode_name(MutationMode mode) {
    return mode == MutationMode::FULL ? "FULL" : "SOFT";
}

inline void print_output(const std::vector<double>& output, std::ostream& os) {
    os << std::fixed << std::setprecision(17);
    for (std::size_t i = 0; i < output.size(); ++i) {
        os << "[" << i << "] " << output[i] << '\n';
    }
}

inline void print_register_state(const SeedMachineTrace& trace, std::ostream& os) {
    os << std::fixed << std::setprecision(17);
    for (std::size_t i = 0; i < trace.final_state.registers.size(); ++i) {
        os << "r[" << i << "] = " << trace.final_state.registers[i] << '\n';
    }
}

inline void print_mutation_trace(const SeedMachineTrace& trace, std::ostream& os) {
    os << std::fixed << std::setprecision(17);
    for (const MutationEvent& event : trace.mutation_events) {
        os << "step=" << event.step
           << " final=" << (event.from_finalization ? "yes" : "no")
           << " mode=" << mutation_mode_name(event.mode)
           << " src=" << event.source_index
           << " dst=" << event.target_index
           << " before={" << primitive_name(event.before.primitive)
           << ", a=" << static_cast<int>(event.before.reg_a)
           << ", b=" << static_cast<int>(event.before.reg_b)
           << ", p=" << event.before.parameter
           << "} after={" << primitive_name(event.after.primitive)
           << ", a=" << static_cast<int>(event.after.reg_a)
           << ", b=" << static_cast<int>(event.after.reg_b)
           << ", p=" << event.after.parameter
           << "} acc=0x" << std::hex << event.accumulator << std::dec << '\n';
    }

    os << "\nFull mutation counts\n";
    for (std::size_t i = 0; i < trace.full_mutation_counts.size(); ++i) {
        os << "[" << i << "] " << trace.full_mutation_counts[i] << '\n';
    }
    os << "\nSoft mutation counts\n";
    for (std::size_t i = 0; i < trace.soft_mutation_counts.size(); ++i) {
        os << "[" << i << "] " << trace.soft_mutation_counts[i] << '\n';
    }
}

inline void print_ca_expansion(const CellularExpansionTrace& trace, std::ostream& os) {
    os << std::hex << std::setfill('0');
    for (const CAGenerationPreview& preview : trace.generations) {
        os << "generation " << std::dec << preview.generation << " first8=";
        for (std::uint8_t value : preview.first8) {
            os << std::hex << std::setw(2) << static_cast<int>(value);
        }
        os << " last8=";
        for (std::uint8_t value : preview.last8) {
            os << std::hex << std::setw(2) << static_cast<int>(value);
        }
        os << std::dec << '\n';
    }
}

inline std::uint32_t popcount32(std::uint32_t value) {
    std::uint32_t count = 0;
    while (value != 0) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

inline std::uint16_t quantize_output16(double value) {
    const double clamped = std::max(0.0, std::min(1.0, value));
    return static_cast<std::uint16_t>(std::llround(clamped * 65535.0));
}

inline std::size_t output_hamming_distance_16bit(const std::vector<double>& lhs,
                                                 const std::vector<double>& rhs) {
    const std::size_t count = std::min(lhs.size(), rhs.size());
    std::size_t distance = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint16_t a = quantize_output16(lhs[i]);
        const std::uint16_t b = quantize_output16(rhs[i]);
        distance += popcount32(static_cast<std::uint32_t>(a ^ b));
    }
    return distance;
}

inline void print_avalanche_report(const std::string& lhs,
                                   const std::string& rhs,
                                   std::ostream& os) {
    const std::vector<double> left = generate(lhs);
    const std::vector<double> right = generate(rhs);
    os << "lhs=\"" << lhs << "\"\n";
    print_output(left, os);
    os << "rhs=\"" << rhs << "\"\n";
    print_output(right, os);
    os << "16-bit hamming distance = "
       << output_hamming_distance_16bit(left, right) << '\n';
    os << "per-slot abs diff\n";
    os << std::fixed << std::setprecision(17);
    for (std::size_t i = 0; i < std::min(left.size(), right.size()); ++i) {
        os << "[" << i << "] " << std::abs(left[i] - right[i]) << '\n';
    }
}
