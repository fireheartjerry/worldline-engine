cmake_minimum_required(VERSION 3.16)
project(worldline VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

function(worldline_apply_warnings target_name)
    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
FetchContent_MakeAvailable(raylib)

add_library(worldline_seed
    src/seed/CellularExpander.cpp
    src/seed/MetaSpec.cpp
    src/seed/SeedMachine.cpp
    src/seed/Seeder.cpp
)
target_include_directories(worldline_seed PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
worldline_apply_warnings(worldline_seed)

add_library(worldline_physics
    src/physics/LawSpec.cpp
    src/physics/ObservableExtractor.cpp
)
target_include_directories(worldline_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_physics PUBLIC worldline_seed)
worldline_apply_warnings(worldline_physics)

add_library(worldline_cosmos
    src/cosmos/LawGenome.cpp
    src/cosmos/ScaleLadder.cpp
    src/cosmos/ObjectCatalog.cpp
    src/cosmos/NBodySystem.cpp
    src/cosmos/Sandbox.cpp
    src/cosmos/Analysis.cpp
    src/cosmos/Universe.cpp
    src/cosmos/ObserverFleet.cpp
    src/cosmos/ProcUniverse.cpp
    src/cosmos/Ecosystem.cpp
    src/cosmos/Nav3D.cpp
)
target_include_directories(worldline_cosmos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos PUBLIC worldline_seed)
worldline_apply_warnings(worldline_cosmos)

add_library(worldline_app
    src/app/AppTypes.cpp
    src/app/SeededUniverseRuntime.cpp
    src/app/UniverseProject.cpp
    src/app/WorldlineCopy.cpp
    src/app/WorldlineStorage.cpp
    src/app/WorldlineStorageSettings.cpp
)
target_include_directories(worldline_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_app PUBLIC worldline_seed worldline_physics worldline_cosmos)
worldline_apply_warnings(worldline_app)

add_executable(worldline
    src/main.cpp
    src/ui/CosmosExplorerScene.cpp
    src/ui/CosmosExplorerPanels.cpp
    src/ui/CosmosDescent.cpp
    src/ui/GuidedFirstUniverseScene.cpp
    src/ui/SeedWorkspaceScene.cpp
    src/ui/SeedWorkspacePanels.cpp
    src/ui/TraceScene.cpp
    src/ui/UniverseAtlasScene.cpp
)
target_include_directories(worldline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline raylib worldline_app worldline_cosmos)
worldline_apply_warnings(worldline)

add_executable(worldline_physics_tests tests/physics_verification.cpp)
target_include_directories(worldline_physics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_physics_tests PRIVATE worldline_physics)
worldline_apply_warnings(worldline_physics_tests)
add_test(NAME physics_verification COMMAND worldline_physics_tests)

add_executable(worldline_seed_tests tests/seed_verification.cpp)
target_include_directories(worldline_seed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_seed_tests PRIVATE worldline_seed)
worldline_apply_warnings(worldline_seed_tests)
add_test(NAME seed_verification COMMAND worldline_seed_tests)

add_executable(worldline_metaspec_tests tests/metaspec_verification.cpp)
target_include_directories(worldline_metaspec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_metaspec_tests PRIVATE worldline_seed)
worldline_apply_warnings(worldline_metaspec_tests)
add_test(NAME metaspec_verification COMMAND worldline_metaspec_tests)

add_executable(worldline_app_tests tests/app_verification.cpp)
target_include_directories(worldline_app_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_app_tests PRIVATE worldline_app)
worldline_apply_warnings(worldline_app_tests)
add_test(NAME app_verification COMMAND worldline_app_tests)

add_executable(worldline_cosmos_tests tests/cosmos_verification.cpp)
target_include_directories(worldline_cosmos_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_tests)
add_test(NAME cosmos_verification COMMAND worldline_cosmos_tests)

add_executable(worldline_cosmos_tiers_tests tests/cosmos_tiers_verification.cpp)
target_include_directories(worldline_cosmos_tiers_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_tiers_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_tiers_tests)
add_test(NAME cosmos_tiers_verification COMMAND worldline_cosmos_tiers_tests)

add_executable(worldline_cosmos_universe_tests tests/cosmos_universe_verification.cpp)
target_include_directories(worldline_cosmos_universe_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_universe_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_universe_tests)
add_test(NAME cosmos_universe_verification COMMAND worldline_cosmos_universe_tests)

add_executable(worldline_cosmos_orbits_tests tests/cosmos_orbits_verification.cpp)
target_include_directories(worldline_cosmos_orbits_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_orbits_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_orbits_tests)
add_test(NAME cosmos_orbits_verification COMMAND worldline_cosmos_orbits_tests)

add_executable(worldline_cosmos_procgen_tests tests/cosmos_procgen_verification.cpp)
target_include_directories(worldline_cosmos_procgen_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_procgen_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_procgen_tests)
add_test(NAME cosmos_procgen_verification COMMAND worldline_cosmos_procgen_tests)

add_executable(worldline_cosmos_astrobio_tests tests/cosmos_astrobio_verification.cpp)
target_include_directories(worldline_cosmos_astrobio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_astrobio_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_astrobio_tests)
add_test(NAME cosmos_astrobio_verification COMMAND worldline_cosmos_astrobio_tests)

add_executable(worldline_cosmos_phonology_tests tests/cosmos_phonology_verification.cpp)
target_include_directories(worldline_cosmos_phonology_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_phonology_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_phonology_tests)
add_test(NAME cosmos_phonology_verification COMMAND worldline_cosmos_phonology_tests)

add_executable(worldline_cosmos_ecosystem_tests tests/cosmos_ecosystem_verification.cpp)
target_include_directories(worldline_cosmos_ecosystem_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_ecosystem_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_ecosystem_tests)
add_test(NAME cosmos_ecosystem_verification COMMAND worldline_cosmos_ecosystem_tests)

add_executable(worldline_cosmos_physics_tests tests/cosmos_physics_verification.cpp)
target_include_directories(worldline_cosmos_physics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_physics_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_physics_tests)
add_test(NAME cosmos_physics_verification COMMAND worldline_cosmos_physics_tests)

add_executable(worldline_cosmos_organism_tests tests/cosmos_organism_verification.cpp)
target_include_directories(worldline_cosmos_organism_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worldline_cosmos_organism_tests PRIVATE worldline_cosmos)
worldline_apply_warnings(worldline_cosmos_organism_tests)
add_test(NAME cosmos_organism_verification COMMAND worldline_cosmos_organism_tests)

install(TARGETS worldline RUNTIME DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)

set(CPACK_PACKAGE_NAME "Worldline")
set(CPACK_PACKAGE_VENDOR "Jerry Li")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "Worldline-${PROJECT_VERSION}-windows")
include(CPack)
