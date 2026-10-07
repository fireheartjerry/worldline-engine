# Worldline architecture

Worldline is one C++17 executable (`worldline`) built on raylib, plus a set of
pure-CPU libraries that the app, the tests and the offline tools share.

```
worldline_seed      seed text -> bytes -> lanes -> MetaSpec      (src/seed)
worldline_physics   MetaSpec -> LawSpec, observables, pendulum   (src/physics)
worldline_cosmos    LawGenome, ProcUniverse, N-body, ecology,    (src/cosmos)
                    quantum / nuclear / atomic suites
worldline_app       app state, runtime, persistence, copy        (src/app)
worldline           main loop, UI scenes, renderers (raylib)     (src/main.cpp, src/ui, src/renderer)
```

Only `worldline` links raylib. Every library below it is deterministic,
windowless and covered by the test executables in `tests/`.

## The seeded-universe pipeline (Seed Workspace)

```
seed text
  └─ CellularExpander::expand      cellular automaton -> byte stream
      └─ SeedMachine::compress     mutation machine   -> numeric lanes
          └─ generate_meta_spec    lanes -> MetaSpec  (metric g, potential V,
                                   couplings C0/C1, symmetry S, warp W, p, ...)
              └─ LawSpec           the generated equation of motion
                  └─ SeededUniverseRuntime (src/app)
                       steps the law live, records history/markers, and feeds
                       ├─ FieldRenderer   ~13k particles advected through the law
                       │                  (FlowOperator = float mirror of
                       │                  LawSpec::derivative), the default stage
                       └─ ObservableExtractor -> pendulum draft -> Renderer
                                          the reference "pendulum view" (V)
```

`generate_descriptor(MetaSpec)` produces the prose reading shown in the
inspector and stored with saved projects. `SeededUniverseRuntime::config_token`
is bumped on every `configure()`, which is how `FieldRenderer` knows to rebuild
its swarm, palette, streamlines and starfield.

## The procedural universe (Cosmos Explorer)

```
seed text -> MetaSpec -> generate_law_genome -> LawGenome (couplings, exponents, signature)
                                                   ├─ ObjectCatalog + ScaleLadder + NBodySystem
                                                   │     the nine-tier N-body "Tier sandbox"
                                                   └─ ProcUniverse(genome.signature)
                                                         the descent map
```

`ProcUniverse` generates a tree of `ProcNode`s lazily:
Universe → Galaxy → StarSystem → Planet → Ecosystem → Creature. A node stores
its `children` as lightweight `ChildRef`s (seed, kind, layout position), and
`node(seed, kind, parent)` regenerates any node identically on demand, so the
engine keeps only an LRU cache and memory stays bounded however far you
explore. Context flows down through the `parent` argument (a planet's
habitability depends on its star; an ecosystem's climate on its planet).
Ecosystems build an `eco::Community` (phylogeny + food web), which
`ecosim::LiveSim` integrates at a fixed timestep while an ecosystem or
creature is in view.

The UI (`src/ui/CosmosDescent.cpp`) keeps a *path* of node copies from the root
to the focus. Engine references can be invalidated by later calls (LRU
eviction), so the UI never holds them across frames.

The header-only physics suites in `src/cosmos` (`QuantumScale`,
`StandardModel`, `NuclearData`, `AtomicStructure`, …) are pure functions with
cited constants; each has its own verification test. The `*Genesis` /
`Nucleosynthesis` modules turn a `LawGenome` into a per-universe verdict that
the tier-sandbox inspector displays.

## The frame loop (`src/main.cpp`)

Each frame: resize render targets → per-screen update (simulation stepping,
input) → `BeginDrawing` → draw the active scene → `EndDrawing` → apply the
scene's result struct (navigation between screens, saves, catalog refresh).
Scenes never switch screens themselves; they return intents
(`open_atlas`, `back_requested`, `toggle_view`, …) and `main` routes them.
Settings (window size, last screen, last seed) are saved on exit, and GPU
resources are released *before* `CloseWindow()`.

## Determinism contract

The same seed must produce the same universe. Rules that keep it true:

- No wall-clock time, `rand()`, or address-dependent ordering in generation.
  Seeds derive child seeds (`child_seed`) and xorshift-style local RNGs.
- Sorts whose order feeds generation use `std::stable_sort` (or a total-order
  comparator); `std::sort` tie order differs between STL implementations.
- `unordered_map` is used only for memoisation, never iterated into output.
- Render-time animation (orbits, wander, particle swarms) is never stored back
  into generated state.

`seed_verification`, `metaspec_verification` and the `cosmos_*` suites pin
this behaviour. The seed → genome path still calls the platform libm, so
bit-exact results are only guaranteed per toolchain; Windows/MSVC is the
reference platform and CI also runs the full suite on Linux/GCC.

## UI conventions

- All screens draw with the shared primitives in `src/ui/UiPrimitives.hpp`
  (glass panels, buttons, metric tiles, scrollbars) and scale every size by a
  per-screen `scale`/`ui` factor derived from the viewport.
- Layout is computed by a pure function where two parties need to agree on it
  (e.g. `seed_workspace_layout()` is used by both `main` and the scene).
- raylib details that have bitten this codebase:
  - The default exit key is Escape; `main` disables it because Escape is the
    in-app "back / close" key.
  - Render textures are stored bottom-up. To sample the screen rectangle `r` of
    a full-screen render texture with `DrawTexturePro`, use the source
    `{r.x, H - r.y - r.height, r.width, -r.height}`.
  - A default-constructed `VectorOverlayConfig` is "everything on".

## Tests

Each suite is its own executable in `tests/`, registered in `CMakeLists.txt`
with `add_executable` / `target_link_libraries` / `worldline_apply_warnings` /
`add_test`. Run everything with `ctest --test-dir build --output-on-failure`.

## Verifying UI changes headlessly

The app runs under a virtual X display, which makes screenshots and scripted
input possible without a monitor (Linux):

```bash
export DISPLAY=:99 WORLDLINE_DATA_DIR=/tmp/wl-ui
mkdir -p $WORLDLINE_DATA_DIR
printf 'last_seed=aurora-prime\nlast_screen=Cosmos\nwindow_width=1600\nwindow_height=900\n' \
  > $WORLDLINE_DATA_DIR/settings.txt
Xvfb :99 -screen 0 1600x900x24 & sleep 1.5
./build/worldline & sleep 7
xdotool keydown Tab; sleep 0.15; xdotool keyup Tab     # hold keys across a frame
scrot shot.png
```

`last_screen` accepts `SeedWorkspace`, `UniverseAtlas`, `ReferenceLab`,
`Trace` and `Cosmos` (anything else opens the main menu), and
`WORLDLINE_DATA_DIR` keeps the run away from your real saved universes.
raylib polls input once per frame, so a key or mouse button pressed *and*
released within one frame is never seen: use `keydown`/`keyup` and
`mousedown`/`mouseup` with a short sleep rather than `xdotool key`/`click`
(mouse-wheel `click 4`/`click 5` is fine).
