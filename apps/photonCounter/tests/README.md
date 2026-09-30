# photonCounter unit tests

Catch2 unit tests for the `photonCounter` MagAO-X application (`apps/photonCounter/photonCounter.hpp`).

## Test files

| File | Covers |
|------|--------|
| `photonCounter_test.cpp` | Configuration, allocation, dark/threshold calibration, photon counting and stacking, framegrabber hooks, INDI callbacks |

## What is tested

- **Configuration**: defaults-only file (parameters keep their pre-config values, both stream names default to the
  config name) and overrides of `parameters.quantile`, `parameters.Nstack`, `parameters.Ncalibrate`,
  `shmimMonitor.shmimName`, `framegrabber.shmimName` and `framegrabber.circBuffLength`.
- **Allocation**: `allocate()` sizes the calibration cube (`width x height x Ncalibrate`), the dark, threshold and
  counted images, zeroes them, and resets the calibration/stacking state.
- **Calibration**: `processImage()` with calibration on stores frames in the cube, and after `Ncalibrate` frames computes
  the mean dark and the per-pixel quantile threshold (sorted index `int(quantile*N)`) on synthetic 3x3 frames with a
  distinct value per pixel; quantile 0 gives the minimum; a new request clears the calibrated flag.
- **Photon counting**: no counting before calibration; a pixel counts only when strictly above its threshold; counts
  accumulate over `Nstack` frames, after which the stack index resets and the frame semaphore is posted.
- **Framegrabber hooks**: `configureAcquisition()` (no stream: `-1`; stream: copies size, float output),
  `loadImageIntoStream()` (copies and resets the counted image), `acquireAndCheckValid()` (posted frame and 1 s
  timeout), `startAcquisition()`, `reconfig()`, `fps()`.
- **INDI callbacks**: `calibrate` (name check, missing `request`, Off, On), `quantile`, `nFrames` (also resizes the
  cube) and `stackNframes` (name check, `target`, `current`, no value), with live callback bodies.

## Test harness

- The app header is included with `#define protected public`. `photonCounter_test` (a `photonCounter` subclass)
  initializes the frame semaphore (normally done in `appStartup()`) and the members the constructor leaves unset,
  names the INDI properties, and provides `connectStream()` to fake a connected uint16 input stream and `allocate()`.
- No shared memory, threads or INDI server are used.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../photonCounter.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/photonCounter/tests/photonCounter_test
../apps/photonCounter/tests/photonCounter_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Known issues and limitations

- **Compile blocker in the app**: `photonCounter::loadImageIntoStream()` constructs an
  `Eigen::Map<eigenImage<float>>` from `static_cast<uint16_t*>(dest)`, which does not compile (it must be
  `static_cast<float*>(dest)`). photonCounter is not in the top-level app build, so this has not been caught. These
  tests assume that fix; the `loadImageIntoStream` test checks float output.
- The calibration cube and images are allocated as `width x height` but indexed as `(row < height, col < width)`, so
  only square images are safe; the tests use square frames.
- `quantile = 1` gives `q_index == N`, one past the end of the sorted vector; not tested.
- The constructor leaves `m_quantile_cut`, `m_stack_frames`, `calibration_steps`, `m_calibrate` and the image size
  uninitialized; the harness initializes them.
- The INDI callbacks check only the property name, not the device.
- `appStartup()`, `appLogic()` and `appShutdown()` are not called, since they start the shmimMonitor and framegrabber
  threads.
- The `configureAcquisition()` no-stream case sleeps 1 s and the `acquireAndCheckValid()` timeout waits 1 s.
