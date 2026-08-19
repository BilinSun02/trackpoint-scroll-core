# Validation

## Required local checks

The core is intended to be testable without any host integration. Run:

```bash
make clean
make test
```

The Makefile compiles with:

```text
-std=c11 -O2 -Wall -Wextra -Werror -pedantic
```

For algorithm changes, also run sanitizer builds when the toolchain supports them. A representative command is:

```bash
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -Iinclude \
  src/engine.c src/profiles.c tests/test_engine.c -lm -o test-engine-san
```

Run an equivalent sanitizer build for `tests/test_profiles.c`.

## Initial extraction status

At the initial repository extraction, the following checks passed:

- strict C11 Makefile build;
- engine regression suite;
- memoryless-profile regression suite;
- AddressSanitizer/UndefinedBehaviorSanitizer engine run;
- AddressSanitizer/UndefinedBehaviorSanitizer profile run;
- source scan confirming the reusable implementation kept its dependency boundary clean.

The available execution environment did not contain Meson, so the checked-in `meson.build` was not executed during the initial extraction. This is not a Meson validation claim. Contributors with Meson installed should run:

```bash
meson setup builddir
meson test -C builddir --print-errorlogs
```

The Meson file is also designed for subproject consumption: standalone test executables are omitted when `meson.is_subproject()` is true, while `core_dep` remains available to the containing build.

## Current regression coverage

The tests exercise:

- componentwise startup fixed steps;
- raw startup magnitude erasure;
- split-axis coalescing;
- startup rebound suppression;
- `first_step_max_reports=0` direct sustained path;
- `first_step_max_reports=1` quota behavior;
- negative/unlimited report-limit behavior;
- idle rearming;
- median interval estimation;
- overlapping causal contributions and pre-transform displacement conservation;
- explicit gesture-end tail cancellation;
- in-gesture restart with startup rearm and startup bypass;
- radial direction preservation;
- exact-zero handling;
- negative-output reversal and clamping;
- containment of non-finite input/profile results at the built-in profile boundary;
- calibrated affine, quadratic, and hyperbolic defaults.

## Extraction-equivalence check

During the first integration refactor, a standalone reference model of the pre-extraction startup/reconstruction behavior was run against the shared engine. Affine, quadratic, and hyperbolic traces matched across:

- componentwise startup and split-axis coalescing;
- the startup-to-sustained transition;
- overlapping sparse reports;
- idle rearming;
- finite reconstruction tails;
- in-gesture restart that deliberately bypasses startup.

This is an algorithm-equivalence check, not a substitute for building and testing any containing host integration.

Future work should add long-running ring-wrap tests and deterministic replay fixtures from captured timestamped input traces.
