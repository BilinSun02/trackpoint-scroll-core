# Project rules

## Repository as source of truth

Important design decisions must not live only in conversations, issue comments, or private notes. When a discussion changes behavior, defaults, invariants, validation expectations, caveats, or contribution conventions, update the repository documentation in the same work or immediately afterward.

Porting handoff documents may remain external. Any lasting technical conclusion from that work that affects this repository belongs here.

## Compatibility discipline

- Prefer additive public APIs over changing existing semantics.
- Keep public structs small and configuration-oriented; keep mutable engine internals opaque.
- New higher-level scrolling facilities should compose around the engine instead of accreting unrelated state inside it.
- Avoid host-specific types, event constants, filesystem paths, or scheduler objects in public reusable APIs.
- New stateful transforms must define reset behavior.

## Host-specific references

Host-specific systems may be mentioned in clearly isolated documentation or examples when that helps explain how to consume the library. Such references are illustrative rather than dependencies or preferred hosts.

The architectural boundary remains strict:

- core source, public headers, configuration structures, and build dependencies must not acquire host-specific types or assumptions merely because an example integration needs them;
- dependency direction remains from an adapter or larger scrolling system toward this library;
- integration-specific gesture routing, event APIs, configuration storage, schedulers, and output injection stay outside reusable modules unless a genuinely general abstraction has first been identified;
- an integration that needs only part of this repository must be able to consume that part without pulling unrelated higher-level facilities into the stable reconstruction engine.

When integration work motivates a useful new facility, describe it in general scrolling terms, give it an explicit interface, and test it without requiring the motivating host. See `INTEGRATION_EXAMPLES.md` for the adapter pattern and quarantined examples.

## Validation discipline

For algorithm changes, tests should cover the relevant invariants directly. Useful categories include:

- exact pre-transform displacement conservation;
- overlapping contribution expiry;
- ring wraparound / long-running operation;
- startup magnitude erasure;
- split-axis coalescing;
- rebound-only reports;
- mixed rebound/new-axis reports;
- report-limit semantics for positive, zero, and negative values;
- idle rearming;
- deliberate reversal outside startup;
- two-dimensional direction preservation;
- zero-input exactness;
- deterministic replay of timestamped traces.

Do not treat subjective feel as a substitute for deterministic regression tests; both matter.
