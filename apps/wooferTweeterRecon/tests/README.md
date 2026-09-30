# wooferTweeterRecon unit tests

Catch2 unit tests for the `wooferTweeterRecon` MagAO-X application (`apps/wooferTweeterRecon/wooferTweeterRecon.hpp`).

## Test files

| File | Covers |
|------|--------|
| `wooferTweeterRecon_test.cpp` | Configuration, buffer allocation, `processImage()` timestamps, the `recon()` interpolation and combination, the `fps` and `telpos` set callbacks, and `appShutdown()` |

## What is tested

- **Configuration**: `integrator.fpsSource`, `woofer.offset`, `tweeter.offset`, `wfs.offset` and the three shmimMonitor sections (`wooferModes`, `tweeterModes`, `wfsModes`), defaults and overrides, including the default stream names and `getExistingFirst` set by `loadConfigImpl()`.
- **Allocation**: the tweeter `allocate()` sizes the mode-value circular buffer, the r0/sigma buffers and the output matrix and restarts the other two monitors; the WFS `allocate()` waits for a tweeter with the same number of modes; the woofer `allocate()` waits for the WFS and accepts fewer modes than the WFS.
- **processImage**: each stream stores its mode values in its circular buffer (with wrap-around), with the timestamp `atime + offset` (woofer, tweeter) or `writetime - 1/fps + offset` (WFS).
- **Reconstruction**: `recon()` (called from the woofer `processImage()`) waits until both the woofer and tweeter values bracket the WFS time, linearly interpolates them, combines `0.04 * woofer + tweeter + wfs / opticalGain`, records r0 and the WFS sum of squares, marks the WFS value reconstructed, and does not reconstruct it twice.
- **Callbacks**: the `fps` source (wrong name rejected, missing `current` ignored, `1/fps` computed, 0 for non-positive fps) and the `telpos` elevation source.
- **Shutdown**: `appShutdown()` succeeds when no monitor threads were started.

## Test harness

- `wooferTweeterRecon_test` subclass exposes the protected members and callbacks with `using` declarations, and adds accessors for the width, restart flag, shmim name and `getExistingFirst` of each of the three shmimMonitor bases (their members have the same names, so they cannot all be exposed with `using`).
- Each shmimMonitor's `IMAGE` points at a local `IMAGE_METADATA` in the harness, so `processImage()` can read the timestamps without any shared memory. `feedWoofer()`, `feedTweeter()` and `feedWfs()` set the timestamps and call the matching `processImage()`.
- `testMacrosINDI.hpp` is not included, so the callback bodies run. The set callbacks check only the property name, so the `XWCTEST_INDI_SET_CALLBACK` macro (which expects a wrong device to be rejected) is not used.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../wooferTweeterRecon.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/wooferTweeterRecon/tests/wooferTweeterRecon_test
../apps/wooferTweeterRecon/tests/wooferTweeterRecon_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not tested: it starts the three shmimMonitor threads.
- `appLogic()` (the seeing estimate) is not tested: it calls `pthread_tryjoin_np()` on monitor threads that are never started in a unit test.
- The two allocation tests where the sizes do not match each take about a second, because `allocate()` sleeps for 1 s before returning.
