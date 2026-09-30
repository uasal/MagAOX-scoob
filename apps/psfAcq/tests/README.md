# psfAcq unit tests

Catch2 unit tests for the `psfAcq` MagAO-X application (`apps/psfAcq/psfAcq.hpp`).

## Test files

| File | Covers |
|------|--------|
| `psfAcq_test.cpp` | Configuration, star detection/fitting/ranking/tracking on synthetic images, dark handling, star acquisition, star INDI property bookkeeping, telemetry, and the INDI callbacks |

## What is tested

- **Configuration**: defaults and overrides of `fitter.*` (fps source, `max_loops`, `zero_area`, `threshold`,
  `fwhm_threshold`), `acquisition.x_center`/`y_center`, and the `shmimMonitor`/`darkShmim` stream settings.
- **Helpers**: the `Star` property ownership (`allocate`, `hasProp`, `prop` throwing when empty, move) and
  `calculateDistance()`.
- **Detection**: `processImage(shmimT)` on 96x96 frames of Gaussian stars on seeded Gaussian noise: a single star is
  fit (position, peak, FWHM, seeing) and exported as `star_0`; several stars are ranked by brightness as
  `star_0..N`; `max_loops`, `threshold` and `fwhm_threshold` limit or reject detections.
- **Tracking**: a star that moves by a few pixels keeps its id, a new far star is added, and stars missing for one
  frame are removed along with their INDI registration.
- **Early returns**: null pointer, unallocated image, zero area larger than the image or non-positive, constant
  image (no noise estimate), noise-only image, and the post-acquisition pause.
- **Dark**: non-float and null darks are rejected, dark frames accumulate into `m_dark`, and the dark is subtracted
  before detection.
- **Acquisition**: selecting a valid star resets the tracked stars and starts the pause; out-of-range selections are
  cleared.
- **Bookkeeping**: `relabelStarsByBrightness()` ordering (ties broken by id), property reuse when labels already
  match, `removeStar()` and `resetAcq()`.
- **Telemetry**: `telem_psfacq::messageT` field round trip and `msgString()`, `emitStarTelemetry()`,
  `recordTelem()`, `checkRecordTimes()`, and the once-per-second emission from `processImage()`
  (observed through `telem_psfacq::lastRecord`).
- **INDI**: device/name validation of `restart_acq`, `record_seeing` and `flipacq.presetName`
  (`XWCTEST_INDI_ARBNEW_CALLBACK`/`XWCTEST_INDI_SET_CALLBACK`), and the effects of `restart_acq`, `record_seeing`,
  `acquire_star`, `seeing_star`, the fps source, and the `flipacq` out On-to-Off auto restart.

## Test harness

- `psfAcq_test` subclass exposes the protected members and methods, sets the stream geometry the shmimMonitors would
  normally set, and calls `allocate()`/`processImage()` directly on in-memory frames, so no ImageStreamIO stream is
  created. `testMacrosINDI.hpp` is included after the app header, so callback bodies are live.

## Known limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called: they start (or join) the shmimMonitor and
  telemeter threads.
- The telescope nudge sent on acquisition goes through `sendNewProperty()`, which fails without an INDI driver, so
  the nudge values are not checked.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../psfAcq.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/psfAcq/tests/psfAcq_test
../apps/psfAcq/tests/psfAcq_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).
