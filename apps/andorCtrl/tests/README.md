# andorCtrl unit tests

Catch2 unit tests for the `andorCtrl` MagAO-X application (`apps/andorCtrl/andorCtrl.hpp`), the Andor iXon EMCCD (SDK2 + EDT framegrabber) camera controller.

## Test files

| File | Covers |
|------|--------|
| `andorCtrl_test.cpp` | SDK helper functions, configuration, camera selection, temperature, EM gain, readout/vertical-shift speeds, exposure/FPS, shutter, crop mode and ROI, framegrabber hooks, power off/shutdown, stdCamera INDI callbacks, and telemetry |

## What is tested

- **Helper functions**: `andorSDKErrorName()` (known and unknown codes), `readoutParams()` for every readout speed name and an invalid one, `vshiftParams()` for every vertical shift speed and an invalid one (which resets to index 0 / 0.3 us).
- **Constructor and configuration**: constructor defaults and the `stdCamera`/`edtCamera`/`frameGrabber` flags; a defaults-only config file (the generated `onlymode` EDT camera mode and `/tmp/andor_<name>.cfg` file, stdCamera, edtCamera/ioDevice and frameGrabber defaults); overrides of `camera.maxEMGain`, `camera.defaultReadoutSpeed`, `camera.defaultVShiftSpeed`, `camera.startupTemp`, `framegrabber.pdv_unit`/`pdv_channel`/`numBuffs`/`shmimName` and `device.readTimeout`; `maxEMGain` limits (1 to 300); `m_shutdown` when the EDT config file can not be written; `writeConfig()` binned width/height; `powerOnDefaults()`.
- **Camera selection**: `cameraSelect()` success (SDK init directory, camera handle, shutter initialized shut or kept open, Camera Link, read/acquisition/frame-transfer/EM-gain modes, default readout and vertical shift speeds, exposure, cooler start when a setpoint is set), library already initialized, no camera at initialization (`DRV_USBERROR`, `DRV_ERROR_NOCAMERA`, `DRV_VXDNOTINSTALLED`), initialization error, zero cameras, invalid default speeds, and an SDK error from each checked call.
- **Temperature**: `getTemp()` status mapping for each `DRV_TEMPERATURE_*` code and the error path; `setTempControl()` on/off/errors; `setTempSetPt()` rounding and error.
- **EM gain**: `getEMGain()` (0 reported as 1, error), `setEMGain()` values, gain 1 turning EM off, limits at 0 and `m_maxEMGain`, the conventional-amplifier guard, and error.
- **Readout**: `setReadoutSpeed()` (amplifier and HS speed, crop mode disabled in CCD mode, invalid name, SDK errors) and `setVShiftSpeed()` (index and speed, invalid name, SDK error); both flag a reconfiguration.
- **Other hooks**: `getFPS()`/`fps()`, `setExpTime()`, `setNextROI()`, `checkNextROI()`, `setShutter()` shut/open/error, `setCropMode()` in EM and CCD modes.
- **Acquisition setup**: `configureAcquisition()` not idle, `GetStatus` error, image mode (1-based inclusive `SetImage` region, current/next ROI, binned width/height, data type), crop mode (`SetIsolatedCropModeEx` arguments, low-latency type), and the ROI revert on each `SetImage`/crop-mode error code.
- **Framegrabber hooks**: `startAcquisition()` (already acquiring, errors, starting the camera and the EDT images), `acquireAndCheckValid()` and `loadImageIntoStream()` through the EDT stub, `reconfig()` writing the config file and reloading the EDT mode, and the EDT failure path.
- **Power off and shutdown**: `onPowerOff()`, `whilePowerOff()`, `appShutdown()` (SDK `ShutDown`, shutter status, `m_poweredOn`, frameGrabber reset).
- **INDI callbacks**: `stdCamera::newCallBack_stdCamera` with a wrong device, unhandled properties (`fps`, `blacklevel`), `readout_speed`, `vshift_speed`, `emgain`, `temp_controller`, `temp_ccd`, `exptime`, `roi_crop_mode`, `shutter` and `roi_region_x`, reaching the andorCtrl hooks.
- **Telemetry**: `recordTelem()` and `checkRecordTimes()` write `telem_stdcam`.

## Test harness

- `andorCtrl_test.cpp` `#undef`s `MAGAOX_NOEDT` (set by the test build), because andorCtrl derives from `dev::edtCamera`, which is compiled out by that flag. The EDT SDK is then provided by the shared `tests/edtinc.h` stub declarations, with the function definitions and an `edtStubState` in the test source.
- The app header is included with `#define protected public`. `andorCtrl_test` (an `andorCtrl` subclass) sets the power state on, sets the device/name of the INDI properties whose callbacks check the property key, adds a helper for the single camera mode, and removes the `/tmp/andor_<name>.cfg` file that `writeConfig()` creates.
- INDI callback bodies are live (`tests/testMacrosINDI.hpp` is not included), so the tests check what a request does.
- `stubs/atmcdLXd.h`: stand-in for the Andor SDK2 header with the `DRV_*` codes (real SDK values, so the `andorSDKErrorName()` switch has distinct cases), `at_32`, `MAX_PATH`, and only the functions andorCtrl calls. The functions are defined in `andorCtrl_test.cpp` and act on an `andorStubState` (forced return codes and call counts by function name, captured arguments, and scripted values).

## Required app fixes

`andorCtrl.hpp` does not compile against the current `libMagAOX` until `static constexpr bool c_stdCamera_blacklevel = false;` is added, since `dev::stdCamera` reads `derivedT::c_stdCamera_blacklevel` unconditionally. The tests assume this fix.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../andorCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/andorCtrl/tests/andorCtrl_test
../apps/andorCtrl/tests/andorCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No camera or framegrabber is used; the SDK and EDT stubs record calls and return scripted values.
- `appStartup()` and `appLogic()` are not called: they start the telemetry log thread, check the framegrabber thread (never started here), and `appLogic()` sleeps 30 s on connection.
- `startAcquisition()` and the EDT-failure `reconfig()` case each sleep 1 s, so the run takes about 2 s.
- `setTempSetPt()` is only checked for positive setpoints: it rounds with `int( setpt + 0.5 )`, which truncates negative setpoints toward zero (e.g. -40 C is sent as -39 C).
