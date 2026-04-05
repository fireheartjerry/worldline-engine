#include "seed/Seeder.hpp"

#include "seed/CellularExpander.hpp"
#include "seed/SeedMachine.hpp"

std::vector<double> generate(const std::string& input) {
    return compress(expand(input));
}
