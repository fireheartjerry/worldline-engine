# Worldline Universe Studio

Worldline is a desktop simulation instrument built with C++ and raylib.

Its flagship experience is a deterministic seeded universe pipeline:

- enter a seed string
- generate a `MetaSpec`, `LawSpec`, and `ObservableExtractor`
- run the generated law live every frame
- render the resulting motion through the existing pendulum renderer as a visual backend

The Newtonian pendulum is still included, but it is framed as a reference system rather than the main product surface.

## Product Structure

- `Guided First Universe`
  A seed-first onboarding flow that explains the deterministic pipeline and opens directly into the live workspace.
- `Seed Workspace`
  The main live simulation view with a large stage, contextual inspector, timeline, notes, saving, and direct access to trace tools.
- `Universe Atlas`
  A local gallery of saved universes with search, compare, derived descriptors, and visual fingerprints.
- `Trace`
  A cleaner inspection surface for generated law summaries, preview paths, and tensor views.
- `Reference System`
  The Newtonian double-pendulum lab, preserved as a polished baseline model.

## Core Principles

- deterministic seed pipeline
- no cloud dependency or telemetry
- direct-manipulation editing on the live stage
- grounded physics terminology in the UI
- renderer quality unchanged between seeded and reference modes

## Features

- deterministic seeded universe generation
- live `LawSpec` stepping with observable extraction every frame
- pendulum-quality rendering and trail playback for seeded universes
- saved local universe projects with title, notes, markers, and thumbnails
- atlas search over seed text, descriptors, and derived metrics
- timeline recording with scrubbing and pinned frames
- glossary-backed terminology for advanced concepts
- persistent local settings and recent project recovery
- reference Newtonian pendulum workspace for comparison

## Build

Worldline builds on Windows, Linux, and macOS. CI validates all three.

### Linux dependencies

raylib is built from source and needs the usual desktop GL/X11 development
headers. On Debian/Ubuntu:

```bash
sudo apt-get install libasound2-dev libx11-dev libxrandr-dev libxi-dev \
  libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libxkbcommon-dev
```

macOS and Windows need no extra packages beyond CMake and a C++17 compiler.

### Configure

```bash
cmake --preset default
```

### Build

```bash
cmake --build build
```

### Run

```bash
# Windows
.\build\worldline.exe
# Linux / macOS
./build/worldline
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The current automated coverage includes:

- seed determinism verification
- generated physics verification
- metaspec verification
- app-layer persistence and atlas query verification
- corrupt save-file recovery (malformed projects and settings)

## Data location

Saved universes and settings live in a per-user application data directory
(`%APPDATA%\Worldline` on Windows, `~/Library/Application Support/Worldline` on
macOS, `$XDG_DATA_HOME/worldline` or `~/.local/share/worldline` on Linux). Set
the `WORLDLINE_DATA_DIR` environment variable to override this with a portable
or custom location.

## Packaging

Create a distributable ZIP package with:

```powershell
cmake --build build --target package
```

## Project Layout

- `src/app` app shell, scene state, runtime ownership, persistence, copy
- `src/physics` law stepping and observable extraction
- `src/seed` deterministic generation pipeline
- `src/ui` desktop UI scenes and drawing primitives
- `tests` verification programs

## Tooling

- `.clang-format` for consistent formatting
- `CMakePresets.json` for repeatable local configure flows
- GitHub Actions CI for Windows build-and-test validation

## License

MIT
