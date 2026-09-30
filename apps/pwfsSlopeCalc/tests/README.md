# pwfsSlopeCalc unit tests

Catch2 unit tests for the `pwfsSlopeCalc` MagAO-X application (`apps/pwfsSlopeCalc/pwfsSlopeCalc.hpp`).

## Test files

| File | Covers |
|------|--------|
| `pwfsSlopeCalc_test.cpp` | Configuration, quadrant geometry, dark handling, semaphore frame handoff, 3- and 4-pupil slope math, fitter SET callbacks |

## What is tested

- **Configuration**: defaults (`pupil.numPupils`, `pupil.D`, `pupil.buffer`, `pupil.fitter`, shmim names derived from the
  config name, dark `getExistingFirst`) and overrides of every `pupil.*` key plus the `shmimMonitor`, `darkShmim` and
  `framegrabber` stream names.
- **Quadrant geometry**: `configureAcquisition()` quadrant size (`D + 2*buffer`), per-pupil quadrant start coordinates,
  output frame size (`quadSize x 2*quadSize`, float) and the "no input stream yet" `-1` return.
- **Dark handling**: main-image `allocate()` resizing/zeroing a mismatched dark and setting `m_reconfig`; dark
  `allocate()`/`processImage()` converting uint16 and float darks, and rejecting an unsupported data type.
- **Frame handoff**: `processImage()` storing the source pointer and posting the semaphore; `acquireAndCheckValid()`
  returning 0 for a posted frame (with timestamp) and 1 on the 1 s timeout.
- **Slope math**: `loadImageIntoStream()` on synthetic 8x8 uint16 frames with 4x4 quadrants: 4-pupil x/y slopes with and
  without dark subtraction, zero slopes for equal quadrants, pixel-to-slope mapping of a single bright pixel, and the
  3-pupil (`sqrt(3)/2 (I2-I3)`, `I1 - (I2+I3)/2`) formulas, all normalised by the mean total intensity.
- **INDI**: `setCallBack_m_indiP_quad1..4` name validation (return codes -1..-4) and updates of `set-x`, `set-y`,
  `set-D`, including `m_reconfig` only on change and ignoring missing elements.

## Test harness

- `pwfsSlopeCalc_test` subclass initialises the frame semaphore (normally done in `appStartup()`), names the fitter
  properties, and exposes the protected pupil geometry, dark image, stream geometry (main, dark and framegrabber bases)
  and the protected framegrabber hooks through small accessors.
- No shared memory streams, threads or INDI server are used; stream geometry is set directly on the base-class members.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../pwfsSlopeCalc.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/pwfsSlopeCalc/tests/pwfsSlopeCalc_test
../apps/pwfsSlopeCalc/tests/pwfsSlopeCalc_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called, since they start the shmimMonitor, framegrabber and
  telemetry threads.
- `recordTelem()`/`checkRecordTimes()` are not tested (they write telemetry).
- The timeout case of `acquireAndCheckValid()` waits about 1 second.
