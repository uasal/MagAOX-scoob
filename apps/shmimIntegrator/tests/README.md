# shmimIntegrator unit tests

Catch2 unit tests for the `shmimIntegrator` MagAO-X application (`apps/shmimIntegrator/shmimIntegrator.hpp`).

## Test files

| File | Covers |
|------|--------|
| `shmimIntegrator_test.cpp` | Configuration, buffer allocation, dark loading, simple and moving averages with dark subtraction, triggered averaging, file saving and dark lookup, framegrabber hooks, INDI NEW and SET callbacks |

## What is tested

- **Configuration**: defaults and overrides of every `integrator.*` key and of the `shmimMonitor`, `darkShmim`,
  `dark2Shmim` and `framegrabber` stream names; the dark monitor's `getExistingFirst` default.
- **allocate()**: 1x1x1 accumulator for the simple average and an `nAverage`-deep zeroed cube for the moving average,
  counter reset, `m_reconfig`, time-based `nAverage = avgTime*fps` (clamped to 1, reverting to the default before an
  fps is known), bad data type, and dark validity from the dark geometry.
- **Dark streams**: `allocate()`/`processImage()` for both darks (uint16 conversion, float copy, mismatched geometry,
  bad data types).
- **Simple average**: block average of `nAverage` frames, semaphore post, ignoring frames until the framegrabber has
  copied the result, `loadImageIntoStream()` copy, restart of the next block; subtraction of no dark, either dark or
  both, and skipping a set-but-invalid dark.
- **Moving average**: burn-in, cube wrap-around, updates every `nUpdate` frames, first-dark subtraction, and skipping
  an update while the framegrabber is behind.
- **Triggered averaging**: frames ignored while stopped, a non-continuous average stopping itself, and the file saver
  refusing to save when the state string is invalid or changed during the acquisition.
- **File saver**: a triggered average is written as `<configName>_<state>__T<timestamp>.fits` in a temporary
  directory, then `findMatchingDark()` reloads it and posts it; no match and a size mismatch invalidate the image;
  a missing directory returns -1.
- **Framegrabber hooks**: `configureAcquisition()` geometry and the no-stream `-1`, `fps()` for the running,
  continuous and no-fps cases, `acquireAndCheckValid()` with and without an update, `startAcquisition()`/`reconfig()`.
- **INDI**: `nAverage`, `avgTime`, `nUpdate` NEW callbacks (wrong name/device, missing elements, target and current,
  the avgTime update for time-based averaging, shmimMonitor restart), the `start` toggle (off/on, missing toggle,
  state-string latching), and the fps-source and state-source SET callbacks.

## Test harness

- `MagAOX::app::shmimIntegrator_test` (the friend class declared by the app) initialises the frame semaphore (normally
  done in `appStartup()`), creates the INDI properties the callbacks validate against, and makes the protected members
  and hooks public with using-declarations; base-class stream geometry is set through small accessors.
- The INDI callbacks run with live bodies (`tests/testMacrosINDI.hpp` is not used, because the callbacks validate only
  the property name and the generic validation macros would not apply).
- No shared memory streams, INDI server or app threads are used. The file saver test writes FITS files to a
  `mkdtemp()` directory under `/tmp` that is removed at the end of each run.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../shmimIntegrator.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/shmimIntegrator/tests/shmimIntegrator_test
../apps/shmimIntegrator/tests/shmimIntegrator_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called, since they start the shmimMonitor, framegrabber
  and telemetry threads and (with the file saver) create a directory under the calibration path.
- The `findMatchingDark()` branch for a stream that is not yet connected (zero width and height) is not tested because
  it sleeps and changes the stream geometry.
- `recordTelem()`/`checkRecordTimes()` are not tested (they write telemetry).
- The "no input stream" case of `configureAcquisition()` waits about 1 second.
