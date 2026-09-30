# adcTracker unit tests

Catch2 unit tests for the `adcTracker` MagAO-X application (`apps/adcTracker/adcTracker.hpp`).

## Test files

| File | Covers |
|------|--------|
| `adcTracker_test.cpp` | Configuration, INDI callbacks, lookup-table startup checks, `appLogic()` ADC target generation, and callback wrappers |

## What is tested

- **Configuration**: `setupConfig()`/`loadConfig()` keep the defaults for a defaults-only file and apply overrides (lookup file, ADC stage and TCS device names, zeros, lookup-table signs, deltas, deltaAngle, minimum zenith distance, update interval, and the initial tracking state).
- **INDI callbacks**: device/name validation of the tracking, deltaAngle, deltaADC1, deltaADC2, minZD, and teldata callbacks, and the effect of valid payloads on the runtime state (tracking flag, offsets, minimum ZD, zenith distance).
- **Telescope data**: the teldata callback handles standard and non-standard exceptions, and non-finite values, thrown/returned while extracting the zenith distance.
- **Startup**: `appStartup()` validates the ADC lookup table (missing, too few rows, malformed rows, non-monotonic zenith distances) and handles interpolator setup exceptions before running.
- **appLogic**: ADC1/ADC2 targets are sent only when tracking is enabled and zenith-distance data are fresh, with zeros, signs, offsets, the minimum ZD, and the update interval applied, and send failures and interpolation exceptions are handled.
- **Helpers**: direct entry points (`recordTelem()`, `sendADC1Position()`, `sendADC2Position()`) and the static callback wrappers forward to the instance.

## Test harness

- `adcTracker_test` subclass of `adcTracker` with a per-test scratch directory under `/tmp/adcTracker_test/`, where ADC lookup tables are written. It exposes protected state through setters/getters, captures the ADC positions that would be sent, and overrides the virtual seams (`sendADC1Position()`, `sendADC2Position()`, `setupInterpolators()`, `interpolateADC1()`, `interpolateADC2()`, `extractZD()`) to inject send failures, standard and non-standard exceptions, and non-finite values.
- `tests/testMacrosINDI.hpp` is included after the app header, so the callback bodies stay live and the tests check what the callbacks do.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../adcTracker.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/adcTracker/tests/adcTracker_test
../apps/adcTracker/tests/adcTracker_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The non-finite injection returns `std::numeric_limits<float>::infinity()`, which relies on `std::isfinite()`. The tests are compiled with `-ffast-math`, where `std::isfinite()` may be optimized to always return true, so that case can be unreliable.
- Commands are captured rather than sent to real ADC stage devices, since no INDI server is running.
