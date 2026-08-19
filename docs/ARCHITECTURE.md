# Architecture

## Layering

The repository is organized around a narrow deterministic engine with optional transforms.

```text
host input adapter
    |
    | timestamp + relative displacement
    v
engine
    |- startup fixed-step policy
    |- startup coalescing / rebound filtering
    |- interval estimator
    |- causal fixed-time reconstruction
    v
transform callback
    |- built-in memoryless profiles
    |- optional stateful/custom transform
    v
host output adapter
```

The host adapters are intentionally outside this repository.

## Stable engine boundary

The engine accepts:

- gesture begin/end boundaries;
- in-gesture reconstruction restarts with explicit startup policy;
- monotonic timestamps in microseconds;
- two-dimensional relative displacement reports;
- logical tick calls.

The engine produces one two-dimensional delta per logical tick. It does not decide how that delta is presented to an application or compositor.

The public engine type is opaque. This is deliberate: internal reconstruction structures can evolve without forcing callers to embed or mirror them.

`tpsc_engine_restart()` is the coordination boundary for a higher-level policy change that must discard pending reconstruction without necessarily ending the surrounding gesture. Callers choose whether the next report rearms the fixed startup branch or bypasses startup and enters sustained reconstruction directly. The latter measures its first interval from the restart timestamp.

## Scheduling model

`tick_us` defines a **logical sampling period**, not a promise that the host scheduler wakes with perfect periodicity. Callers should treat timestamps as authoritative and keep the logical tick sequence deterministic. Future APIs may add catch-up/batching helpers; such additions should preserve the semantics of the existing one-tick interface.

The tested baseline uses 2 ms because the input device can approach roughly 10 ms hardware-report intervals at higher force. A finer logical grid avoids tying nonlinear mapping to raw packet cadence.

## Startup branch

The startup branch exists to make deliberate micro-scrolls predictable when raw low-force reports are quantized and occasionally split across axes.

For each represented axis, only sign matters:

```text
positive -> +first_step_distance
zero     -> 0
negative -> -first_step_distance
```

Raw magnitude is ignored. Thus `(0,1)` and `(0,2)` have the same startup result, and `(1,1)` gives one fixed component on each axis rather than a radial irrational fraction.

The merge window begins at the first raw report, not at gesture begin and not as a rolling deadline.

A previously unseen axis may join the fixed step during the window. A same-sign repeat may consume report quota but adds no second fixed component. An opposite-sign component on a represented axis is treated as startup rebound and ignored until the merge window ends.

`first_step_max_reports` conventions are part of the API contract:

```text
N > 0   at most N accepted reports participate
N = 1   only the first report gets fixed-step treatment
N = 0   fixed-step behavior is disabled
N < 0   no report-count limit; only the time window limits participation
```

Consumed startup reports are not replayed later. Their timestamps still matter for the first subsequent interval estimate.

## Sustained reconstruction

After startup, each raw displacement is partitioned causally over a finite number of logical ticks.

For estimated interval `T` and logical period `h`:

```text
N = round(T / h)
share = raw_delta / N
```

The share is activated on the next tick and removed after `N` ticks. Overlapping reports add linearly.

Before the transform, the sum of all shares for one report equals that raw report exactly except when the gesture is explicitly ended before the finite tail completes.

No emitted share is ever revised.

## Interval estimator

The tested estimator is the median of up to five recent raw-report intervals. Each measured interval is clamped before entering the history. Until a usable interval exists, the configured initial estimate is used.

This intentionally favors robustness over elaborate prediction.

## Transform boundary

Reconstruction happens **before** nonlinear mapping. This is a hard architectural rule because nonlinear mapping and temporal redistribution do not commute.

The transform callback receives a uniform-grid reconstructed vector. It may be memoryless or stateful. A stateful transform should supply a reset callback so idle/burst and gesture resets do not leak history across independent motion episodes.

The fixed startup step bypasses the transform by design.

## Build as a subproject

The Meson build always exposes `core_dep` for consumers. Standalone regression-test executables are defined only when this repository is the top-level Meson project; embedding the core as a subproject therefore does not add unrelated test targets to the containing build.

This is a dependency-boundary rule, not a special case for any one host. Consumers should depend on `core_dep` and should not reach into the core's private source list or internal structs.

## Extending toward a larger scrolling stack

Add new capabilities as separate modules with explicit interfaces. Examples include direction locking, gesture policy, sequence lifecycle, momentum, application-facing units, or richer configuration. Avoid folding those responsibilities into the reconstruction engine unless they change reconstruction itself.

When adding a higher-level facility:

1. depend on the public engine/profile APIs rather than internal structs;
2. keep host event types out of the reusable modules;
3. express time explicitly;
4. add deterministic trace tests;
5. preserve existing semantics unless an API version or opt-in mode clearly states otherwise.

Do not make the current engine a miscellaneous scrolling state container. New layers should compose with it.
