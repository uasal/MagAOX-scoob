# asiCtrl unit tests

Catch2 unit tests for the `asiCtrl` MagAO-X application (`apps/asiCtrl/asiCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `asiCtrl_test.cpp` | Configuration, camera connection, gain/exposure/temperature/cooler control, ROI and binning setup, framegrabber hooks, power off, INDI callbacks, and telemetry |

## What is tested

- **Constructor and configuration**: constructor defaults (ROI defaults, full frame, max gain), the `stdCamera`/`frameGrabber` flags, defaults-only and override config files (`camera.cameraName`, `camera.maxEMGain`, `camera.startupTemp`, `camera.default_*`, `framegrabber.shmimName`, `framegrabber.defaultFlip`), and `powerOnDefaults()`.
- **Connection**: `connect()` with no cameras, the named camera found at a non-zero index, the name not found, power off, and closing an already open camera; `getAcquisitionState()` for each power/running combination.
- **Gain**: `setEMGain()` (including the camera clamping the value and an SDK error) and `getEMGain()` through `ASI_GAIN`.
- **Temperature and cooler**: `getTemp()` (0.1 C units), `setTempSetPt()`, `setTempControl()` on/off, `getASIParameter()`/`setASIParameter()`.
- **Exposure**: `setExpTime()` sends microseconds and reports the camera's value; SDK error path.
- **ROI and binning**: `configureAcquisition()` for unbinned and binned ROIs (start position, current ROI, bit depth and `m_bfactor`, exposure, gain, black level, buffer allocation), camera-adjusted ROI readback, and each SDK error return.
- **Framegrabber hooks**: `startAcquisition()`, `acquireAndCheckValid()`, `reconfig()`, `setNextROI()`, `checkNextROI()`, `fps()`, and `loadImageIntoStream()` with no flip, up-down flip, and an invalid flip.
- **Power off and shutdown**: `onPowerOff()`, `whilePowerOff()`, `appShutdown()` close an open camera.
- **INDI callbacks**: `newCallBack_m_indiP_blacklevel` (name check, `target` and `current` elements, SDK error), and the `stdCamera` callbacks `emgain`, `temp_ccd`, `temp_controller`, and `roi_set`, which reach the asiCtrl hooks.
- **Telemetry**: `recordTelem()` and `checkRecordTimes()`.

## Test harness

- `asiCtrl_test.cpp` includes the app header with `#define protected public`, and defines `asiCtrl_test` (an `asiCtrl` subclass) which initializes the members the `asiCtrl` constructor leaves unset (`m_camNum`, `m_running`, `m_imgBuff`, `m_imgSize`), sets the power state on, sets the device/name of the INDI properties used by the callbacks, and frees the image buffer that `configureAcquisition()` allocates.
- INDI callback bodies are live (`tests/testMacrosINDI.hpp` is not included), so the tests check what a request does.
- `stubs/ASICamera2.h`: stand-in for the ZWO ASI SDK header, declaring only the types and functions asiCtrl uses. The functions are defined in `asiCtrl_test.cpp` and act on an `asiStubState` (connected cameras, control values, forced return codes, ROI/start position, call counts). `ASI_ERROR_CODE` has a fixed `int` underlying type so that negative error codes can be injected.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../asiCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/asiCtrl/tests/asiCtrl_test
../apps/asiCtrl/tests/asiCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No camera is used; the SDK stub records calls and returns scripted values.
- `appStartup()` and `appLogic()` are not called, since they start the telemetry log thread and depend on the framegrabber thread.
- `setExpTime()` sleeps 1 s per call and a successful blacklevel request sleeps 2 s, so the run takes about 4 s.
