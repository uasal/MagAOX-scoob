# pixelinkCtrl unit tests

Catch2 unit tests for the `pixelinkCtrl` MagAO-X application (`apps/pixelinkCtrl/pixelinkCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `pixelinkCtrl_test.cpp` | Configuration, connection, temperatures, streaming, gain/exposure/FPS, ROI and binning setup, framegrabber hooks, power off, INDI callbacks, telemetry |

## What is tested

- **Constructor and configuration**: constructor defaults (default/full ROI, max gain, exposure) and the
  `stdCamera`/`frameGrabber` flags; defaults-only and override config files (`camera.maxEMGain`, `camera.default_*`,
  `framegrabber.shmimName`, `framegrabber.defaultFlip`, `telemeter.maxInterval`); `powerOnDefaults()`.
- **Connection and state**: `connect()` (first camera, re-open of an open camera, no camera), `getAcquisitionState()`
  for power off, not streaming, running and stopped.
- **Temperatures**: `getTemps()` reads `FEATURE_SENSOR_TEMPERATURE` and `FEATURE_BODY_TEMPERATURE`, with SDK errors.
- **Streaming**: `toggleStreaming()` start/stop and SDK errors.
- **Gain, exposure, FPS**: `setEMGain()` (`FEATURE_GAIN`, manual flag, camera readback, set and readback errors),
  `setExpTime()` (`FEATURE_EXPOSURE` set, `FEATURE_SHUTTER` readback, errors), `setFPS()` and `fps()`.
- **ROI and binning**: `checkNextROI()`, `setNextROI()` (sets `m_reconfig`), and `configureAcquisition()`: stream
  start/stop/start sequence, exposure, gain, `PIXEL_FORMAT_MONO16`, ROI parameters, binning parameters, camera-adjusted
  ROI/binning readback, frame size and data type, and every SDK error return (the ROI set error is not fatal).
- **Framegrabber hooks**: `startAcquisition()`, `reconfig()`, `acquireAndCheckValid()` (buffer size, descriptor size,
  time stamp, error), `loadImageIntoStream()` (big-endian byte swap, no flip, up-down flip, invalid flip).
- **Power off and shutdown**: `onPowerOff()`, `whilePowerOff()`, `appShutdown()` stop and release an open camera.
- **INDI callbacks**: `newCallBack_m_indiP_streamSwitch` (name check, toggling), and the `stdCamera` callbacks
  `exptime`, `emgain`, `fps`, `roi_region_x` and `roi_set` reaching the pixelinkCtrl hooks, plus wrong device, unknown
  property, and `blacklevel` (not exposed).
- **Telemetry**: `recordTelem()` and `checkRecordTimes()`.

## Test harness

- The app header is included with `#define protected public`. `pixelinkCtrl_test` (a `pixelinkCtrl` subclass)
  initializes the members the constructor leaves unset (`m_running`, temperatures), turns the power on, names the INDI
  properties used by the callbacks, and frees the frame buffer that `configureAcquisition()` allocates with `malloc`.
- INDI callback bodies are live (`tests/testMacrosINDI.hpp` is not included).
- `stubs/PixeLINKApi.h`: stand-in for the PixeLINK SDK header, declaring only the types, constants and functions
  pixelinkCtrl uses. The functions are defined in `pixelinkCtrl_test.cpp` and act on a `pxlStubState` (stored feature
  parameters, readback overrides, forced return codes, stream state history, frame data). `FEATURE_EXPOSURE` is an
  alias of `FEATURE_SHUTTER`, as in the SDK.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../pixelinkCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/pixelinkCtrl/tests/pixelinkCtrl_test
../apps/pixelinkCtrl/tests/pixelinkCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Known issues and limitations

- **Required app fix**: `dev::stdCamera` reads `derivedT::c_stdCamera_blacklevel` unconditionally, and pixelinkCtrl
  does not define it, so the app (and these tests) do not compile until pixelinkCtrl adds
  `static constexpr bool c_stdCamera_blacklevel = false;`. The tests assume that fix.
- `setFPS()` sends the current `m_fps` rather than the requested `m_fpsSet`, and never updates `m_fps`; the tests
  check the current behavior.
- `acquireAndCheckValid()` passes a fixed 512x512x2 byte buffer size regardless of the ROI; the stub writes only the
  test's frame data.
- `configureAcquisition()` allocates a new frame buffer on every call without freeing the old one, and its
  `if(PXL_RETURN_CODE rc = ... < 0)` stores the comparison result in `rc`.
- `getTemps()` treats only codes `< -1` as errors (SDK errors are large negative numbers, so they are caught).
- `appStartup()` and `appLogic()` are not called, since they start the framegrabber and telemetry threads.
