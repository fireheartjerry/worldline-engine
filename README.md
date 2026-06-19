# Worldline Universe Studio

Worldline is a desktop simulation instrument built with C++ and raylib.

Its flagship experience is a deterministic seeded universe pipeline:

- enter a seed string
- generate a `MetaSpec` (a vault of tensors: metric, potential, coupling, gyroscopic, warp …) and assemble a `LawSpec` equation of motion
- advect a swarm of tens of thousands of test masses through the **actual generated law** every frame
- paint their world-lines as a luminous **phase-flow field** — vortices, spiral arms, shear sheets, saddles and accretion rings emerge directly from the seed

Each universe receives a signature colour palette derived from its own invariants, so no two seeds look alike, and an exotic-index / vorticity / flux / order readout for at-a-glance comparison.

The Newtonian double pendulum is still included as a polished **reference system** for comparison, but the seeded universe is now rendered by the dedicated `FieldRenderer` rather than squashed onto two rods.

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

- deterministic seeded universe generation with genuinely exotic, varied laws
  (anisotropic metrics, saddle/well/ridge potentials, gyroscopic vortices, warp)
- live `FieldRenderer`: multithreaded advection of ~13k test masses through the
  generated law, additive glow trails, cheap multi-tap bloom, a static streamline
  atlas, a per-universe starfield, and a highlighted "hero" world-line
- per-universe signature colour palette + live exotic-index / flux / swirl / order metrics
- `tools/worldline_field_preview`: offline BMP renderer for universe "postcards" (no GPU needed)
- saved local universe projects with title, notes, markers, and thumbnails
- atlas search over seed text, descriptors, and derived metrics
- timeline recording with scrubbing and pinned frames
- glossary-backed terminology for advanced concepts
- persistent local settings and recent project recovery
- reference Newtonian pendulum workspace for comparison

## Build

### Configure

```powershell
cmake --preset default
```

### Build

```powershell
cmake --build build
```

### Run

```powershell
.\build\worldline.exe
```

## Tests

```powershell
ctest --test-dir build --output-on-failure
```

The current automated coverage includes:

- seed determinism verification
- generated physics verification
- metaspec verification
- app-layer persistence and atlas query verification

## Packaging

Create a distributable ZIP package with:

```powershell
cmake --build build --target package
```

## Project Layout

- `src/app` app shell, scene state, runtime ownership, persistence, copy
- `src/physics` law stepping and observable extraction
- `src/seed` deterministic generation pipeline
- `src/renderer` `FieldRenderer` (live flow-field engine) + `FlowOperator` (shared lean force operator) + the reference pendulum renderer
- `src/ui` desktop UI scenes and drawing primitives
- `tools` offline field preview + corpus diagnostics
- `tests` verification programs

## Field preview tool

Render universe "postcards" to a BMP without opening a window:

```powershell
.\build\worldline_field_preview.exe --size 460 andromeda vortex-sigma saddle-rift helix-nine
```

## Tooling

- `.clang-format` for consistent formatting
- `CMakePresets.json` for repeatable local configure flows
- GitHub Actions CI for Windows build-and-test validation

## License

MIT
