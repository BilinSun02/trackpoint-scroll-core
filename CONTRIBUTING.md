# Contributing

The project welcomes extensions, including work that grows it beyond the current reconstruction engine into a broader scrolling stack.

## Before changing code

Read:

- `docs/ARCHITECTURE.md`
- `docs/DESIGN.md`
- `docs/PROJECT_RULES.md`

## Structural guidance

Keep responsibilities separated. The existing engine is intentionally narrow; new facilities should usually be new modules layered around it. Avoid exposing internal reconstruction state merely because a new feature needs coordination. Prefer explicit inputs, outputs, callbacks, or a higher-level coordinator.

Public API changes should be deliberate. Additive APIs are preferred when existing callers can continue using the current engine unchanged.

## Code conventions

- C11.
- Prefix exported symbols with `tpsc_`.
- Keep helpers file-local unless they are genuine reusable API.
- Define symbols before use and choose names that make units explicit (`*_us`, `*_ms`).
- Use monotonic integer timestamps at API boundaries.
- Avoid hidden global mutable state.
- Validate nonfinite and structurally impossible configuration values.

## Tests

Run:

```bash
make test
```

New behavior should come with focused deterministic tests. When fixing a bug, add a test that fails before the fix whenever practical.

## Documentation

If a change follows from an important design discussion, update the relevant documentation in the same change. The repository must remain self-contained for future contributors who do not have access to prior conversations.
