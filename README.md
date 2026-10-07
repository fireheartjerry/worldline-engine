# Worldline Universe Studio

Worldline is a desktop simulation instrument built with C++ and raylib.

Its flagship experience is a deterministic seeded universe pipeline:

- enter a seed string
- generate a `MetaSpec` (a vault of tensors: metric, potential, coupling,
  gyroscopic, warp …) and assemble a `LawSpec` equation of motion
- advect a swarm of ~13,000 test masses through the **actual generated law**
  every frame
- paint their world-lines as a luminous **phase-flow field** — vortices, spiral
  arms, shear sheets, saddles and accretion rings emerge directly from the seed

Each universe receives a signature colour palette derived from its own
invariants, plus a live exotic-index / flux / swirl / order readout for
at-a-glance comparison. The same live state can also be viewed through the
reference pendulum renderer (press `V` in the Seed Workspace).

The Newtonian double pendulum is still included as a polished **reference
system** for comparison.

## Product Structure

- `Guided First Universe`
  A seed-first onboarding flow that explains the deterministic pipeline and opens directly into the live workspace.
- `Seed Workspace`
  The main live simulation view: the phase-flow field stage (or the pendulum
  view), a contextual inspector with the universe's signature metrics, a
  timeline with scrubbing and pinned frames, notes, saving, and direct access to
  trace tools.
- `Universe Atlas`
  A local gallery of saved universes with search, compare, derived descriptors, and visual fingerprints.
- `Cosmos Explorer`
  A zoomable map of a procedurally generated universe — descend from the cosmic
  web into a galaxy, a star system, a planet, a living ecosystem and a single
  creature — with a per-level scientific instrument deck, plus a nine-tier
  N-body sandbox with per-tier physics derived from the law genome.
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

- deterministic seeded universe generation with genuinely varied laws
  (anisotropic metrics, saddle/well/ridge potentials, gyroscopic vortices, warp)
- live `FieldRenderer`: multithreaded RK2 advection of ~13k test masses through
  the generated law, additive glow trails with multi-tap bloom, a static
  streamline atlas, a per-universe starfield and a highlighted "hero" world-line
- per-universe signature colour palette + live exotic-index / flux / swirl /
  order metrics
- live `LawSpec` stepping with observable extraction every frame, viewable
  through the reference pendulum renderer as an alternative stage
- saved local universe projects with title, notes, markers, and thumbnails
- atlas search over seed text, descriptors, and derived metrics
- timeline recording with scrubbing and pinned frames
- glossary-backed terminology for advanced concepts
- persistent local settings and recent project recovery
- reference Newtonian pendulum workspace for comparison
- multi-scale Cosmos Explorer with a scale ladder, object catalog, and live
  N-body sandbox with per-tier physics derived from the law genome
- a 124-object catalogue spanning nine tiers, from Planck-scale quanta and the
  full particle zoo through the periodic table, organics, planets, exotic stars
  and the complete black-hole family (intermediate / supermassive / primordial)
  plus a wormhole, up to galaxies and the cosmic web and its voids
- real SI anchors that drive computed physics in the inspector — density,
  Schwarzschild radius, escape velocity, and compactness derived from `G` and `c`
