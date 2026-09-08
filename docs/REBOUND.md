# Terminal rebound classifier

`trackpoint_scroll/rebound.h` provides an optional, platform-neutral classifier
for a specific TrackPoint failure mode: after otherwise established motion, the
device can finish with a short, small displacement in the opposite direction.
If forwarded literally, that terminal reversal makes the pointer visibly step
back after the user has stopped applying force.

This module exists to classify that narrow residual failure without turning
ordinary reversal into filtered or delayed input.

## Motivation

The motivating integration observed a pattern like:

```text
... +3, +3, +2, -1, -1, quiet
```

where the small opposite-sign tail occurred only at the end of a motion episode.
The important distinction is **terminality**. A sequence such as:

```text
... +3, +3, -2, -2, -3, ...
```

is an intentional live reversal and must remain responsive.

That distinction rules out a simple sign-change filter. Suppressing or delaying
every reversal would make normal pointer control feel sticky and would violate
the core project's responsiveness goals. Instead, the classifier lets live
motion pass through unchanged and decides only after enough quiet has elapsed
whether the final, already-emitted reversal had the small-and-short shape of a
rebound.

If it did, the module returns an **exact inverse** of the candidate displacement.
For example:

```text
terminal candidate:  (-2, 0)
returned correction: (+2, 0)
```

The module deliberately does not add overdrive, edge pressure, acceleration, or
any other extra displacement. Those are separate host/output concerns.

## Relationship to startup rebound handling

The reconstruction engine already has a narrow startup rule that ignores an
immediate sign reversal on an axis represented inside the first-step merge
window. That solves a different problem:

- **startup rebound** happens inside the fixed first-step/coalescing window and
  is handled synchronously before sustained reconstruction;
- **terminal rebound** happens after established live pointer motion and can
  only be distinguished from intentional reversal retrospectively, after quiet.

Do not merge these mechanisms into one generalized reversal gate. Their timing,
purpose, and safety properties are different.

## Model

Classification is independent per axis.

For each axis:

1. recent same-direction motion establishes a direction and cumulative count;
2. an opposite-sign displacement becomes a candidate only after enough
   established motion and within the configured arm window;
3. while the candidate continues in the reversed direction, it remains eligible
   only while it is both short and small;
4. exceeding either the duration or displacement threshold immediately commits
   the reversal as intentional live motion;
5. reversing again before terminal quiet also cancels the candidate;
6. if the candidate remains both short and small and the stream becomes quiet,
   `tpsc_rebound_filter_finish()` returns the exact inverse of that candidate.

Live reports are never buffered, delayed, suppressed, or rewritten by the core
classifier. A host normally forwards them immediately and may later apply the
returned correction.

## Why the module is separate from the reconstruction engine

The terminal classifier is useful outside scrolling reconstruction. The
motivating host applies it to ordinary TrackPoint pointer motion, while the
existing engine is concerned with turning sparse relative motion into continuous
scroll output.

Keeping the classifier separate preserves several boundaries:

- no dependency on gesture begin/end semantics from the scroll engine;
- no dependency on a particular output system or cursor representation;
- no platform timer or event-loop objects in reusable state;
- consumers that do not need terminal rebound handling pay no behavioral cost;
- the reconstruction engine keeps its existing live-reversal semantics.

The module uses `struct tpsc_vec` and the shared status codes for consistency,
but does not otherwise require an engine instance.

## Host responsibilities

The classifier intentionally knows nothing about a cursor, compositor, event
queue, other pointing devices, or scheduler. A host must provide the policy
around it.

A host should:

- feed every relevant **live relative-motion report** with a monotonic
  microsecond timestamp;
- forward that live report normally rather than waiting for classification;
- when `tpsc_rebound_filter_pending()` becomes true, obtain
  `tpsc_rebound_filter_deadline_us()` and arrange a one-shot quiet check;
- cancel/reschedule that check as later reports change the candidate/deadline;
- at the deadline, perform any host-specific safety checks that should prevent a
  retrospective move;
- call `tpsc_rebound_filter_finish()` and, if it returns a nonzero correction,
  inject/apply that correction through the host's normal relative-motion path;
- call `tpsc_rebound_filter_reset()` at hard input boundaries where an old
  candidate must not leak into a new interaction.

Useful reset boundaries commonly include device removal, mode changes, button
or drag transitions whose semantics must never be rewritten retrospectively,
and any discontinuity in the host's coordinate/input state.

### External-pointer safety

