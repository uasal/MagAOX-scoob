# hsfwCtrl unit tests

Catch2 unit tests for the `hsfwCtrl` MagAO-X application (`apps/hsfwCtrl/hsfwCtrl.hpp`), the Optec High Speed
Filter Wheel controller built on `libhsfw`, `dev::stdMotionStage` and `dev::telemeter`.

## Test files

| File | Covers |
|------|--------|
| `hsfwCtrl_test.cpp` | Configuration, `presetNumber()`, `moveTo()` rounding (the hsfw bounce fix), homing and stop, the `stdMotionStage` INDI callbacks, the `appLogic()` discovery/connection/status state machine, shutdown, power-off hooks and telemetry |

## What is tested

- **Configuration**: defaults and overrides of `stage.serialNumber` (converted to a `std::wstring`), `stage.powerOnHome`, `stage.homePreset`, `filters.names`/`filters.positions` (a 0 position becomes index + 1) and `telemeter.maxInterval`.
- **Positions**: `presetNumber()` is the zero-based index `m_pos - 1`; `moveTo()` sends exactly one `move_hsfw()` per request with the target rounded to the nearest filter (`static_cast<unsigned short>(filters + 0.5)`, the hsfw bounce fix, commit ed1a6d4), sets `m_moving = 1`, does nothing in POWEROFF, and returns -1 on a libhsfw error.
- **Homing and stop**: `startHoming()` calls `home_hsfw()` and sets `m_moving = 2`, is skipped in POWEROFF, and reports a libhsfw error; `stop()` never talks to the wheel.
- **INDI**: the `stdMotionStage` `filter` (preset), `filterName`, `home` and `stop` callbacks, including device/name mismatch, unknown and multiple selections, a full `filterName` property with one selection producing exactly one move, configured preset positions, and the static dispatcher.
- **State machine**: `appStartup()` from UNINITIALIZED; `appLogic()` in INITIALIZED, POWERON -> NODEVICE, NODEVICE with other wheels, NODEVICE -> NOTCONNECTED while powered off, NODEVICE -> CONNECTED -> READY in one pass (checking the `open_hsfw()` arguments), NOTCONNECTED with no wheel, a vanished wheel, an open failure, and re-opening after closing a stale handle; status mapping to NOTHOMED/HOMING/OPERATING/READY with `m_moving`, power-on homing, preset mapping, error-state clearing, `get_hsfw_status()` errors, no polling while powered off, and FAILURE when the telemetry thread is not running.
- **Shutdown, power and telemetry**: `appShutdown()` (`close_hsfw()` and `exit_hsfw()`), `onPowerOff()`, `whilePowerOff()`, `recordTelem()`, `checkRecordTimes()` and `recordStage(true)` (checked through `telem_stage::lastRecord`).

## Test harness

- The app header is included with `#define protected public` (flowRPM precedent) so the protected app, `MagAOXApp` power state, `stdMotionStage` and telemetry `logManager` state can be set and checked directly.
- `hsfwCtrl_test` subclass: resets the fake libhsfw state, sets `m_configName`, points telemetry at `/tmp/hsfwCtrl_test_telem` (removed at exit), and marks the telemetry log thread as running (`m_tel.m_logThreadRunning = true`) so `telemeter::appLogic()` does not force FAILURE. `setupWheel()` configures five filters and calls `stdMotionStage::appStartup()` to create the INDI properties; `power()` sets the power state and target; `connectWheel()` installs the fake wheel handle with power on.
- `stubs/libhsfw.h`: a stand-in for the Optec `libhsfw.h` declaring only the types and functions the app uses. The stub bodies are defined in `hsfwCtrl_test.cpp` and act on `g_hsfwStub`, which holds the enumerated wheel list, forced return codes, the reported status, call counts and captured arguments.
- Callback bodies run live (`testMacrosINDI.hpp` is not used), with device/name mismatch checked explicitly.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../hsfwCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/hsfwCtrl/tests/hsfwCtrl_test
../apps/hsfwCtrl/tests/hsfwCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` past the UNINITIALIZED check is not called because `telemeter::appStartup()` starts the telemetry log thread.
- The stub `libhsfw.h` is written from how the app uses the library (the real header is not available here), so field and parameter types may differ slightly from the real one.
- `moveTo()` rejects every position below 0.5: the wrap-around loop adds 8 until the value is at least 8.5 and then treats any value at least 8.5 as an error. The tests check the current behavior.
- On a successful connection the second `enumerate_wheels()` list (in NOTCONNECTED) is never freed; the tests check the current call counts.
- `hsfwCtrl::home()` is declared but not defined, so it is not tested.
