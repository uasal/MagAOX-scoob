# t2wOffloader unit tests

Catch2 unit tests for the `t2wOffloader` MagAO-X application (`apps/t2wOffloader/t2wOffloader.hpp`).

## Test files

| File | Covers |
|------|--------|
| `t2wOffloader_test.cpp` | Configuration, the effective FPS, actuator offloading in `processImage()`, the INDI callback bodies, and the telemetry |
| `t2wOffloader_indi_test.cpp` | INDI callback device/name validation (validation mode) |

## What is tested

- **Configuration**: the `integrator.*` and `offload.*` keywords, `shmimMonitor.shmimName` and `telemeter.maxInterval`, defaults and overrides, including `offload.startupOffloading` setting the initial offloading state.
- **Effective FPS**: `updateFPS()` gives `fps / navg`, and 0 when `navg` is 0.
- **Actuator offloading**: `processImage()` does nothing when not offloading; otherwise it applies the response matrix to the tweeter frame and integrates `gain * delta + (1 - leak) * previous`, clamps each actuator to `+/- actLim`, writes the command into the DM stream and increments its counter; a stuck write flag makes it time out without writing.
- **Callbacks**: `gain`, `leak` and `actLim` (target, current fallback, no value, wrong device/name), `numModes` (clamped to `offload.maxModes`), the `offload` toggle (zeroes the woofer only when starting), `zero` (request off, wrong device), and the `fps`/`nAverage` source set callbacks updating the effective FPS (missing `current` ignored, wrong devices rejected).
- **Telemetry**: `recordLoopGain()` and `recordOffloading()` record only on change unless forced, the `recordTelem()` overloads force a record, and `checkRecordTimes()` records both types once the max interval has elapsed.
- **INDI validation** (`t2wOffloader_indi_test.cpp`): every new and set callback rejects the wrong device and property names.

## Test harness

- `t2wOffloader_test` subclass exposes the protected members and callbacks with `using` declarations. `setupProperties()` creates the INDI properties the way `appStartup()` does, and `setupActuatorOffload()` builds a 2x1 tweeter to 2x2 woofer response matrix and an in-memory woofer DM `IMAGE` (a local `IMAGE_METADATA` and pixel buffer with no semaphores, so `ImageStreamIO_sempost()` does nothing), so no shared memory is created.
- Telemetry records are detected through the `lastRecord` timestamps of `telem_loopgain` and `telem_offloading`, which `telemeter::telem()` updates on every record. The telemetry log level is left at INFO, so the records themselves are dropped and no file is written. `recordLoopGain()`/`recordOffloading()` keep their last values in function-static variables, so each test first forces a record to synchronize them.
- `t2wOffloader_test.cpp` does not include `testMacrosINDI.hpp`, so the callback bodies run. `t2wOffloader_indi_test.cpp` includes it before the app header, so each callback returns right after the device/name check.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../t2wOffloader.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/t2wOffloader/tests/t2wOffloader_test
../apps/t2wOffloader/tests/t2wOffloader_test
make -f Makefile.one t=../apps/t2wOffloader/tests/t2wOffloader_indi_test
../apps/t2wOffloader/tests/t2wOffloader_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `prepareModes()` and `allocate()` are not tested: they read FITS files, create the `aol<N>_dmmask`, `aol<N>_CMmodesDM`, `aol0_modevalDM` and `aol0_modevalDMf` shared-memory streams, open the DM channel, write `/tmp/tModesOrtho.fits` and `/tmp/wModes.fits`, and start the telemetry and shmimMonitor threads.
- Modal offloading (`m_numModes > 0`) and `zero()` are not tested: they need the `milkImage` mode-value streams created by `allocate()` (`milkImage::setWrite()` throws on an unopened image). The `zero` callback is therefore tested only with a request that is not On.
- `appLogic()` is not tested: it calls `pthread_tryjoin_np()` on a monitor thread that is never started in a unit test.
- The test with a stuck DM write flag waits for the 10000 x 1 us retry loop, which takes up to about a second.
