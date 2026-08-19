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
