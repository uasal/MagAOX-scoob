# cameraSim unit tests

Catch2 unit tests for the `cameraSim` MagAO-X application (`apps/cameraSim/cameraSim.hpp`).

## Test files

| File | Covers |
|------|--------|
| `cameraSim_test.cpp` | `dev::stdCamera` INDI callback validation for cameraSim, and the stdCamera focus-state and goto-focus helpers |

## What is tested

- **INDI callbacks**: `newCallBack_stdCamera()` rejects a wrong device or property name and accepts every stdCamera property that cameraSim enables (reconfigure, temperature, readout and vertical shift speed, EM gain, exposure time, FPS, synchro, crop mode, ROI controls, shutter, and goto_focus).
- **Focus-state helper**: `setCallBack_focusMonitored()` caches the monitored switch property and the published `focus.state` follows the configured element, for both polarities of `focus.stateElementOnMeansInFocus`.
- **Goto-focus helper**: `sendGotoFocusCommand()` formats the preset name from the cached source switches (`{}-{}-{}`), sends it to the target property, and propagates send failures.
- **Configuration**: the `focus.gotoFocus` keys (`numSwitches`, `property1..3`, `format`, `targetProperty`) are loaded by `dev::stdCamera::loadConfig()`, and the wrapping quotes are stripped from the format string.

## Test harness

- `cameraSim_test` subclass of `cameraSim`, which sets the stdCamera property device/names and enables `m_hasFocus`, for the INDI validation tests.
- `focusHelper_test`: a minimal `MagAOXApp<>` + `dev::stdCamera` app with only the focus features enabled. It overrides `sendNewProperty()` to capture the command sent by the goto-focus helper and to return a configurable result. Only one app object is alive at a time.
- `tests/testMacrosINDI.hpp` is included before the app header, so the INDI callbacks run in validation mode and return right after the device/name checks.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../cameraSim.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/cameraSim/tests/cameraSim_test
../apps/cameraSim/tests/cameraSim_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The simulated frame generation (`configureAcquisition()`, `acquireAndCheckValid()`, `loadImageIntoStream()`), `appStartup()`, and `appLogic()` are not tested.
- The callback bodies are not exercised (validation mode only).
