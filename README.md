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
- `Cosmos Explorer`
  A multi-scale universe navigator with a scale ladder, object catalog, live
  N-body sandbox, and per-tier physics derived from the law genome.
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

## Interface & Design System

Worldline ships a single, cohesive interface language — a premium glassmorphic
HUD built to feel like an instrument panel from a hard-sci-fi observatory, not a
generic neon dashboard. Every screen is composed from one shared set of drawing
primitives in `src/ui/UiPrimitives.hpp`, so material, depth, light, and motion
read identically everywhere.

**Frosted glass, done with restraint**

- Every panel is a translucent frosted pane (`draw_glass_panel`) layered over a
  living starfield, so the background genuinely bleeds through the surface.
- Depth is built from real material cues rather than blur tricks: a soft drop
  shadow for elevation, a single low-alpha top sheen, a crisp accent hairline
  border, a bright top-rim light catch, and a grounded bottom shade with a
  powered accent underglow.
- A disciplined palette — deep-void blacks, bioluminescent cyan, exotic violet,
  hot xenon, and plasma green — with **one accent per surface**. No rainbow
  gradients, no glow-on-everything.

**Sci-fi HUD framing**

- Live HUD surfaces (title bar, simulation dock, force inspector) carry L-shaped
  corner brackets, tick detailing, and a slow scan-sweep line for an engineered,
  targeting-reticle feel.
- Status is alive but subtle: a quiet pulse on the live indicator dots only when
  a system is actually running — motion as seasoning, never decoration.

**Atmosphere & typography**

- The global backdrop layers nebula blooms, a deterministic twinkling starfield,
  a cinematic edge vignette, and a barely-there scanline grain.
- Type prefers a sharp, technical monospace, auto-discovered across Windows,
  Linux, and macOS so the instrument renders with real type on every platform.

**Shared components**

Cards, accented cards, buttons, badges, checkboxes, metric tiles, probe rows,
and scrollbars all derive from the same glass core, giving consistent hover
lift, pressed-inset feedback, and accent-aware edges across the Guided flow,
Seed Workspace, Universe Atlas, Cosmos Explorer, Trace, and Reference System.

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
- multi-scale Cosmos Explorer with a scale ladder, object catalog, and live
  N-body sandbox with per-tier physics derived from the law genome
- universe classification with granular signature metrics and an observer fleet
- 3D navigation boilerplate for future free-flight exploration
- cohesive glassmorphic, sci-fi HUD interface shared across every screen

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
