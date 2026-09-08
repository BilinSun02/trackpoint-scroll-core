# Terminal rebound classifier

`trackpoint_scroll/rebound.h` provides an optional, platform-neutral classifier
for a specific TrackPoint failure mode: a short, small reversal that appears at
the terminal end of otherwise established motion.

The module is deliberately separate from the scrolling reconstruction engine.
It consumes timestamped relative displacement and never suppresses, delays, or
rewrites live input.

## Model

For each axis independently:

1. recent same-direction motion establishes a direction and cumulative count;
2. a reversal becomes a candidate only after enough established motion and
   within the configured arm window;
3. the candidate is immediately accepted as intentional live input if either
   its duration or cumulative displacement exceeds the configured threshold;
4. if the candidate instead remains both short and small and motion becomes
   quiet, `tpsc_rebound_filter_finish()` returns the exact inverse of the
   candidate displacement.

The correction is **exact undo only**. The core does not add an overdrive or
outward impulse.

## Host responsibilities

The core classifier has no scheduler and no knowledge of a cursor, compositor,
or other pointing devices.

A host should:

- feed every relevant live relative-motion report with a monotonic timestamp;
- use `tpsc_rebound_filter_deadline_us()` to schedule a quiet check;
- before applying a retrospective correction, perform any host-specific safety
  checks needed to ensure another input source has not moved the pointer;
- call `tpsc_rebound_filter_reset()` at hard gesture/input boundaries.

The module is opt-in by construction: consumers that do not create/use a
`tpsc_rebound_filter` retain all existing behavior.

## Defaults

The defaults reflect the currently validated TrackPoint experiment:

```text
arm window:                  400 ms
minimum prior displacement:   6 counts
candidate maximum duration:  120 ms
candidate maximum distance:    6 counts
quiet default:               140 ms
quiet clamp:             100..700 ms
quiet/report-gap multiplier: 1.75
report-gap sample range:   3..500 ms
recent established cap:      4096 counts
```

These defaults are intentionally not part of the reconstruction engine's
default behavior.
