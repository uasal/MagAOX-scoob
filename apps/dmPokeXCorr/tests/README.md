# dmPokeXCorr unit tests

Catch2 unit tests for the `dmPokeXCorr` MagAO-X application (`apps/dmPokeXCorr/dmPokeXCorr.hpp`), including the parts of the `dev::dmPokeWFS` base class it uses.

## Test files

| File | Covers |
|------|--------|
| `dmPokeXCorr_test.cpp` | Configuration, `dmPokeWFS` stream handling and poke sequencing, live INDI callback effects, the zonal-response reference, the cross-correlation measurement, and poke loop telemetry |
| `dmPokeXCorr_indi_test.cpp` | Device/name validation of every INDI callback with `XWCTEST_INDI_NEW_CALLBACK` and `XWCTEST_INDI_SET_CALLBACK` |

## What is tested

- **Configuration**: defaults (shmim names and camera device default to the config name, semaphore waits split into seconds and nanoseconds, poke parameters), overrides of every `wfscam.*`, `wfsdark.*`, `zrespM.*`, `pokecen.*` and `telemeter.*` key, the camera device defaulting to `wfscam.shmimName`, and shutdown on an empty or mismatched `pokeX`/`pokeY`.
- **Constructor**: the dark and zonal-response monitors grab existing images first.
- **Streams**: `allocate()` for the dark (dark validity from the geometry) and for the WFS camera (raw, poke and DM image sizes, failure on a missing DM channel); `processImage()` converts `uint16` frames to float, subtracts a valid dark, and posts the image semaphore.
- **Poke sequencing**: `runSensor()` fails without a poke image, zeroes the DM and leaves the poke image alone on a stop request, and, with a feeder thread playing the camera (frames are a fixed response scaled by the poked actuator's current DM value), averages the +/- pokes to the poke amplitude times the response and zeroes the DM afterwards.
- **Reference and measurement**: the zonal-response `processImage()` sums the planes of the poked actuators (`y * dm rows + x`) into the reference; `analyzeSensor()` fails without a matching reference, reports no shift for the reference itself, and measures a (+2, -1) pixel shift of a Gaussian.
- **INDI**: `poke_amp`, `nPokeImages` and `nPokeAverage` take `target` (or `current`) and reject other properties/elements; the WFS `fps` set callback reads `current`; `single`/`continuous` start measurements only when idle and post the WFS semaphore, `continuous` off and `stop` request a stop only while measuring, and missing switch elements are rejected.
- **Telemetry**: `recordPokeLoop()` records only on a change of counter, deltas or measuring state, or when forced; `recordTelem()` always records; `checkRecordTimes()` respects `telemeter.maxInterval`.

## Test harness

- `dmPokeXCorr_test` subclass exposes the protected `dmPokeWFS` and app members with using-declarations, sets the INDI property keys, adds the `measurement` elements, sets the shmimMonitor geometries directly, and initializes the two semaphores that `appStartup()` would normally initialize.
- The stream tests create real shared-memory images with `MILK_SHM_DIR=/tmp/dmPokeXCorr_test_shm`, which is removed afterwards.
- `dmPokeXCorr_test.cpp` includes no INDI test macros, so callback bodies are live; `dmPokeXCorr_indi_test.cpp` includes `testMacrosINDI.hpp` before the app header so the callbacks return right after validation.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dmPokeXCorr.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dmPokeXCorr/tests/dmPokeXCorr_test
../apps/dmPokeXCorr/tests/dmPokeXCorr_test
make -f Makefile.one t=../apps/dmPokeXCorr/tests/dmPokeXCorr_indi_test
../apps/dmPokeXCorr/tests/dmPokeXCorr_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not run: they start the shmimMonitor, WFS and telemetry threads, and `dmPokeWFS::appLogic()` joins the WFS thread.
- The WFS thread loop (`wfsThreadExec()`) is not run; the feeder-thread test drives `runSensor()` directly instead.
- `recordPokeLoop()` keeps its last-recorded values in function-local statics, so its tests first force a record to synchronize them.
