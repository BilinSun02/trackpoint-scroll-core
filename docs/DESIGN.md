# Design invariants and lessons

## Hard invariants

1. **Regularize time before nonlinear mapping.** Raw packet duration must not implicitly change the transfer function.
2. **Causal assignment only.** Once a reconstructed share has been assigned and emitted, later reports do not rewrite it.
3. **No prediction debt.** The engine has no mutable predicted target, correction reservoir, or retrospective repayment.
4. **Finite support.** Every sustained raw report has a finite reconstruction tail.
5. **Prompt explicit stop.** Ending a gesture discards the remaining tail rather than manufacturing post-release motion.
6. **Two-dimensional vector treatment.** Sustained memoryless profiles operate on vector magnitude and restore direction; avoid independent per-axis nonlinear thresholds unless a feature explicitly requires componentwise behavior.
7. **Startup is intentionally componentwise.** The fixed micro-step is the exception to radial treatment because predictability per axis is its purpose.
8. **Zero is exact.** A zero input vector maps to exact zero even when a memoryless profile has a positive one-sided onset.
9. **Profile-independent startup.** Startup fixed-step distance must not depend on the selected sustained transform.
10. **Reset stateful transforms at burst/gesture boundaries.** No stale velocity history crosses an idle rearm or explicit reset.
11. **Explicit time.** Algorithms depend on timestamps and logical periods, not assumptions about callback punctuality.
12. **Reset policy is explicit.** Higher-level state changes that discard reconstruction must choose whether startup is rearmed; the engine does not infer this from an opaque gesture policy.
13. **Contain non-finite profile results.** Built-in memoryless profiles map a non-finite input magnitude or non-finite scalar result to zero rather than allowing NaN/Inf to escape the transform boundary.

## Current tested defaults

```text
tick_us                         2000
initial_interval_ms             240.0
interval_min_ms                  10.0
interval_max_ms                 250.0
idle_reset_ms                   333.3
first_step_distance               0.4
first_step_axis_merge_ms         70.0
first_step_max_reports             -1
```

Memoryless profile defaults:

```text
affine:     y = 0.4*x
quadratic:  y = 0.16*(x - 0)^2 + 0.025
hyperbolic: y = 0.75*sqrt(1.6^2 + x^2) - 1.175
```

At input magnitude `x=2.5`, these produce approximately 1.000, 1.025, and 1.051 respectively.

The affine intercept is intentionally zero: a positive affine offset was too eager under the lightest force. The small positive onset in the nonlinear profiles remains useful for making tiny sustained samples visible.

## Numerical containment

The engine normally receives finite relative-motion values, but the built-in profile boundary is defensive: if vector magnitude is non-finite, or a configured formula produces a non-finite scalar magnitude, the output is exactly zero. This avoids propagating malformed/extreme numerical values into a host scrolling system and preserves the containment behavior expected by integrations that previously performed this check around their profile code.

## Failed approaches worth remembering

These are design lessons, not a chronology.

### Long low-pass smoothing

Large attack/release constants made motion look smoother but produced buildup, lag, overshoot, and momentum-like stopping. Responsiveness matters more than hiding every sparse-input artifact.

### Mutable trajectory prediction

Predicting future position and later reconciling it created reverse corrections and large jumps when new evidence revised an already emitted trajectory. Do not conserve a target that can change retrospectively.

### Position repayment / cumulative telescoping

Telescoping against a mutable predicted position is mathematically conservative only for a fixed trajectory. Once the trajectory changes, the correction itself becomes user-visible motion.

### Treating hand jitter as the primary problem

The strongest evidence identified sparse, irregular packet timing as the main low-force issue. Do not add hand-jitter filters, reversal gates, or recalibration heuristics without a controlled residual failure that requires them.

### Applying the transfer function per raw packet

This makes equivalent motion depend on packetization. Redistribution and nonlinear mapping must remain in the current order.

## Startup rebound is a scoped exception

The current startup rebound rule is evidence-driven and intentionally narrow: an immediate sign reversal on an already represented axis inside the merge window is ignored. Outside that window, reversal is ordinary input and must remain possible.

Do not generalize this into sustained-motion reverse gating without new evidence.
