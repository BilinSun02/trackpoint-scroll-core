# Versioning

The core library uses semantic-style project versions independently of any one integration. Version 0.1.0 is the first repository-backed shared-core line.

An integration may pin any exact core commit through its own dependency mechanism; the exact commit remains the reproducibility authority even when a human-readable version is also recorded.

Version changes that alter public API, defaults, reconstruction invariants, or extension semantics must be documented in-repository together with corresponding tests.


## 0.2.0

Adds the optional platform-neutral terminal-rebound classifier API in
`trackpoint_scroll/rebound.h`. Existing reconstruction-engine behavior and
defaults are unchanged; consumers must opt into the new module explicitly.
