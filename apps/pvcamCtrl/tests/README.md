# pvcamCtrl unit tests

Catch2 unit tests for the `pvcamCtrl` MagAO-X application (`apps/pvcamCtrl/pvcamCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `pvcamCtrl_test.cpp` | Configuration, camera connection, fan-speed and temperature readout, exposure/FPS, readout speed, ROI and binning, the speed table, framegrabber hooks, PVCAM error handling, INDI callbacks, shutdown, and telemetry |

## What is tested

- **Constructor and helpers**: constructor defaults (exposure, ROI defaults and full frame, readout-speed and fan-speed names/labels, the `stdCamera`/`frameGrabber` flags) and `pvcamErrMessage()` formatting of `pl_error_message()` text.
- **Configuration**: serial-number-only config (defaults), overrides (`camera.serialNumber`, `camera.circBuffMaxBytes`, `camera.defaultReadoutSpeed`, `camera.fanSpeedControl`, `camera.defaultFanSpeed`, `camera.default_w/h`, `framegrabber.acqSleep`, `framegrabber.shmimName`, `shutter.dioDevice`), and the fatal errors for a missing serial number and an invalid default fan speed (`m_shutdown`).
- **Power-on defaults and simple hooks**: `powerOnDefaults()` with fan control enabled and disabled; the no-op `setTempControl()`, `setTempSetPt()`, `setVShiftSpeed()`, `setEMGain()`; `setReadoutSpeed()` and `setNextROI()` requesting a reconfiguration; `fps()`; an invalid `setShutter()` request.
- **Fan control**: `setFanSpeed()` for each of `high`/`medium`/`low`/`off` (the `PARAM_FAN_SPEED_SETPOINT` value sent), an invalid name, and an SDK error; `getFanSpeed()` for each PVCAM value, an unknown value, the parameter not available, failed reads with power on (`ERROR` state) and off, and no SDK access while `OPERATING` or with fan control disabled.
- **Temperature**: `getTemp()` conversion from hundredths of a degree, `LOCKED`/`UNLOCKED` status against `m_tempTol`, unavailable parameters, a failed read with power off, and no SDK access while `OPERATING`.
- **Exposure and FPS**: `setExpTime()` within limits, clamped to the minimum and maximum, and failed limit reads; `setFPS()` conversion to an exposure time.
- **ROI**: `checkNextROI()` for a valid ROI, an oversize ROI, and ROIs past the high and low detector edges.
- **Connection**: `connect()` finding the camera by serial number at the second index (exposure resolution, default fan speed applied), fan control disabled, no cameras, serial not found or unavailable, a camera that fails to open, closing an already open handle (and that failing), uninit reporting not-initialized, init/count/serial-read failures, and a failed fan read after connecting.
- **Speed table**: `fillSpeedTable()` enumerating ports, speeds, and gains, the not-connected check, and SDK failures; `dumpEnum()` connected and not connected.
- **Acquisition setup**: `configureAcquisition()` callback registration, readout port for each readout speed (and the 8-bit `speed` mode), fallback for an unknown speed, unbinned and binned PVCAM regions and image size, exposure readback, exposure- and readout-limited frame rate, the two-pass FPS-requested exposure, circular buffer sizing, fan-speed restore after setup, and each SDK failure path.
- **Framegrabber hooks**: `startAcquisition()`, `acquireAndCheckValid()`, `loadImageIntoStream()` for 16-bit and 8-bit readout, the end-of-frame callback (`st_endOfFrameCallback()`/`endOfFrameCallback()`) handshake with the frame-ready and frame-done semaphores, and `reconfig()`.
- **INDI callbacks**: `newCallBack_stdCamera()` for a wrong device, an unknown property, `fan_speed` (including two speeds selected and fan control disabled), `readout_speed`, `exptime`, and `fps`, reaching the pvcamCtrl hooks.
- **Shutdown, power-on wait, telemetry**: `appLogic()` during the power-on wait, `appShutdown()` with and without an open camera, `recordTelem()` and `checkRecordTimes()`.

## Test harness

- `pvcamCtrl_test.cpp` includes the app header with `#define protected public`, and defines `pvcamCtrl_test` (a `pvcamCtrl` subclass) which sets the power state on, initializes the frame-ready/frame-done semaphores normally created in `appStartup()`, sets the device/name of the `exptime` and `fps` properties used by `indiTargetUpdate()`, and frees the circular buffer `configureAcquisition()` allocates (the app itself never frees it). `prepareAcq()` opens handle 0, uses a 1 MB circular buffer, and sets the stub exposure limits, readout time, and fan availability.
- INDI callback bodies are live (`tests/testMacrosINDI.hpp` is not included), so the tests check what a request does.
- `stubs/`: stand-ins for the Teledyne PVCAM SDK headers (`master.h` with the basic types, `pvcam.h` with the parameter IDs, attributes, enums, `rgn_type`, `FRAME_INFO`, and `pl_*` prototypes), shadowing `/opt/pvcam/sdk/include/`. The `pl_*` functions are defined in `pvcamCtrl_test.cpp` and act on a `pvcamStubState` (cameras and serial numbers, parameter values by parameter and attribute, forced failures, captured `pl_set_param` calls, regions, exposures, and buffers). `pl_get_param` writes each value with the width of the variable pvcamCtrl passes, and failing calls set `pl_error_code()`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../pvcamCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/pvcamCtrl/tests/pvcamCtrl_test
../apps/pvcamCtrl/tests/pvcamCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Requirements on the app

- `dev::stdCamera` reads `derivedT::c_stdCamera_blacklevel` unconditionally, and `pvcamCtrl` does not define it. The app (and these tests) will not compile until `pvcamCtrl` adds `static constexpr bool c_stdCamera_blacklevel = false;` with its other `c_stdCamera_*` flags. The tests are written assuming that fix. No other required `stdCamera` members are missing.

## Limitations

- No camera is used; the SDK stub records calls and returns scripted values.
- `appStartup()` and most of `appLogic()` are not called, since they start the telemetry and framegrabber threads and the `dssShutter` open/shut threads.
- `setShutter()` is tested only with an invalid state, since valid states signal the `dssShutter` threads.
- The failure paths of `getTemp()` with power on, and of `pl_exp_get_latest_frame()` in `loadImageIntoStream()`, are not tested because the app then reads an uninitialized variable (see the notes below).
- The end-of-frame callback test waits for a frame-ready signal in a loop; it normally completes in well under a second.

## Notes on current app behavior

- `setExpTime()` clamps to `(int)(min/1e6 + 0.5)` and `(int)(max/1e6 - 0.5)` seconds, so a sub-second limit is rounded to a whole number of seconds (the tests use limits where this is exact).
- `configureAcquisition()` reads `PARAM_EXPOSURE_TIME` `ATTR_CURRENT` into an `uns32`, while `setExpTime()` reads `ATTR_MIN`/`ATTR_MAX` into `ulong64`; the stub matches each.
- `fillSpeedTable()` reads `ATTR_MIN` for the maximum gain, returns `false` (0, success) when `pl_get_enum_param()` fails, and never fills the `gains` entries.
- `checkNextROI()` sets the ROI center to 0 when it trims an ROI past the low edge.
