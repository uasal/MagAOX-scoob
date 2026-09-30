# timeSeriesSimulator unit tests

Catch2 unit tests for the `timeSeriesSimulator` MagAO-X application (`apps/timeSeriesSimulator/timeSeriesSimulator.hpp`).

## Test files

| File | Covers |
|------|--------|
| `timeSeriesSimulator_test.cpp` | Configuration, `appStartup()` property setup, waveform math, gizmo motion, `appLogic()`, and INDI callback validation |

## What is tested

- **Configuration**: the `startup_delay` default and override via `setupConfig()`/`loadConfig()`, and the constructor
  defaults (200 ms loop pause, square wave, amplitude, period, gizmo count and time to target).
- **Startup**: `appStartup()` creates the `function` switches (square on), `function_out`, `duty_cycle` and the
  `gizmo_0000`/`gizmo_0001` properties, registers their callbacks, and sets `READY`.
- **Waveforms**: `updateSimsensor()` for the sine, cosine, square and constant functions, sampled at points of zero
  slope (peaks, troughs, mid-level of the square wave) so the result does not depend on test timing.
- **Interpolation**: `lerp()` at the end points, mid points, and beyond the end point.
- **Gizmo motion**: `requestGizmoTarget()` creating and updating motion requests, and `updateGizmos()` interpolating
  a gizmo in motion and completing a move once the time to target has elapsed.
- **appLogic**: updates the sensor output and the gizmos.
- **INDI callbacks**: `newCallBack_function()` and `newCallBack_duty_cycle()` device/name validation with
  `XWCTEST_INDI_ARBNEW_CALLBACK`; `newCallBack_gizmos()` ignoring properties without a `target` and unknown names.

## Test harness

- `timeSeriesSimulator_test` subclass sets the device name and the `function`/`duty_cycle` property device and name,
  exposes the protected members with `using` declarations, wraps the configuration steps, and can run
  `updateSimsensor()` at a chosen elapsed time by moving the start time back.
- `tests/testMacrosINDI.hpp` is included before the app header, so the `function` and `duty_cycle` callbacks return
  0 right after a successful validation.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../timeSeriesSimulator.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/timeSeriesSimulator/tests/timeSeriesSimulator_test
../apps/timeSeriesSimulator/tests/timeSeriesSimulator_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The bodies of the `function` and `duty_cycle` callbacks, and the `target` path of the gizmo callback, are not run:
  they call `m_indiDriver->sendSetProperty()` without a null check, and there is no INDI driver in the unit tests.
- `appStartup()` is only run with the default `startup_delay` of 0, since a non-zero delay sleeps.
