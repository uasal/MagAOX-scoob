# qhyCtrl unit tests

Catch2 unit tests for the `qhyCtrl` MagAO-X application (`apps/qhyCtrl/qhyCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `qhyCtrl_test.cpp` | Configuration, SDK helper functions, camera connection, acquisition setup and readout, temperature/exposure handling, the exposure INDI callback, ROI hooks, SDK startup/shutdown, and telemetry |

## What is tested

- **Constructor and configuration**: constructor defaults and `stdCamera`/`frameGrabber` flags; defaults-only and override config files (`camera.serialNumber`, `camera.bits`, `camera.startupTemp`, `framegrabber.shmimName`, `framegrabber.circBuffLength`), including the copy of the serial number into `m_camId`.
- **SDK helpers**: `qhyccdSDKErrorName()`, `SDKVersion()` for each version-string layout, and `FirmWareVersion()` success and failure.
- **Connection**: `connect()` opens the camera by ID, closes an open camera first, handles a null handle, and maps an SDK exception to `NODEVICE`.
- **Acquisition setup**: `configureAcquisition()` with no camera, a camera status error, and success (binning, 1-based top-left resolution from the ROI center, image size, data type, and frame-buffer allocation/reuse/reallocation).
- **Framegrabber hooks**: `startAcquisition()`, `AbortAcquisition()`, `reconfig()`, `getFPS()`, `fps()`, and `loadImageIntoStream()` (frame readout and copy, and an SDK exception).
- **Temperature and exposure**: `getTemp()`, `getExpTime()` (microseconds to seconds), and `setExpTime()`, with no camera, success, and SDK exceptions.
- **INDI**: the `stdCamera` `exptime` callback rejects the wrong device and reaches `setExpTime()`.
- **ROI and power on**: `checkNextROI()`, `setNextROI()`, `powerOnDefaults()`.
- **Lifecycle**: `appStartup()` fails when `InitQHYCCDResource()` fails; `appShutdown()` closes the camera and releases the SDK, including a release failure.
- **Telemetry**: `recordTelem()` and `checkRecordTimes()`.

## Test harness

- `qhyCtrl_test.cpp` includes the app header with `#define protected public`, and defines `qhyCtrl_test` (a `qhyCtrl` subclass) which initializes the members the `qhyCtrl` constructor leaves unset (`m_camId`, `m_ccdTemp`, `m_expTime`, `m_expTimeSet`, `m_frame_length`, `m_frame_data`), sets up the `exptime` property, and frees the frame buffer.
- `stubs/qhyccd.h`: stand-in for the QHYCCD SDK header, declaring only what qhyCtrl uses (`qhyccd_handle` is `void`, as in the real SDK). The functions are defined in `qhyCtrl_test.cpp` and act on a `qhyStubState` (return codes, exceptions to throw, parameter values, frame data, call counts and arguments).
- `stubs/ImageStruct.h` and `stubs/ImageStreamIO.h`: forward to `<ImageStreamIO/...>`, since `qhyCtrl.hpp` includes these headers without the `ImageStreamIO/` prefix.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../qhyCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/qhyCtrl/tests/qhyCtrl_test
../apps/qhyCtrl/tests/qhyCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No camera is used; the SDK stub records calls and returns scripted values.
- `acquireAndCheckValid()` and `setTempSetPt()` are not called: neither returns a value, so calling them is undefined behavior.
- `appLogic()` and the success path of `appStartup()` are not called, since they start the telemetry log thread and depend on the framegrabber thread.
