# cred2Ctrl unit tests

Catch2 unit tests for the `cred2Ctrl` MagAO-X application (`apps/cred2Ctrl/cred2Ctrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `cred2Ctrl_test.cpp` | Configuration, serial response helpers, getters/setters, ROI sync, framegrabber hooks, telemetry, and `appLogic()` |
| `cred2Ctrl_lifecycle_test.cpp` | `appStartup()`/`appLogic()` lifecycle entry points, isolated in their own executable |
| `cred2Utils_test.cpp` | Pure parsing and formatting helpers in `cred2Utils.hpp` |

## What is tested

- **Configuration**: `loadConfig()` writes the runtime EDT configuration file and loads the camera settings.
- **Serial helpers**: response cleaning, acknowledgement checks, preset name normalization, and command helpers.
- **Getters**: temperatures, FPS, exposure limits, and the discrete getters, including fallback and malformed-response parsing paths.
- **Setters**: bounds validation and state updates for temperature, FPS, exposure time, and other camera settings.
- **ROI**: `syncROIFromCamera()` tracks the camera cropping state, and the framegrabber hooks (`configureAcquisition()`, `startAcquisition()`, ...) manage acquisition and ROI configuration.
- **Telemetry and power**: telemetry wrappers and power-off helpers update the cached state.
- **appLogic and lifecycle**: connection and housekeeping flow in `appLogic()`, and startup failures and success in `appStartup()`.
- **cred2Utils**: cleaning CLI responses, parsing float, float-vector, range, boolean, and crop responses, and formatting ROI commands.

## Test harness

- `cred2Ctrl_test.cpp` includes the app header with `#define protected public`, and defines `cred2Ctrl_test` (a `cred2Ctrl` subclass) plus an `edtStubState` with scripted serial responses (`serialResponse`) for the EDT stub functions declared in `tests/edtinc.h`. `fgThreadScope` and `startupScope` are RAII helpers that stop threads and clean up after lifecycle calls.
- `cred2Ctrl_lifecycle_test.cpp` includes `cred2Ctrl_test.cpp` with `CRED2CTRL_TEST_SUPPORT_ONLY` defined, which keeps only the harness and stubs, so the lifecycle cases run in a separate process from the other cases.
- cred2Ctrl is an EDT framegrabber camera. `tests/Makefile.one` lists `cred2Ctrl_test` and `cred2Ctrl_lifecycle_test` in `EDT_TESTS`, so they are built without `-DMAGAOX_NOEDT` and compile against the EDT SDK stub header `tests/edtinc.h` (found through `-I tests/`). The utility test does not need EDT.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../cred2Ctrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/cred2Ctrl/tests/cred2Ctrl_test
make -f Makefile.one t=../apps/cred2Ctrl/tests/cred2Ctrl_lifecycle_test
make -f Makefile.one t=../apps/cred2Ctrl/tests/cred2Utils_test
../apps/cred2Ctrl/tests/cred2Ctrl_test
../apps/cred2Ctrl/tests/cred2Ctrl_lifecycle_test
../apps/cred2Ctrl/tests/cred2Utils_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No real camera or EDT framegrabber is used: serial traffic and EDT calls are scripted by the stubs, so the tests check the command sequences and response handling, not camera behavior.
