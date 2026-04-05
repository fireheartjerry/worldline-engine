# Worldline Engine

This project is now a focused standard double-pendulum simulator built with raylib.

Features:
- Strong desktop UI for editing launch conditions before each run
- Drag-and-drop launch editing directly in the simulation stage
- Slider and numeric entry for arm lengths, bob masses, connector masses, launch angles, and launch angular velocities
- Per-bob linear and quadratic drag coefficients
- Per-connector directional drag coefficients with separate axial and normal response for rigid rods and taut ropes
- Pivot and elbow resistance controls for viscous damping, quadratic damping, and dry friction in rigid mode
- Connector-mode switching between rigid rods and massless ropes
- Optional rigid-connector mass so rods can be ideal massless links or uniform massive rods
- Pause, resume, stop, restart, and clear-trail controls at any time
- Dynamic screen normalization so different arm lengths stay framed cleanly
- Longer-lived after-image trails for visualizing chaotic motion in real time
- Adaptive time stepping for stiff, high-damping regimes
- Ambient-flow modeling with uniform wind, shear, and swirl driving drag through relative fluid velocity
- On-canvas vector overlays for velocity, gravity, bob drag, link drag, constraint resultants, net force, rigid-joint torque, and ambient-flow streamlines
- Overlay presets for clean presentation, force balance, resistance debugging, or full diagnostics
- A live force inspector that mirrors the stage labels with per-bob, per-link, torque, and local-flow magnitudes

Physics modes:
- `Rigid rods`: holonomic double-pendulum constraints with optional uniform rod mass
- `Massless ropes`: unilateral constraints that can go slack and catch again during motion
- `Distributed resistance`: drag assembled from generalized forces on bobs, rods, or taut rope segments, with axial/normal anisotropy on connectors
- `Ambient flow`: drag is computed from body velocity relative to a configurable fluid field instead of assuming still air
- `Joint resistance`: rigid-mode pivot and elbow damping/friction applied as generalized torques

Build:

```powershell
cmake -S . -B build
cmake --build build
```

Run:

```powershell
.\build\worldline.exe
```

