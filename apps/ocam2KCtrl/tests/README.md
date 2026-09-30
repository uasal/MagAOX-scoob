# ocam2KCtrl unit tests

Catch2 unit tests for the `ocam2KCtrl` MagAO-X application (`apps/ocam2KCtrl/ocam2KCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `ocam2KCtrl_test.cpp` | Sync stream, state strings, configuration, serial getters/setters, temperature control, framegrabber hooks, INDI callbacks, telemetry, `appLogic()`, and reconfig |
| `ocam2KCtrl_lifecycle_test.cpp` | `appStartup()` failures and the POWERON path of `appLogic()`, isolated in their own executable |
| `ocamUtils_test.cpp` | Pure temperature and gain response parsers in `ocamUtils.hpp` |

## What is tested

- **Sync stream**: creation (1x1 uint8), publication mirroring metadata and posting semaphores, and reuse/replacement in `ensureSyncStream()`.
- **State strings**: `stateString()` and `stateStringValid()`.
- **Configuration**: defaults and supported gain limits.
- **Serial helpers**: `getTemps()`, `getFPS()`, gain helpers, temperature control, `setShutter()`, and the serial setter command sequences, with valid and malformed responses.
- **Framegrabber**: `startAcquisition()`, `acquireAndCheckValid()` (valid, skipped, and corrupt frame numbers), `loadImageIntoStream()` with the OCAM descramble output, and `configureAcquisition()` with valid and invalid modes.
- **INDI and telemetry**: callbacks update local state, static wrappers forward to the instance, and telemetry wrappers record snapshots and stale intervals.
- **appLogic and lifecycle**: connection and housekeeping flow, startup failures, POWERON logic, and reconfig through `edtCamera`.
- **ocamUtils**: parsing of temperature and gain responses.

## Test harness

- `ocam2KCtrl_test.cpp` includes the app header with `#define protected public`, and defines `ocam2KCtrl_test` (an `ocam2KCtrl` subclass), an `edtStubState` with scripted serial responses (`serialResponse`) for the EDT stubs in `tests/edtinc.h`, an `ocam2StubState` for the OCAM2 SDK functions (`fli/ocam2_sdk.h`), a `tempStream` helper for temporary ImageStreamIO streams, and the `fgThreadScope`/`startupScope` RAII helpers.
- `ocam2KCtrl_lifecycle_test.cpp` includes `ocam2KCtrl_test.cpp` with `OCAM2KCTRL_TEST_SUPPORT_ONLY` defined, which keeps only the harness and stubs, so the lifecycle cases run in a separate process.
- ocam2KCtrl is an EDT framegrabber camera. `tests/Makefile.one` lists `ocam2KCtrl_test` and `ocam2KCtrl_lifecycle_test` in `EDT_TESTS`, so they are built without `-DMAGAOX_NOEDT` and compile against the EDT SDK stub header `tests/edtinc.h` (found through `-I tests/`). The utility test does not need EDT.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../ocam2KCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/ocam2KCtrl/tests/ocam2KCtrl_test
make -f Makefile.one t=../apps/ocam2KCtrl/tests/ocam2KCtrl_lifecycle_test
make -f Makefile.one t=../apps/ocam2KCtrl/tests/ocamUtils_test
../apps/ocam2KCtrl/tests/ocam2KCtrl_test
../apps/ocam2KCtrl/tests/ocam2KCtrl_lifecycle_test
../apps/ocam2KCtrl/tests/ocamUtils_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No real camera or EDT framegrabber is used: serial traffic, EDT calls, and the OCAM2 SDK are stubbed.
