# alignLoop unit tests

Catch2 unit tests for the `alignLoop` MagAO-X application (`apps/alignLoop/alignLoop.hpp`).

## Test files

| File | Covers |
|------|--------|
| `alignLoop_test.cpp` | Configuration, loop state transitions, the control math in `processImage()`, and the INDI callbacks |

## What is tested

- **Configuration**: the `ctrl.*` and `loop.*` keywords and `shmimMonitor.shmimName`, defaults and overrides (including comma-separated vectors and the `upstreamFollowClosed` flag).
- **Loop state**: `toggleLoop()` closes and opens the loop only on a transition.
- **Control math**: `processImage()` copies the measurements and residuals, skips the command computation until the controller currents are known, and computes `intMat * measurements` with the default 2x2 interaction matrix from `appStartup()`, both with the loop open and closed; `sendCommands()` with no controllers.
- **Controller callback**: `setCallBack_ctrl()` (and its static wrapper) caches the current value only for a matching device, property and element.
- **INDI callbacks**: `loop_gain` (target, current fallback, wrong name, wrong device, no value), `loop_state` (on, off, wrong name), and the upstream loop set callback (follow closed or not, always follow open, no toggle element, wrong name).
- **Other**: `allocate()` sizes the measurement buffer to the stream; `appShutdown()` succeeds with no monitor thread.

## Test harness

- `alignLoop_test` subclass exposes the protected members with `using` declarations, and `setupLoop()` configures a two-axis loop, the `loop_gain`/`loop_state` properties, the interaction matrix and a 2x1 stream the way `appStartup()` does, without starting the shmimMonitor thread.
- `testMacrosINDI.hpp` is not used: the callbacks check only the property name (plus the device through `indiTargetUpdate()` for `loop_gain`), not `INDI_VALIDATE_CALLBACK_PROPS`, so the standard validation macros do not apply. The callbacks are called directly with live bodies.
- With no INDI driver, `sendCommands()` logs a failed send for each controller but still returns 0, so the command vector sent is not observable; the tests check `m_commands` instead.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../alignLoop.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/alignLoop/tests/alignLoop_test
../apps/alignLoop/tests/alignLoop_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` and `appLogic()` are not tested: `appStartup()` starts the shmimMonitor thread first, and `appLogic()` calls `pthread_tryjoin_np()` on a thread that is never started in a unit test. The configuration consistency checks in `appStartup()` are therefore not covered.
