#pragma once

#include "app/AppTypes.hpp"
#include "app/UniverseModel.hpp"

UniverseProject make_universe_project(const SeededUniverseUiState& seeded);
void apply_universe_project(SeededUniverseUiState& seeded, const UniverseProject& project);
