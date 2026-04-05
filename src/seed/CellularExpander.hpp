#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

static constexpr std::size_t SEED_EXPANDED_SIZE = 512;

struct CellularExpansionTrace {
    std::vector<std::uint8_t> expanded;
};

std::vector<std::uint8_t> expand(const std::string& input);
CellularExpansionTrace expand_with_trace(const std::string& input);
