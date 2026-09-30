# nsvCtrl unit tests

Catch2 unit tests for the `nsvCtrl` MagAO-X application (`apps/nsvCtrl/nsvCtrl.hpp` and `apps/nsvCtrl/v4l2lib.hpp`).

## Test files

| File | Covers |
|------|--------|
| `nsvCtrl_test.cpp` | Configuration, v4l2-ctl command building and output parsing, I2C power commands, v4l2lib error paths, ROI handling, INDI callbacks and telemetry |

## What is tested

- **Construction defaults**: EM gain, black level and exposure limits set in the constructor.
- **Configuration**: `camera.*` options, camera mode loading (`m_full_*`, fps limits from the startup mode),
  `camera.maxEMGain` clamping, frame-grabber and telemeter options, the temporary `/tmp/nsvCam_<name>.cfg`
  file written by `writeConfig()`, and the shutdown when that file cannot be written.
- **cmdRes parsing**: value extraction after the first space and the `Cannot open device`/`unknown control`
  error detection.
- **v4l2-ctl getters and setters**: `getFPS`, `getEMGain`, `getBlacklevel`, `getExpTime`, `getVCrop` parse the
  tool output and handle errors; `setEMGain`, `setBlacklevel`, `setFPS`, `setExpTime`, `setVCrop` clamp their
  values and build the expected command lines; `setBitDepth` rejects every value (see limitations);
  `setReadoutMode` selects the sensor mode and resets/clamps the ROI to the mode.
- **Power**: `send_i2c_cmd`, `turn_on_power` and `turn_off_power` I2C command lines.
- **v4l2lib**: every ioctl wrapper fails cleanly on a closed descriptor, bit-depth validation in `initCamera`,
  and `getCameraParams`.
- **Device error handling**: `cameraSelect`, `startAcquisition`, `acquireAndCheckValid` and `reconfig` state
  transitions with no camera.
- **stdCamera hooks**: `powerOnDefaults` ROI defaults and clamping, `setNextROI`, `whilePowerOff` and the no-op hooks.
- **Frame grabber**: `configureAcquisition` ROI snapping and frame geometry, `writeROISubframe` extraction and edge
  clamping, `loadImageIntoStream`, `resizeROIbufs`.
- **INDI callbacks**: `vcropoffset` (with and without `camera.vcropoffset` configured), `bitDepth`, and the
  device/name validation of `power` (`XWCTEST_INDI_NEW_CALLBACK`).
- **Telemetry**: `recordTelem()` and `checkRecordTimes()` for `telem_stdcam`.

## Test harness

- `nsvCtrl_test` subclass: sets the configuration name, points `m_camPath` at a path that does not exist,
  names the INDI properties created in `appStartup()`, and resets the `v4l2lib.hpp` globals (`fd` is set to -1,
  so no call reaches a real descriptor). Protected members are reached via the `#define protected public`
  include precedent. `testMacrosINDI.hpp` is included after the app header so the callback bodies stay live.
- `fakeTools`: the app shells out to `v4l2-ctl` and `i2ctransfer` with `std::system`/`popen`. The tests
  write fake shell scripts with those names into `/tmp/nsvCtrl_test_bin_<pid>/` and put that directory first on
  `PATH`. The fakes log their arguments and print/exit with values set by the test. Each test first checks that
  the shell resolves the tool to the fake, so the real tools (which could drive the camera power controller)
  never run.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../nsvCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/nsvCtrl/tests/nsvCtrl_test
../apps/nsvCtrl/tests/nsvCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `/tmp` must allow executing scripts (not mounted `noexec`) for the fake tools.
- Not covered: `appStartup()`/`appLogic()`/`appShutdown()` (start threads, need INDI), `onPowerOff()` and the
  power-on branch of the `power` callback (they `sleep` for 3 and 6 seconds), and the successful paths of
  `cameraSelect`, `queryBuffer`, `getCamInput`, `getPowerStatus` and `waitForFrame`, which need a real V4L2 device
  (`waitForFrame` calls `exit` on error and `getPowerStatus`/`getCamInput` read uninitialized data on error).
- Some assertions record current behaviour that looks unintended (see the comments in the test): `setBitDepth`
  rejects all values, the `vcropoffset` and `bitDepth` callbacks update the member before rejecting a request,
  and the `camera.default_*` ROI options are ignored because `camera.full_*` are not registered options.
