# dmPokeCenter unit tests

Catch2 unit tests for the `dmPokeCenter` MagAO-X application (`apps/dmPokeCenter/dmPokeCenter.hpp`).

## Test files

| File | Covers |
|------|--------|
| `dmPokeCenter_test.cpp` | Configuration, stream allocation and frame copy, `runSensor()` without shutter control, pupil and poke fitting, `analyzeSensor()`, live INDI callback effects, and poke-center telemetry |
| `dmPokeCenter_indi_test.cpp` | Device/name validation of every INDI callback with `XWCTEST_INDI_NEW_CALLBACK` and `XWCTEST_INDI_SET_CALLBACK` |

## What is tested

- **Configuration**: defaults (shmim name and camera device default to the config name, semaphore waits split into seconds and nanoseconds, poke and pupil-fit parameters), overrides of every key, and shutdown on an empty or mismatched `pokeX`/`pokeY`.
- **Streams**: `allocate()` creates the raw, dark, pupil and poke images at the camera size and sizes the DM image from the DM channel (failing, before the poke image is created, when the channel is missing); `processImage()` converts `int16` frames to float and posts the image semaphore.
- **runSensor**: fails on the first run because the shutter cannot be closed without INDI, and on later runs returns without measuring when the shutter cannot be opened.
- **Fitting**: `fitPupil()` finds the center of a synthetic uniform disk (to within a pixel) and rejects a cut buffer that runs off the image; `fitPokes()` locates two Gaussian pokes (brightest first) and their average.
- **analyzeSensor**: reports pupil minus average-poke position in the `measurement` property and increments the counter; it does nothing on a stop request or shutdown, and reports a pupil-fit failure.
- **INDI**: `poke_amp`, `nPupilImages` and `nPokeImages` take `target` (or `current`) and reject other devices/elements; the camera `fps` callback reads `current`; the camera `shutter` callback maps the toggle to shut/open and rejects a missing toggle; `single`/`continuous` start measurements only when idle and post the WFS semaphore, `continuous` off and `stop` request a stop only while measuring.
- **Telemetry**: `recordPokeCenter()` records only on a change of measuring state, pupil or poke positions, or when forced; `recordTelem()` always records; `checkRecordTimes()` respects `telemeter.maxInterval`.

## Test harness

- `dmPokeCenter_test` subclass exposes the protected members with using-declarations, sets the INDI property keys, adds the `measurement` elements, sets the shmimMonitor geometry directly, creates the pupil/poke images for the fitting tests, and initializes the two semaphores that `appStartup()` would normally initialize.
- The stream and fitting tests create shared-memory images with `MILK_SHM_DIR=/tmp/dmPokeCenter_test_shm`, which is removed afterwards.
- `fitPupil()`, `fitPokes()` and `runSensor()` write debugging FITS files to fixed paths in `/tmp` (`fullMask.fits`, `pupilMagnified.fits`, `magMask.fits`, `magEdge.fits`, `sm.fits`, `pupilImage.fits`, `poke.fits`); the tests remove them.
- `dmPokeCenter_test.cpp` includes no INDI test macros, so callback bodies are live; `dmPokeCenter_indi_test.cpp` includes `testMacrosINDI.hpp` before the app header so the callbacks return right after validation.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dmPokeCenter.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dmPokeCenter/tests/dmPokeCenter_test
../apps/dmPokeCenter/tests/dmPokeCenter_test
make -f Makefile.one t=../apps/dmPokeCenter/tests/dmPokeCenter_indi_test
../apps/dmPokeCenter/tests/dmPokeCenter_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The full dark/pupil/poke sequence in `runSensor()` is not tested: it needs a camera shutter commanded over INDI.
- `appStartup()`, `appLogic()` and `appShutdown()` are not run: they start the shmimMonitor, WFS and telemetry threads.
- `runSensor()` with no allocated dark is not tested because it waits 2 s for the dark.
- The `wfscam.camDevName`, `wfscam.loopSemWait` and `wfscam.imageSemWait` keys are read from the config-file section `[wfs]` (as registered by the app), so the overrides test writes them there.
