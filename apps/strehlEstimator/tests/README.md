# strehlEstimator unit tests

Catch2 unit tests for the `strehlEstimator` MagAO-X application (`apps/strehlEstimator/strehlEstimator.hpp`).

## Test files

| File | Covers |
|------|--------|
| `strehlEstimator_test.cpp` | INDI callback validation, published planning properties, estimate handling, and the loop-speed optimisation |

## What is tested

- **INDI**: `XWCTEST_INDI_SET_CALLBACK` checks for the camera (`fps`, `emgain`), stage (`presetName`) and TCS (`seeing`, `telpos`) properties, and `XWCTEST_INDI_ARBNEW_CALLBACK` checks for `star_mag`, `seeing`, `wind_speed` and `use_estimates`.
- **Published properties**: the planning-input and `loop_speed_optimum` properties are created with the expected elements.
- **Planning inputs**: `current` writes to star magnitude and seeing are ignored while `estimated` writes are accepted; live TCS seeing updates the current value; the wind-speed selector sets the planning wind speed.
- **Estimates**: auto-tracked estimates freeze while `use_estimates` is on, and the model uses the estimated values when requested.
- **Optimisation**: the reported optimum loop speed and Strehl match a brute-force scan of the fixed FPS grid.

## Test harness

- `strehlEstimator_test` subclass seeds the callback property keys, initialises the published INDI properties without starting the shmim-monitor threads, and exposes the live/estimated values and published properties.
- `testMacrosINDI.hpp` is included after the app header, so callback bodies are live and their effects can be checked.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../strehlEstimator.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/strehlEstimator/tests/strehlEstimator_test
../apps/strehlEstimator/tests/strehlEstimator_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The shmim monitors and a running INDI server are not exercised.
