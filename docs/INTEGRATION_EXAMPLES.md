# Integration examples

`trackpoint-scroll-core` is intended to sit inside larger input/scrolling systems. The examples in this document are deliberately quarantined from the engine API: they illustrate adapter patterns, not dependencies or preferred hosts.

## General adapter shape

A typical integration needs to provide four things around the core:

```text
host input source
    -> timestamped relative motion
    -> tpsc_engine_feed()

host scheduler
    -> logical tick cadence from tpsc_engine_tick_us()
    -> tpsc_engine_tick()

optional host-owned transform
    -> tpsc_transform apply/reset callbacks

host scrolling system
    <- emitted two-dimensional scroll deltas
```

Gesture recognition, device discovery, button/key routing, timer implementation, configuration storage, and output-event injection normally remain outside this library. A larger scrolling implementation may also add direction locking, sequence lifecycle, units/scaling policy, or other modules around the engine without changing reconstruction internals.

## Example: integrating with an existing input stack

An input stack that already has a button-scroll gesture can use the core only for the pieces it needs: feed the pre-scroll relative displacement and timestamps into the engine, schedule logical ticks, and translate emitted vectors back into the stack's continuous-scroll representation. Existing gesture routing, axis-locking, stop-event handling, and device policy can remain owned by that stack.

For example, a libinput-based adapter can keep libinput's device classification, button-scroll state machine, timer/event plumbing, keyboard policy, and locked-axis path while using the core for startup coalescing, sparse-report reconstruction, and portable memoryless transforms. A host-specific stateful accelerator can be supplied through the transform callback instead of being copied into this repository.

This example does **not** make libinput types, lifecycle rules, configuration formats, or event APIs part of the core contract. Other integrations may choose a different subset of facilities.

## Dependency direction

Integration examples must preserve this direction:

```text
host/integration -> trackpoint-scroll-core
```

Never change the public engine API solely to expose a host-specific object or convention. If an integration reveals a genuinely general abstraction, describe and test that abstraction in host-neutral terms before adding it to the stable core.

## Partial use is expected

Future versions of this repository may grow additional scrolling modules. Integrators should be able to consume only the modules they need. New facilities should therefore have explicit interfaces and should not reach through opaque state or make unrelated modules mandatory.

## Optional terminal-rebound adapter

The terminal-rebound classifier is consumed beside the reconstruction engine,
not through it. A host that wants this facility typically adds:

```text
relative pointer report
    -> forward immediately
    -> tpsc_rebound_filter_feed()

pending candidate
    -> schedule tpsc_rebound_filter_deadline_us()

quiet deadline
    -> host-specific "did another pointer move?" safety check
    -> tpsc_rebound_filter_finish()
    -> apply nonzero exact-undo correction through the host pointer path
```

The host should reset the classifier at semantic boundaries where retrospective
pointer movement would be unsafe, such as device removal or drag/button mode
changes. Timer APIs, global-pointer queries, and correction injection remain
host-owned.

See `REBOUND.md` for the motivation, classifier state model, default thresholds,
new-platform adaptation guidance, pseudocode, and non-goals.
