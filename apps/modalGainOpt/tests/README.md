# modalGainOpt unit tests

Catch2 unit tests for the `modalGainOpt` MagAO-X application (`apps/modalGainOpt/modalGainOpt.hpp`).

## Test files

| File | Covers |
|------|--------|
| `modalGainOpt_test.cpp` | Configuration, the `modalPsdProcessor` PSD processing, published gain arrays, SI/PC gain integration, and predictor publication |

## What is tested

- **Configuration**: PSD-processing settings loaded via `setupConfig()`/`loadConfig()`, and restoring the current selection when the extrapolation mode switches.
- **modalPsdProcessor**: power-law fit fallbacks, noise estimation (closed-loop space, low-frequency end, minimum in range, max-frequency limit), NTF-aware OL PSD reconstruction, power-law-only estimation and Moffat handoff, automatic power-law crossover selection and capping, smoothed-PSD anchoring, and repair of dropouts (deep, raw, and trailing high-frequency runs).
- **Gain publication**: LP and max-LP outputs stay distinct, calibration scaling is applied, and `zero_gains` resets the integrated SI gains.
- **Gain updates**: the SI gain integrator, enabled-mode counts, and gain-factor, multiplier, and frequency updates (unchanged when the frame is not ready, resizing and resetting state otherwise), and refreshing pending gopt structures without a PSD wakeup.
- **Predictor**: coefficient layout, blending, clearing of stale coefficient blocks, SI/PC mode-count updates depending on predictor control, and PC gain-factor and multiplier updates.

## Test harness

- `processPsdProcessorHarness` exposes the protected `modalPsdProcessor` methods through `using` declarations.
- `modalGainOptHarness` subclass of `modalGainOpt` with accessors and setters for the protected state and config loading.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../modalGainOpt.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/modalGainOpt/tests/modalGainOpt_test
../apps/modalGainOpt/tests/modalGainOpt_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The first test case only checks that the harness constructs the app (a placeholder).
- Shared-memory stream I/O and the worker threads are not exercised.
