# psfFit unit tests

Catch2 unit tests for the `psfFit` MagAO-X application (`apps/psfFit/psfFit.hpp`).

## Test files

| File | Covers |
|------|--------|
| `psfFit_test.cpp` | Configuration, circular buffer sizing, PSF fitting and frame quality checks on synthetic Gaussian images, dark/reference handling, the frameGrabber interface, and live INDI callback bodies |
| `psfFit_indi_test.cpp` | Device/name validation of every INDI NEW and SET callback (`XWCTEST_INDI_*` macros) |

## What is tested

- **Configuration**: defaults and overrides for all `fitter.*` options (fps and shutter sources, thresholds,
  `defaultFPS`) and the `shmimMonitor`, `darkShmim`, `refShmim` and `framegrabber` stream names.
- **Buffer sizing**: `allocate(shmimT)` sets the fit circular buffers to `statsTime*fps + 1`, capped at the maximum
  length, with a minimum of 3, and disabled for zero fps or zero stats time.
- **Fitting**: `processImage(shmimT)` on a sampled Gaussian recovers the center of light; before statistics exist
  frames only fill the buffers and report zero (or the reference position when a 2x1 reference is loaded); good
  frames report the fit; a second frame before the framegrabber posts is counted as skipped-updating.
- **Quality checks**: closed shutter, max pixel far from the center of light, max too high or too low relative to
  the running statistics, and x or y jumps too large, each with its skip counter.
- **Dark and data types**: non-float darks are rejected, float darks are subtracted, and uint16 frames are converted.
- **frameGrabber interface**: `configureAcquisition()` (2x1 float), `startAcquisition()`, `reconfig()`, `fps()`,
  `acquireAndCheckValid()` (signaled with and without an update) and `loadImageIntoStream()` (offsets applied only to
  good frames).
- **INDI**: `reset`, `statsTime`, the four thresholds, `dx`, `dy`, and the fps source and shutter SET callbacks
  (including fps tolerance and shutter-change restarts), plus device/name validation for all of them.

## Test harness

- `psfFit_test` subclass (in `psfFit_test.cpp`) exposes the protected members, initializes the frame semaphore,
  sets the stream geometry the shmimMonitors would normally set, and calls `allocate()`/`processImage()` directly on
  in-memory frames, so no ImageStreamIO stream is created. Callback bodies are live.
- `psfFit_test` subclass (in `psfFit_indi_test.cpp`) sets up the INDI properties for the validation macros;
  `testMacrosINDI.hpp` is included before the app header so the callback bodies return right after validation.

## Known limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called: they start (or join) shmimMonitor, frameGrabber
  and telemeter threads. The running-statistics computation in `appLogic()` is therefore not covered; the tests set
  the statistics directly.
- `recordTelem()`/`checkRecordTimes()` are not tested (they write telemetry).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../psfFit.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/psfFit/tests/psfFit_test
../apps/psfFit/tests/psfFit_test
make -f Makefile.one t=../apps/psfFit/tests/psfFit_indi_test
../apps/psfFit/tests/psfFit_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).
