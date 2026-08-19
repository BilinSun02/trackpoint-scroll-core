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
- source scan confirming the repository contains no integration-specific platform names prohibited by the repository scope.

The available execution environment did not contain Meson, so the checked-in `meson.build` was not executed during the initial extraction. This is not a Meson validation claim. Contributors with Meson installed should run:

```bash
meson setup builddir
meson test -C builddir --print-errorlogs
```

## Current regression coverage

The initial tests exercise:

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
- radial direction preservation;
- exact-zero handling;
- negative-output reversal and clamping;
- calibrated affine, quadratic, and hyperbolic defaults.

Future work should add long-running ring-wrap tests and deterministic replay fixtures from captured timestamped input traces.
