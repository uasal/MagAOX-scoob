# w2tcsOffloader unit tests

Catch2 unit tests for the `w2tcsOffloader` MagAO-X application (`apps/w2tcsOffloader/w2tcsOffloader.hpp`).

## Test files

| File | Covers |
|------|--------|
| `w2tcsOffloader_test.cpp` | Configuration, `appStartup()` file handling and mode-count clamping, the Zernike projection in `processImage()`, and the coefficient telemetry |

## What is tested

- **Configuration**: `offload.wZModesPath`, `offload.wMaskPath`, `offload.nModes`, `shmimMonitor.shmimName` and `telemeter.maxInterval` defaults and overrides, through both `loadConfig()` and `loadConfigImpl()`.
- **Startup**: `appStartup()` fails on a missing mode cube or mask; the coefficient vectors are sized to the cube, `nModes` is clamped to the number of planes (and kept when smaller), and a cube with more than 100 modes is a critical error that sets the shutdown flag.
- **Projection**: `processImage()` copies the frame, computes `sum(woofer * mode * mask) / norm` for each mode below `nModes`, zeroes the rest (including after `nModes` is reduced), and mirrors the values into the `zCoeffs` INDI property with state `Ok`.
- **Telemetry**: `recordZCoeffs()` records only on change unless forced, resizes the last-recorded vector when needed, and updates it; `recordTelem()` forces a record; `checkRecordTimes()` records once the max interval has elapsed and not again immediately after.
- **Other**: `allocate()` sizes the woofer buffer to the stream; `appShutdown()` succeeds with no monitor thread.

## Test harness

- `w2tcsOffloader_test` subclass exposes the protected members with `using` declarations, and `setupProjection()` builds a 2x2, three-mode basis, a mask and the `zCoeffs` INDI property the way `appStartup()` would.
- Small FITS mode cubes are written to `/tmp/w2tcsOffloader_test_modes.fits` for the startup tests and removed afterwards.
- Telemetry records are detected through `logger::telem_w2tcsoffloader::lastRecord`, which `telemeter::telem()` updates on every record. The telemetry log level is left at INFO (and reset after `loadConfig()`), so the records themselves are dropped and no file is written.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../w2tcsOffloader.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/w2tcsOffloader/tests/w2tcsOffloader_test
../apps/w2tcsOffloader/tests/w2tcsOffloader_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The successful end of `appStartup()` and `appLogic()` are not tested: they start the telemetry log thread and the shmimMonitor thread, and `appLogic()` calls `pthread_tryjoin_np()` on a thread that is never started in a unit test.
- The app has no INDI new-property callbacks, so there are no callback tests.