Retrospective correction can be surprising if another mouse, trackpad, tablet,
remote-input source, or accessibility tool moved the pointer after the
TrackPoint's last report.

The core cannot detect that portably. A pointer-owning host should therefore
consider recording its last realized TrackPoint position and, immediately
before applying the correction, verify that the current pointer is still at that
position within a small host-appropriate tolerance. If not, reset the classifier
and discard the correction.

A host without a global pointer concept may use a different safety criterion, or
may decide that no such check is necessary for its output model.

## Adapting to a new platform

A minimal integration needs four pieces:

```text
raw TrackPoint relative report
        |
        | 1. forward live motion immediately
        | 2. feed (timestamp_us, dx, dy)
        v
tpsc_rebound_filter
        |
        | pending? -> schedule deadline
        v
host one-shot timer
        |
        | deadline reached
        | optional external-motion safety check
        v
tpsc_rebound_filter_finish()
        |
        | nonzero correction
        v
host relative-motion injection/output
```

Pseudocode:

```c
on_trackpoint_motion(time_us, dx, dy)
{
    forward_pointer_motion(dx, dy);

    tpsc_rebound_filter_feed(filter, time_us,
                             (struct tpsc_vec){ dx, dy });

    if (tpsc_rebound_filter_pending(filter))
        schedule_one_shot(tpsc_rebound_filter_deadline_us(filter));
    else
        cancel_one_shot();
}

on_rebound_deadline(time_us)
{
    struct tpsc_vec correction;

    if (another_pointer_moved_since_last_trackpoint()) {
        tpsc_rebound_filter_reset(filter);
        return;
    }

    tpsc_rebound_filter_finish(filter, time_us, &correction);
    if (correction.x != 0.0 || correction.y != 0.0)
        forward_pointer_motion(correction.x, correction.y);
}
```

The exact host APIs are intentionally unspecified:

- on macOS, an adapter can use a run-loop timer, query the current global cursor
  for the external-pointer safety check, and inject the correction through its
  existing pointer-event path;
- a Linux compositor/input stack might use its own monotonic event timestamps,
  event-loop timer source, seat/pointer state, and relative-motion dispatch;
- an embedded or test environment can call `finish()` directly when advancing
  simulated time.

The portable requirement is only that timestamps are monotonic and that the
host does not present the returned correction as if it were new user intent.

## Scheduling notes

`tpsc_rebound_filter_deadline_us()` is authoritative. The quiet interval is
adaptive: recent sparse-report gaps influence the deadline, bounded by configured
minimum and maximum quiet times.

The core deliberately does not invoke callbacks. This keeps deterministic tests
simple and avoids choosing among platform-specific timer models. A host may use
one timer per device/filter, reuse an existing event-loop timer, or poll the
deadline if that is natural for the platform.

Calling `tpsc_rebound_filter_finish()` before the deadline is safe: it returns a
zero correction and leaves pending state unchanged.

## Configuration and defaults

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

These values describe the classifier, not a recommendation that every consumer
enable it. The facility is opt-in by construction: consumers that never create
or feed a `tpsc_rebound_filter` retain all existing behavior.

A host exposing this as a user-facing option should generally default it off
until the failure mode has been observed on that device/integration and the
host's retrospective-correction safety behavior has been validated.

## Non-goals and rejected extensions

The terminal rebound module is **not**:

- a general hand-jitter filter;
- a live reversal gate;
- a low-pass smoother;
- a prediction/correction reservoir;
- a pointer acceleration profile;
- a screen-edge or Dock-pressure mechanism;
- a reason to alter the reconstruction engine's sustained-motion semantics.

The motivating macOS work separately discovered that auto-hidden Dock reveal
requires HID-class edge pressure. Synthetic overdrive added to rebound
correction did not solve that problem and is intentionally absent from this
module. Terminal rebound classification should remain concerned only with
recognizing and exactly undoing the qualifying terminal reversal.

## Validation expectations

Portable tests should cover at least:

- a small, short terminal reversal producing exact undo after quiet;
- no correction before the quiet deadline;
- a large reversal being committed as intentional immediately;
- a long reversal being committed as intentional;
- a second reversal cancelling the terminal candidate;
- independent per-axis classification;
- reset behavior;
- nondecreasing timestamp enforcement;
- adaptive quiet/deadline behavior when report cadence changes.

Host integrations should additionally test their timer cancellation,
external-pointer safety check, correction injection path, and reset boundaries.
