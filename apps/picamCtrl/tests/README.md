# picamCtrl unit tests

Catch2 unit tests for the `picamCtrl` MagAO-X application (`apps/picamCtrl/picamCtrl.hpp`), the Princeton
Instruments EMCCD camera controller built on the PICam SDK, `dev::stdCamera`, `dev::frameGrabber`, `dev::dssShutter`
and `dev::telemeter`.

## Prerequisite: `c_stdCamera_blacklevel`

`libMagAOX/app/dev/stdCamera.hpp` reads `derivedT::c_stdCamera_blacklevel` unconditionally, and `picamCtrl` does not
currently define it, so neither the app nor these tests compile until the app declares

```cpp
static constexpr bool c_stdCamera_blacklevel = false; ///< app::dev config to tell stdCamera not to expose black level controls
```

The tests are written assuming that fix (and check the flag is false). No other required `stdCamera` members are
missing: `c_stdCamera_led` and `c_stdCamera_analogGain` are optional (detected by `stdCameraHasLED` and
`stdCameraHasAnalogGain`).

## Test files

| File | Covers |
|------|--------|
| `picamCtrl_test.cpp` | Configuration, readout/vertical-shift parsing, connection and fan-support probing, temperature and cooling-fan readback, EM gain, exposure time, synchro, `configureAcquisition()`, frame acquisition, `reconfig()`, PICam parameter helpers, INDI callbacks, power off and shutdown, telemetry |

## What is tested

- **Construction and configuration**: constructor defaults (readout/vertical-shift/fan names and labels, full-frame ROI, max EM gain) and the `stdCamera`/`frameGrabber` flags; defaults-only and override config files (`camera.serialNumber`, `synchro.*`, `camera.startupTemp`, `camera.defaultReadoutSpeed`, `camera.defaultVShiftSpeed`, `camera.fanSpeedControl`, `camera.defaultFanSpeed`, `camera.maxEMGain`, `camera.default_*`, `framegrabber.shmimName`/`defaultFlip`, `shutter.powerDevice`, `telemeter.maxInterval`); an invalid `camera.defaultFanSpeed`; `powerOnDefaults()` with and without fan control.
- **Parsers**: `readoutParams()` (ADC quality and speed for each readout speed name) and `vshiftParams()`, including invalid names.
- **Connection**: `connect()` with no cameras, power off, the serial number not found, an open failure, success with and without fan control (camera name/model, model handle, pending readout/vertical-shift/fan settings), the `DisableCoolingFan`/`CoolingFanStatus` support probes and their errors, and releasing an open camera and acquisition buffer first.
- **State and thermal readback**: `getAcquisitionState()` (OPERATING, READY with a reconfigure request, ERROR, power off); `getTemps()` status mapping (UNLOCKED, LOCKED, FAULTED, UNKNOWN) and read errors; `getFanSpeed()` mapping (`Off` -> `off`, `On` -> `on`, `ForcedOn` -> `on` with `m_fanForcedOn`), unsupported status, unknown status and read errors.
- **Setters**: the deferred setters (`setTempControl()` always on, `setTempSetPt()`, `setReadoutSpeed()`, `setVShiftSpeed()`, `setFanSpeed()`, `setNextROI()` request a reconfigure), `setEMGain()` (online set on the model handle, clamping to 0 and `m_maxEMGain`, conventional amplifier, errors), `setExpTime()`/`capExpTime()` (ms conversion, offline vs online, capping at the readout time, uncommitted parameter, forwarding to the other camera and the function generator), `setSynchro()`.
- **`configureAcquisition()`**: time stamps, the fan command (`on` -> `DisableCoolingFan = 0`, `off` -> `1`) with its error paths, temperature set point, ADC speed/quality (EM and conventional), vertical shift, ROI conversion for full-frame, binned, left-right and up-down flipped ROIs, an off-sensor ROI, geometry and timing readback errors, exposure-time constraints and capping, buffer allocation and reuse, the hardware trigger for synchro, the readout count, starting the acquisition, and power off.
- **Framegrabber hooks**: `acquireAndCheckValid()` (timeout, error, no readout, camera time stamp from the frame metadata), `loadImageIntoStream()` with flips, and `reconfig()` (including one pass of the stop-and-wait loop).
- **Parameter helpers**: `getPicamParameter()`, `setPicamParameter()` (commit, no commit, commit errors, failed parameters) and `setPicamParameterOnline()`.
- **INDI**: the `stdCamera` `fan_speed`, `readout_speed`, `vshift_speed`, `temp_ccd`, `temp_controller`, `emgain`, `exptime`, `synchro` and `roi_set` callbacks reaching the picamCtrl hooks (including device mismatch and fan control disabled), and the app's `receiveSynchro` and `receiveExptime` callbacks with device/name validation.
- **Power and telemetry**: `onPowerOff()`, `whilePowerOff()`, `appShutdown()` and the shutter status; `recordTelem()` and `checkRecordTimes()`.

## Test harness

- The app header is included with `#define protected public` (flowRPM precedent) so the protected state of the app and its `dev::` base classes can be set and checked directly.
- `picamCtrl_test` subclass: resets the fake SDK state, sets `m_configName`, points telemetry at `/tmp/picamCtrl_test_telem` (removed at exit), marks the telemetry log thread as running, turns the power on, and initializes members the constructor leaves unset (`m_tsRes`, `m_frameSize`, `m_FrameRateCalculation`, `m_ReadOutTimeCalculation`). `power()` and `shutterPower()` set the camera and shutter power states (the two `m_powerState` members are distinct). `startupProperties()` calls `stdCamera::appStartup()` and creates the other INDI properties `appStartup()` would, without starting threads. `openCamera()` installs the fake handles, and `primeConfigure()` sets up a frame-transfer camera and a full-frame ROI for `configureAcquisition()`.
- `stubs/picam.h` and `stubs/picam_advanced.h`: stand-ins for the PICam SDK headers declaring only the types, enumerations and functions the app uses. The functions are defined in `picamCtrl_test.cpp` and act on `g_picamStub`, which holds the available cameras, integer/floating/large-integer parameter values, forced errors per parameter (get, set and online set), a record of every set call (handle, parameter, value, kind), commit failures, parameter existence/readability, the acquisition running sequence, the ROI, exposure constraints and acquisition data. Enumerator values are arbitrary, and `PicamError` has an `int` underlying type.
- Callback bodies run live (`testMacrosINDI.hpp` is not used), with device/name mismatch checked explicitly.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../picamCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/picamCtrl/tests/picamCtrl_test
../apps/picamCtrl/tests/picamCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No camera is used; the SDK stub records calls and returns scripted values.
- `appStartup()` and `appLogic()` are not called: they start the telemetry, shutter and framegrabber threads, and `frameGrabber::appLogic()` calls `pthread_tryjoin_np()` on the (unstarted) framegrabber thread.
- `setShutter()` is only tested with an invalid request, because valid requests signal the (unstarted) shutter threads.
- One `reconfig()` case sleeps for 1 s, as the app does.
- The stub headers are written from how the app uses the SDK (the real headers are not available here), so the enumerator values and some types differ from the real SDK.