- a state-of-the-art quantum & Planck-scale physics suite for the smallest tier —
  the "first layer of existence" — every module header-only, pure, deterministic
  and cited (CODATA 2022 / PDG 2024), each backed by a verification test:
  - `cosmos/QuantumScale.hpp` — Planck units re-derived from `hbar`/`c`/`G`,
    Compton / de Broglie / thermal wavelengths, the Compton–Schwarzschild crossover,
    Heisenberg bounds, the quantum harmonic oscillator, the Bohr/hydrogen ladder
    (13.6 eV ground state, Lyman/Balmer lines), and decay width ↔ lifetime
  - `cosmos/PlanckScale.hpp` — the full Planck unit system (charge, force, power,
    density, …) plus black-hole thermodynamics (Hawking T, Bekenstein–Hawking
    entropy, Page evaporation), the holographic / Bekenstein bounds, and a GUP
    minimal length
  - `cosmos/StandardModel.hpp` — electroweak relations (weak mixing angle, the
    Higgs VEV / Yukawa / self-coupling), conserved quantum numbers with the
    Gell-Mann–Nishijima charge check, the CKM matrix, and one-loop running of the
    gauge couplings (asymptotic freedom, α_s(M_Z) ≈ 0.118)
  - `cosmos/Hadronization.hpp` — builds colour-singlet hadrons from quark content
    and reads off their charge, baryon number and strangeness (the QCD spectrum)
  - `cosmos/QuantumVacuum.hpp` — Casimir pressure, the Schwinger critical field,
    the Unruh temperature, and the cosmological-constant problem
  - `cosmos/QuantumStatistics.hpp` — Fermi-Dirac / Bose-Einstein / Maxwell-Boltzmann
    occupation, WKB tunnelling, particle-in-a-box levels, the Gamow factor, and
    degeneracy pressure
  - `cosmos/SpinEntanglement.hpp` — the non-classical core: Pauli algebra,
    single-qubit gates with unitarity checks, the Bloch sphere, two-qubit
    entanglement (Bell states, partial trace, von Neumann entropy, Wootters
    concurrence), and the CHSH/Bell inequality reaching the Tsirelson bound 2√2
  - `cosmos/QEDScattering.hpp` — the classical electron radius and Thomson limit,
    Klein–Nishina Compton scattering and the wavelength shift, Rutherford/Mott
    Coulomb scattering, Mandelstam `s+t+u`, and the Breit–Wigner resonance
  - `cosmos/LatticeQCD.hpp` — confinement: the Cornell static-quark potential
    (Coulomb + linear), the string tension and its Wilson-loop area law, string
    breaking, Regge trajectories, and a cold-lattice plaquette / Wilson action
  - `cosmos/NeutrinoOscillation.hpp` — flavour oscillation probabilities, the
    PMNS mixing angles and mass splittings, oscillation lengths, row unitarity,
    and the MSW matter resonance
  - `cosmos/QuantumGenesis.hpp` — the **generation step**: synthesizes a universe's
    entire particle-physics content from its law genome (effective couplings, the
    n–p mass split, proton / deuteron / di-proton stability, the periodic-table
    cutoff, primordial He/H, the hadron spectrum and the early-universe epoch
    timeline) and returns an anthropic verdict on whether complex matter can form
    — surfaced live in the inspector alongside per-object rest energy, Compton
    wavelength and size in Planck lengths
