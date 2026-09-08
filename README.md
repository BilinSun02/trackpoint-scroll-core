# trackpoint-scroll-core

A small, platform-neutral C library for turning sparse TrackPoint relative-motion reports into deterministic continuous scroll deltas.

The library deliberately concentrates on reusable motion processing rather than device discovery, event interception, keyboard policy, or scroll-event delivery. Its current pipeline is:

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

The repository also exposes an optional, separate terminal-rebound classifier in `trackpoint_scroll/rebound.h`. It never suppresses live reversal and is not enabled by the reconstruction engine; consumers opt in explicitly and decide whether/where to apply a retrospective exact-undo correction.

## Quick start

```bash
make test
```

The public headers are under `include/trackpoint_scroll/`.

## Start here

- `docs/README.md` — documentation map.
- `docs/ARCHITECTURE.md` — layering, stable engine boundary, restart semantics, and extension guidance.
- `docs/DESIGN.md` — hard invariants, tested defaults, and design classes that should not be revived casually.
- `docs/INTEGRATION_EXAMPLES.md` — generic adapter shape and deliberately isolated host examples.
- `docs/VALIDATION.md` — required checks and current regression coverage.
- `docs/REBOUND.md` — optional terminal-rebound classifier semantics and host responsibilities.
- `docs/PROJECT_RULES.md` — repository-as-source-of-truth, compatibility, and integration-reference discipline.
- `CONTRIBUTING.md` — code and contribution conventions.

## Repository scope

This repository owns reusable scrolling mechanics and their invariants. Host-specific discovery, interception, button/key routing, scheduler APIs, and scroll-event injection normally live in adapters or larger scrolling systems.

Host-specific systems may appear in clearly quarantined documentation examples when useful. Those examples do not change dependency direction: core source, public APIs, configuration structures, and build dependencies remain host-neutral.

The architecture is intentionally layered so additional scrolling facilities can be added beside or above the current engine without coupling them into its stable reconstruction path, and consumers can use only the modules they need. Read `docs/ARCHITECTURE.md` before extending the library.

## Documentation rule

Lasting design decisions, defaults, invariants, caveats, and contribution conventions discussed elsewhere must be reflected in this repository so it remains understandable on its own. Porting handoff notes may remain external; enduring technical decisions discovered during such work belong here when they affect this code.
