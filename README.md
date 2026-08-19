# trackpoint-scroll-core

A small, platform-neutral C library for turning sparse TrackPoint relative-motion reports into deterministic continuous scroll deltas.

The library deliberately concentrates on motion processing rather than device discovery, event interception, keyboard policy, or scroll-event delivery. Its current pipeline is:

```text
timestamped relative-motion reports
  -> profile-independent startup fixed step
  -> short split-axis/rebound window
  -> causal fixed-time reconstruction
  -> configurable transform
  -> scroll deltas
```

The default configuration reflects the currently tested behavior:

```text
logical tick:                 2 ms
initial interval estimate:  240 ms
interval clamp:          10..250 ms
interval history:              5 samples, median
idle rearm:                 333.3 ms
first-step distance:           0.4
first-step merge window:      70.0 ms
first-step report limit: unlimited (-1)
```

Built-in memoryless radial profiles are affine, quadratic, and hyperbolic. Stateful or experimental transforms can be supplied through the transform callback without changing the reconstruction engine.

## Quick start

```bash
make test
```

The public headers are under `include/trackpoint_scroll/`.

## Start here

- `docs/ARCHITECTURE.md` — layering, stable engine boundary, restart semantics, and extension guidance.
- `docs/DESIGN.md` — hard invariants, tested defaults, and design classes that should not be revived casually.
- `docs/VALIDATION.md` — required checks and current regression coverage.
- `docs/PROJECT_RULES.md` — repository-as-source-of-truth and compatibility discipline.
- `CONTRIBUTING.md` — code and contribution conventions.

## Repository scope

This repository owns reusable scrolling mechanics and their invariants. It does not own host-specific device classification, input interception, button/key routing, scheduler APIs, or scroll-event injection.

The architecture is intentionally layered so additional scrolling facilities can be added beside or above the current engine without coupling them into its stable reconstruction path. Read `docs/ARCHITECTURE.md` before extending the library.

## Documentation rule

Lasting design decisions, defaults, invariants, caveats, and contribution conventions discussed elsewhere must be reflected in this repository so it remains understandable on its own. Porting handoff notes may remain external; enduring technical decisions discovered during such work belong here when they affect this code.
