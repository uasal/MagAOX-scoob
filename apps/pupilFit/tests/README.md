# pupilFit unit tests

Catch2 unit tests for the `pupilFit` MagAO-X application (`apps/pupilFit/pupilFit.hpp`) and its
pupil fitting class (`apps/pupilFit/pupilFitter.hpp`).

## Test files

| File | Covers |
|------|--------|
| `pupilFit_test.cpp` | App configuration, reference and main stream `allocate()`, set point selection, frameGrabber interface, INDI callbacks |
| `pupilFitter_test.cpp` | `pupilFitter` buffers, quadrant geometry, thresholding, and fits to synthetic 4- and 3-pupil images |

## What is tested

- **Configuration**: defaults (`fit.*`, `cal.*`, stream names) and overrides via `setupConfig()`/`loadConfigImpl()`,
  including the set point source chosen from the reference stream name.
- **pupilFitter**: `setSize()` allocation, `quadCoords()` for both layouts, `threshold()`, `getQuad()`/`putQuad()`
  round trips and size checks, and `fit()` on synthetic images (centers, radii, background/median levels, the
  threshold and edge images, response to pupil shifts and size changes, 3-pupil geometry).
- **Reference stream**: `allocate(refShmimT)` rejects non-float streams; `processImage(refShmimT)` copies the
  image and requests a restart of the main stream.
- **Main stream allocation**: `allocate(shmimT)` configures the fitter and picks default, user, or
  reference-image-fit set points (falling back to defaults for a mismatched reference), and creates the
  threshold/edge output streams.
- **frameGrabber interface**: `configureAcquisition()`, `startAcquisition()`, `reconfig()`, `loadImageIntoStream()`
  and `acquireAndCheckValid()` (updated, not-updated and timeout paths).
- **INDI callbacks** (live bodies): `threshold`, `averaging`, `setpt_reload`, `setpt_current` and `setpt_mode`,
  including rejection of wrong names/devices and missing elements.

## Test harness

- `pupilFit_test` subclass exposes protected members with using-declarations and small accessors for the
  `shmimMonitor`/`frameGrabber` base state; it initializes the frame-grabber semaphore and builds the INDI
  properties the callbacks use (as `appStartup()` would) without registering them or starting any threads.
- The main-stream `allocate()` test creates real ImageStreamIO output streams under a private
  `MILK_SHM_DIR` (`/tmp/pupilFit_test_shm`), which the app destructor removes; the directory is deleted afterwards.
- `testMacrosINDI.hpp` is not used: the app callbacks only check the property name (not the device), so the
  standard `XWCTEST_INDI_NEW_CALLBACK` validation does not apply.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../pupilFit.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/pupilFit/tests/pupilFit_test
../apps/pupilFit/tests/pupilFit_test
make -f Makefile.one t=../apps/pupilFit/tests/pupilFitter_test
../apps/pupilFit/tests/pupilFitter_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `processImage(shmimT)` is not tested: it calls `m_indiDriver->sendSetProperty()` unconditionally, which requires
  a live INDI driver. The running-average accumulation it performs is therefore not covered.
- `appStartup()`, `appLogic()` and `appShutdown()` are not called because they start the shmimMonitor,
  frameGrabber and telemetry threads. Telemetry (`recordTelem()`) is not covered.
- One `acquireAndCheckValid()` case waits for its 1 second semaphore timeout.