- an ultra-advanced nuclear-physics suite for the second tier (protons, neutrons,
  nuclei) — same header-only, pure, deterministic, cited (PDG / textbook) idiom,
  each module test-backed:
  - `cosmos/NuclearData.hpp` — nuclear radius/density, the extended SEMF (liquid
    drop + pairing + Wigner), binding & separation energies, mass excess, the
    valley of beta stability, and the neutron/proton drip lines
  - `cosmos/NuclearShell.hpp` — the shell model: magic numbers (2,8,20,28,50,82,126),
    the spin-orbit level ordering, ground-state spin-parity, doubly-magic nuclei,
    and the pairing gap
  - `cosmos/NuclearDecay.hpp` — the decay law and every mode: alpha (Gamow +
    Geiger–Nuttall), beta∓/EC (Q-values, Sargent's Q⁵ rule), gamma (Weisskopf
    single-particle rates), and decay-mode prediction from the energetics
  - `cosmos/DecayChains.hpp` — the Bateman solution, secular/transient
    equilibrium, the four natural series (4n … 4n+3), and α/β step counts
  - `cosmos/NuclearReactions.hpp` — reaction/fusion Q-values, the Coulomb barrier,
    the Gamow peak & astrophysical S-factor, and fission (fissility Z²/A, barrier,
    ~200 MeV release)
  - `cosmos/StellarBurning.hpp` — the pp-chain and CNO cycle, triple-alpha, and
    the ordered advanced burning stages (C, Ne, O, Si) up to the iron peak
  - `cosmos/Nucleosynthesis.hpp` — the nuclear **generation step**: from the law
    genome it forges the iron peak, the s-/r-process abundance peaks pinned to the
    neutron magic numbers, the fission limit that caps the periodic table, the
    primordial He/H split, the cosmic abundance pattern and the synthesis sites,
    ending in an anthropic verdict — surfaced live in the navigator's inspector as
    a "NUCLEAR FORGE" readout on the nuclear tier
  - `cosmos/NuclearStructure.hpp` — collective structure: quadrupole deformation,
    rotational bands and moments of inertia, vibrational phonons, the rotor/vibrator
    R₄/₂ signature, and the giant dipole resonance + TRK sum rule
  - `cosmos/BetaDecayTheory.hpp` — the Fermi theory of beta decay: the Q⁵ phase
    space, ft / log ft classification, Fermi vs Gamow–Teller selection rules, the
    Fermi Coulomb function, the Kurie plot, and double beta decay
  - `cosmos/FissionPhysics.hpp` — asymmetric fragment mass distribution, prompt /
    delayed neutrons, the ~200 MeV energy partition, the fission barrier, and
    reactor criticality (four/six-factor formulas, reactivity)
  - `cosmos/NuclearMoments.hpp` — the nuclear magneton, the Schmidt single-particle
    magnetic moments, free-nucleon g-factors, quadrupole moments, and Larmor
    precession (the basis of NMR/MRI)
  - `cosmos/NuclearMatter.hpp` — bulk nuclear matter and neutron stars: the
    saturation point, incompressibility, symmetry energy, the equation of state and
    its pressure, beta-equilibrium neutronisation, and the neutron-star mass-radius
    end-points
- a comprehensive atomic-physics suite for the third tier (atoms) — same
  header-only, pure, deterministic, cited idiom, each module test-backed:
  - `cosmos/AtomicStructure.hpp` — hydrogenic energy levels and Z² scaling, the
    quantum numbers and orbital degeneracies, orbital radii and electron
    velocities, the Rydberg formula, and the fine-structure scale
  - `cosmos/PeriodicTable.hpp` — Aufbau/Madelung electron configurations, valence
    counting, period/block assignment, noble gases, and the measured periodic
    trends (ionization energy, atomic radius, electronegativity)
  - `cosmos/AtomicSpectra.hpp` — the hydrogen spectral series (Lyman/Balmer/…),
    term symbols, dipole selection rules, the Zeeman effect and Landé g-factor,
    line broadening, and the Wien/Planck blackbody law
  - `cosmos/Ionization.hpp` — photoionization thresholds, the Saha ionization
    equilibrium (the bridge to stellar atmospheres), and plasma collective
    behaviour (Debye length, plasma frequency)
  - `cosmos/AtomicGenesis.hpp` — the atomic **generation step**: from the law
    genome it sets the periodic-table extent (the relativistic Z≈1/α bound), the
    Rydberg/Bohr energy and size scales, and whether the CHNOPS elements of life
    can exist — surfaced live in the navigator's inspector as an "ATOMIC ASSEMBLY"
    readout on the atomic tier
  - `cosmos/MultiElectronAtoms.hpp` — Slater screening / effective nuclear charge,
    Hund's rules for ground-state terms, term multiplicity, and the Aufbau
    exceptions (Cr, Cu, …)
  - `cosmos/LightMatter.hpp` — the Einstein A/B coefficients, the photoelectric
    effect, Rabi flopping, Beer–Lambert absorption, and the laser population-
    inversion / gain condition
  - `cosmos/FineStructure.hpp` — Dirac fine structure (spin-orbit + relativistic +
    Darwin), the 21 cm hyperfine line, the QED Lamb shift, and the
    gross ≫ fine ≫ {Lamb, hyperfine} hierarchy
  - `cosmos/ExoticAtoms.hpp` — reduced-mass scaling, positronium, muonic hydrogen,
    and the Rydberg-atom n-power scaling laws (radius ∼ n², lifetime ∼ n³,
    polarizability ∼ n⁷)
  - `cosmos/AtomicCollisions.hpp` — geometric and Coulomb cross sections, mean free
    path and collision frequency, Bethe stopping power, electron-impact
    thresholds, and radiative recombination
