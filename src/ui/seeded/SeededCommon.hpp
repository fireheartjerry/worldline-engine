#pragma once
// Shared include surface for the Seeded Universe screen modules.
// All seeded/* headers pull this in so each stays focused on one concern.
#include "../MainMenuScreen.hpp"
#include "../UiPrimitives.hpp"

#include "../../app/SeededUniverseRuntime.hpp"
#include "../../app/AppTypes.hpp"
#include "../../physics/LawSpec.hpp"
#include "../../seed/MetaSpec.hpp"
#include "../../seed/SeedDebug.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <vector>
