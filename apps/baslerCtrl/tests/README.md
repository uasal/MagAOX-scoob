# baslerCtrl unit tests

Catch2 unit tests for the `baslerCtrl` MagAO-X application (`apps/baslerCtrl/baslerCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `baslerCtrl_test.cpp` | Configuration, connection and ROI interrogation, ROI checking and setup, blacklevel handling, temperature/exposure/FPS, framegrabber hooks, state string, shutdown, INDI callbacks, telemetry |

## What is tested

- **Constructor and configuration**: constructor defaults and `stdCamera`/`frameGrabber` flags (including
  `c_stdCamera_blacklevel == false`); defaults-only and override config files (`camera.serialNumber`, `camera.bits`,
  `camera.default_*`, `framegrabber.shmimName`, `framegrabber.defaultFlip`, `telemeter.maxInterval`).
- **Default ROI fallback (commit 28e0f0d)**: each unset `camera.default_*` value falls back to the full ROI on its own,
  so a partial default ROI keeps its configured values; `powerOnDefaults()` no longer clobbers the ROI.
- **Blacklevel (commits 39b3a5e, 28e0f0d)**: blacklevel is not exposed: a `blacklevel` request is rejected by
  `newCallBack_stdCamera`, `newCallBack_blacklevel` is a no-op, and `configureAcquisition()` sets `CenterX`/`CenterY`
  only when `GenApi::IsWritable` reports them writable (a camera whose `CenterX` is not writable still configures).
- **Connection**: `connect()` by serial number, shmim name from model and serial, continuous-acquisition
  configuration, exposure auto off, pixel format for 8/10/12 bits (and an unsupported depth), binning modes, the
  interrogated binning values and per-binning width/height/offset limits, the full-ROI consistency checks, and each
  failure path (no camera, open, exposure auto, pixel format), including closing and deleting the camera.
- **ROI**: `checkNextROI()` rounding of size and offset to the increments, clamping to the sensor and to the minimum
  size, invalid binning, binned limits and left-right flip; `configureAcquisition()` binning, width/height and offsets
  for full frame, sub-window, 2x2 binning, left-right and up-down flips, the readback of the current ROI and the full
  ROI for the current binning, frame size and data type, and camera errors.
- **Temperature, exposure, FPS**: `getTemp()` running average, `getExpTime()` (microseconds to seconds), `getFPS()`,
  `setExpTime()`, `setFPS()` (limit on, limit off for 0), with the no-camera and error paths.
- **Framegrabber hooks**: `startAcquisition()` (latest-image strategy), `acquireAndCheckValid()` (timeout, failed
  grab, time stamp), `reconfig()`, `loadImageIntoStream()` (no result, empty buffer, no flip, up-down flip, invalid flip).
- **State string**: `stateString()` format and `stateStringValid()`.
- **Shutdown**: `appShutdown()` closes the camera and terminates Pylon.
- **INDI callbacks**: `newCallBack_stdCamera` for `exptime`, `fps`, `temp_ccd` (read-only), `roi_region_w`,
  `roi_region_check`, `roi_set`, and rejection of the wrong device, `emgain` and `blacklevel`.
- **Telemetry**: `recordTelem()` and `checkRecordTimes()`.

## Test harness

- The app header is included with `#define protected public`. `baslerCtrl_test` (a `baslerCtrl` subclass) sizes the
  temperature history (normally done in `appStartup()`), turns the power on, names the INDI properties used by the
  callbacks, deletes the camera on destruction, and provides `connectCamera()` and `setNext()` helpers.
- INDI callback bodies are live (`tests/testMacrosINDI.hpp` is not included).
- `stubs/`: stand-ins for the Pylon SDK headers baslerCtrl includes. `stubs/pylon/PylonIncludes.h` declares the
  Pylon, GenApi and Basler_UsbCameraParams names baslerCtrl uses; the other headers (`pylon/PixelData.h`,
  `pylon/GrabResultData.h`, `pylon/usb/BaslerUsbInstantCamera.h`, `pylon/usb/_BaslerUsbCameraParams.h`,
  `GenApi/IFloat.h`) include it. Camera parameters are value holders with injectable `SetValue`/`GetValue` exceptions
  and a writable flag. The factory, camera constructor/destructor, open/close, grabbing and Pylon init/terminate are
  defined in `baslerCtrl_test.cpp` and act on a `baslerStubState` (sensor size, binning and increment limits,
  fault flags, call counts). Changing the binning updates the maximum width/height as a real camera does. An empty
  `CGrabResultPtr` throws on access, like Pylon.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../baslerCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/baslerCtrl/tests/baslerCtrl_test
../apps/baslerCtrl/tests/baslerCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Known issues and limitations

- `connect()` fills the per-y-binning offset increments (`m_incYs`) from `OffsetX.GetInc()` rather than
  `OffsetY.GetInc()`; one test checks the current behavior.
- `getTemp()` divides by the history size, which is zero until `appStartup()` sizes the history.
- `appShutdown()` closes but does not delete the camera.
- `appStartup()` and `appLogic()` are not called, since they start the framegrabber and telemetry threads.
- The stub parameters do not enforce ranges or increments; only the injected faults throw.