- a deterministic, lazily-generated, LRU-cached procedural universe you can
  descend into by zooming — galaxy → star system → planet → ecosystem → creature
  — with bounded work and memory (same place always regenerates identically)
- aim-true descent navigation: the child under your cursor is the child you
  enter (with an on-stage "entering" highlight before the hand-off), zoom-through
  transitions instead of hard cuts, clamped panning with a recenter key, and the
  full keyboard shell documented on screen
- research-grounded generation: stars follow the real stellar IMF and HR
  relations, planet habitability is gated by the Kopparapu habitable zone and
  star lifetime (so most worlds are barren), and living worlds grow Whittaker
  biomes with NPP-scaled, trophically-balanced food webs
- universe classification with granular signature metrics and an observer fleet
- 3D navigation boilerplate for future free-flight exploration (orbit-camera
  math, tested; not yet wired to a renderer)
- cohesive glassmorphic, sci-fi HUD interface shared across every screen
- offline tools: `worldline_field_preview` renders universe "postcards" to BMP
  without a GPU; `worldline_metaspec_stats` profiles the generated-law corpus

## Keyboard Reference

Escape goes back (or closes the open modal) on every screen.

**Seed Workspace** — `Space` pause/resume · `R` restart · `C` clear trail ·
`V` switch the stage between the phase-flow field and the pendulum view ·
`Tab` open Trace · `Enter` (in the seed box) generate

**Cosmos Explorer — map** — scroll to zoom (zooming past a level enters the
object under the cursor; zooming out ascends) · drag or `W A S D` to pan ·
`+`/`-` zoom · `C` recenter · arrows move the selection · `Tab`/`Shift+Tab`
cycle · `1`–`9` select · `Enter` enter · `H` jump to the next notable object
(a habitable world, an ecosystem's keystone species) · `[` `]` previous/next
sibling · `Backspace` up · `Home` root · `G` show/hide the analysis deck

**Cosmos Explorer — live ecosystem** — `Space` pause · `,` `.` slower/faster ·
`O` single step · `X` perturb the selected species and watch it recover

## Build

Worldline builds on Windows, Linux, and macOS. CI builds and runs the full test
suite on Windows (MSVC, the reference platform) and Linux (GCC); macOS is
supported on a best-effort basis.

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

- `src/seed` deterministic generation pipeline (seed text → bytes → lanes → `MetaSpec`)
- `src/physics` the generated equation of motion (`LawSpec`), observable
  extraction, and the reference pendulum physics
- `src/cosmos` the law genome, the procedural universe (`ProcUniverse`), the
  N-body sandbox, ecology, and the header-only quantum / nuclear / atomic suites
- `src/app` app state, the seeded-universe runtime, persistence, and UI copy
- `src/renderer` `FieldRenderer` (live flow-field backend) + `FlowOperator`
  (shared float force operator) + the reference pendulum renderer and bloom
- `src/ui` desktop UI scenes and the shared glass drawing primitives
- `src/math` double-precision vectors and integrators
- `tools` offline field preview and corpus diagnostics
- `tests` verification programs (one executable per suite)

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for how the pieces fit
together, the determinism contract, and how to verify UI changes headlessly.

## Field preview tool

Render universe "postcards" to a BMP without opening a window:

```bash
./build/worldline_field_preview --size 460 andromeda vortex-sigma saddle-rift helix-nine
```

## Tooling

- `.clang-format` for consistent formatting
- `CMakePresets.json` for repeatable local configure flows
- GitHub Actions CI: build + full test suite on Windows (MSVC) and Linux (GCC)

## License

MIT
