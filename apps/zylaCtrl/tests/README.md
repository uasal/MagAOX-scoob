# zylaCtrl unit tests

Catch2 unit tests for the `zylaCtrl` MagAO-X application (`apps/zylaCtrl/zylaCtrl.hpp`), the Andor sCMOS (Zyla, SDK3) camera controller.

## Test files

| File | Covers |
|------|--------|
| `zylaCtrl_test.cpp` | Configuration, camera selection, temperature and cooling, AOI/acquisition setup, framegrabber hooks, power off/shutdown, stdCamera INDI callbacks, and telemetry |

## What is tested

- **Constructor and configuration**: constructor defaults (power management, startup temperature, exposure/FPS, power-on and full ROI, image timeout, handle state), the `stdCamera`/`frameGrabber` flags, a defaults-only config file, and overrides of `camera.serial`, `camera.startupTemp`, `camera.default_*`, `framegrabber.shmimName` and `framegrabber.circBuffLength`; `powerOnDefaults()`.
- **Camera selection**: `cameraSelect()` finding the configured serial number at a non-zero index (other cameras closed), serial not found, no cameras, closing an open camera and reinitializing the library, and each SDK error (`AT_FinaliseLibrary`, `AT_FinaliseUtilityLibrary`, `AT_InitialiseLibrary`, `AT_InitialiseUtilityLibrary`, `Device Count`, `AT_Open`, `SerialNumber`, `Camera Model`).
- **Temperature**: `getTemp()` mapping of every `TemperatureStatus` string (and an unknown one) to status/on-target flags, sensor and target temperature readback, and each SDK error; `setTempControl()` on/off and SDK errors.
- **Simple hooks**: `setExpTime()`, `setFPS()`, `setNextROI()` flag `m_reconfig`; `checkNextROI()`, `setTempSetPt()`, `setShutter()`, `getExpTime()`, `getFPS()` are no-ops; `fps()`.
- **Acquisition setup**: `configureAcquisition()` AOI binning/size/corner values sent to the SDK, current ROI computed from the readback, width/height/stride/data type, buffer allocation and queueing, exposure time and frame rate, pixel encoding, continuous cycle mode, buffer replacement on reconfiguration, the not-open guard, and SDK errors on every checked feature, `AT_Flush` and `AT_QueueBuffer`.
- **Framegrabber hooks**: `startAcquisition()`, `reconfig()` (stop + flush), `acquireAndCheckValid()` (valid frame, timeout, error, wrong size), `loadImageIntoStream()` (Mono16 conversion arguments, in-order requeue, wrap-around, skipped buffer, queue error).
- **Power off and shutdown**: `onPowerOff()`, `whilePowerOff()`, `appShutdown()` close the camera and finalize both SDK libraries.
- **INDI callbacks**: `stdCamera::newCallBack_stdCamera` with a wrong device, an unhandled property (`blacklevel`), `temp_controller` on/off, `temp_ccd`, `fps`, `exptime`, `roi_region_x` and `roi_set`, reaching the zylaCtrl hooks.
- **Telemetry**: `recordTelem()` and `checkRecordTimes()` write `telem_stdcam`.

## Test harness

- `zylaCtrl_test.cpp` includes the app header with `#define protected public`, and defines `zylaCtrl_test` (a `zylaCtrl` subclass) which sets the power state on, sizes `m_inputBuffers` as `appStartup()` would, initializes `m_stride`/`m_pixelEncoding` (left unset by the constructor), sets the device/name of the INDI properties used by the callbacks, and adds helpers to mark the camera open and allocate buffers.
- INDI callback bodies are live (`tests/testMacrosINDI.hpp` is not included), so the tests check what a request does.
- `stubs/atcore.h` and `stubs/atutility.h`: stand-ins for the Andor SDK3 headers with the real SDK types (`AT_H`/`AT_BOOL` are `int`, `AT_64` is `long long`, `AT_WC` is `wchar_t`) and only the functions zylaCtrl calls. The functions are defined in `zylaCtrl_test.cpp` and act on a `zylaStubState` (camera serials/models, integer/float/bool/enum feature values, per-feature forced error codes, captured set values, commands, queued buffers, wait and convert results, call counts).

## Required app fixes

`zylaCtrl.hpp` does not compile against the current `libMagAOX` and these tests assume it has been fixed:

- `static constexpr bool c_stdCamera_blacklevel = false;` must be added, since `dev::stdCamera` reads `derivedT::c_stdCamera_blacklevel` unconditionally.
- The constructor and `powerOnDefaults()` use `m_startup_x`, `m_startup_y`, `m_startup_w`, `m_startup_h`, `m_startup_bin_x`, `m_startup_bin_y`, which no longer exist in `dev::stdCamera`; they must be renamed to `m_default_x` ... `m_default_bin_y`. The tests check the power-on ROI through the `m_default_*` names.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../zylaCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/zylaCtrl/tests/zylaCtrl_test
../apps/zylaCtrl/tests/zylaCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No camera is used; the SDK stub records calls and returns scripted values.
- `appStartup()` and `appLogic()` are not called: they start the telemetry log thread and check the framegrabber thread, which is never started in the tests.
- The unchecked return values of `AT_SetFloat("ExposureTime")`/`AT_SetFloat("FrameRate")` and of the pixel encoding queries in `configureAcquisition()` are not tested, since the app ignores them.
