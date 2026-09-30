# filterWheelCtrl unit tests

Catch2 unit tests for the `filterWheelCtrl` MagAO-X application (`apps/filterWheelCtrl/filterWheelCtrl.hpp`), a
Faulhaber MCBL filter wheel controller built on `dev::stdMotionStage` and `dev::telemeter`.

## Test files

| File | Covers |
|------|--------|
| `filterWheelCtrl_test.cpp` | Configuration, preset/count conversion, MCBL command strings and response parsing, INDI callbacks (app and `stdMotionStage`), the `appLogic()` state machine including homing and preset moves, power-off hooks, and telemetry |

## What is tested

- **Configuration**: defaults (9600 baud, motor 100/50/3000, no presets) and overrides of `usb.*`, `timeouts.*`, `motor.*`, `stage.homeOffset`/`powerOnHome`/`homePreset`, `filters.names`/`filters.positions` (a 0 position becomes index + 1) and `telemeter.maxInterval`.
- **Conversions**: `presetNumber()` rounding and wrap-around; `moveTo()` filter-to-counts conversion (`homeOffset + circleSteps/nFilters*(filter-1)`, or the raw value when `circleSteps` is 0).
- **Commands**: `onPowerOnConnect()` (`ANSW0`, `MOTTYP2`, `AC`, `DEC`, `SP`), `home()` (`EN HA4 HL4 CAHOSEQ HP0 HOSP GOHOSEQ`, state HOMING), `stop()` (`DI` twice), `moveToRaw()` (`EN LA<n> M`), `moveToRawRelative()` (`EN LR<n> M`), `startHoming()`.
- **Responses**: `getPos()` (`POS`), `getMoving()` (`GN`, speed above 10% of the motor speed is moving, 2 while homing), `getSwitch()` (`GAST`), unparseable replies, and timeouts.
- **INDI**: `newCallBack_m_indiP_counts` (device/name validation, target over current, preset target update); the `stdMotionStage` preset, preset-name (unknown, multiple, and no selections), home and stop callbacks, and the static dispatcher.
- **State machine**: `appStartup()` from UNINITIALIZED; `appLogic()` INITIALIZED, POWERON -> NODEVICE, stale device name, ERROR while powered off, CONNECTED -> NOTHOMED or power-on homing, the 5-step homing sequence to READY with a home preset, the 3-step preset move to READY, READY -> OPERATING when moving, and NOTCONNECTED on no reply.
- **Power and telemetry**: `onPowerOff()`, `whilePowerOff()`, `recordPosition()` change detection, `recordTelem()` overloads, `checkRecordTimes()`.

## Test harness

- The app header is included with `#define protected public` (flowRPM precedent) so the protected app, `stdMotionStage` and telemetry `logManager` state can be set and checked directly.
- `filterWheelCtrl_test` subclass: sets `m_configName`, creates the `counts` property as `appStartup()` does, uses 200 ms timeouts, points telemetry at `/tmp/filterWheelCtrl_test_telem` (removed at exit), and marks the telemetry log thread as running (`m_tel.m_logThreadRunning = true`) so `telemeter::appLogic()` does not force FAILURE. `setupWheel()` configures six filters (6000 counts, offset 100) and calls `stdMotionStage::appStartup()` to create the preset, preset-name, home and stop properties.
- `fakeMCBL`: a `socketpair()` whose app end is assigned to `m_fileDescrip`. A responder thread records each `\r`-terminated command and answers `GAST`, `GN` and `POS` with configured replies (or stays silent).
- Callback bodies run live (`testMacrosINDI.hpp` is not used), with device/name mismatch checked explicitly.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../filterWheelCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/filterWheelCtrl/tests/filterWheelCtrl_test
../apps/filterWheelCtrl/tests/filterWheelCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` past the UNINITIALIZED check is not called because `telemeter::appStartup()` starts the telemetry log thread.
- The NOTCONNECTED -> CONNECTED path is not tested because `connect()` opens a real tty.
- `presetNumber()` loops forever with no presets configured, so every test that can reach it configures presets.
- `getSwitch()` never sets `m_switch` true because the reply still carries its `\r\n` terminator; the test checks the current behavior.
- The ERROR-state test sleeps 1 s, as the app does.
