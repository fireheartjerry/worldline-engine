#include "app/AppTypes.hpp"

#include "app/SeededUniverseRuntime.hpp"

#include <utility>

SeededUniverseUiState::SeededUniverseUiState()
    : runtime(std::make_unique<SeededUniverseRuntime>()) {}

SeededUniverseUiState::~SeededUniverseUiState() = default;

SeededUniverseUiState::SeededUniverseUiState(SeededUniverseUiState&& other) noexcept = default;

SeededUniverseUiState& SeededUniverseUiState::operator=(SeededUniverseUiState&& other) noexcept = default;
