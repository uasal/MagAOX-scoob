# smc100ccCtrl unit tests

Catch2 unit tests for the `smc100ccCtrl` MagAO-X application (`apps/smc100ccCtrl/smc100ccCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `smc100ccCtrl_test.cpp` | Configuration, `appStartup()` error paths, command/response parsing, controller queries, error-code decoding, motion commands, presets, live INDI callbacks, `appLogic()` controller states, and telemetry |
| `smc100ccCtrl_indi_test.cpp` | Device/name validation of every INDI callback (validation mode) |

## What is tested

- **Construction and configuration**: constructor defaults (power management, 5 s power-on wait, no default preset positions), and defaults and overrides of `stage.homingOffset`, `stage.opDelta`, the `dev::stdMotionStage` options (`stage.powerOnHome`, `stage.homePreset`, `presets.names`/`positions`), `usb.baud` (default B57600), `device.readTimeout`/`writeTimeout`, and `telemeter.maxInterval`.
- **appStartup()**: registration of the `position` property, and the UNINITIALIZED and preset-count errors.
- **Command/response parsing**: `makeCom()`, and `splitResponse()` for one- and two-digit axes, empty values, CRLF trimming, letter-first replies, and short replies.
- **Controller queries**: `getCtrlState()` (valid replies, wrong axis/command/length, no reply, write failure), `getPosition()` (positive, negative, non-numeric, no reply), and `testConnection()`.
- **Error codes**: every `TE` code decoded by `getLastError()`, the no-error `@`, a truncated reply, and write failures (including the silent power-off case).
- **Motion**: `moveTo()` (the `PA` command, the `opDelta` threshold for OPERATING, controller errors, write failures), `stop()` (`ST`), `startHoming()` (`OR`), and `presetNumber()`.
- **INDI callbacks**: `position` (wrong device/name, ignored outside READY/OPERATING, moves in READY and OPERATING), and the `dev::stdMotionStage` `home`, `stop`, `presetName` and `preset` callbacks driving the SMC100CC commands.
- **appLogic()**: the INITIALIZED error, no state reply, NOT REFERENCED (with and without power-on homing), HOMING, MOVING below `opDelta`, READY after homing (move to the homing offset), and DISABLE (re-enable with `MM1`).
- **Telemetry**: `onPowerOff()`, `recordTelem()`, `recordPosition()` change detection, and `checkRecordTimes()`.

## Test harness

- `smc100ccCtrl_test.cpp` includes the app header with `#define protected public`.  The `smc100ccCtrl_test` subclass gives the INDI properties their device and names, points `m_fileDescrip` at a fake tty with short timeouts (`attach()`), or at an invalid descriptor so writes fail at once (`detach()`).
- `fakeSmc`: a fake controller on a `socketpair()`.  Replies are queued on the device side before the app sends its command, and everything the app writes is read back for comparison.  Each reply is queued just before the read that consumes it, since the tty reader takes all available bytes at once.
- `smc100ccCtrl_indi_test.cpp` includes `testMacrosINDI.hpp` before the app header, so the callbacks return right after validation, and uses `XWCTEST_INDI_NEW_CALLBACK`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../smc100ccCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/smc100ccCtrl/tests/smc100ccCtrl_test
../apps/smc100ccCtrl/tests/smc100ccCtrl_test
make -f Makefile.one t=../apps/smc100ccCtrl/tests/smc100ccCtrl_indi_test
../apps/smc100ccCtrl/tests/smc100ccCtrl_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The READY/OPERATING branch of `appLogic()` is not run: it calls `m_indiDriver->sendSetProperty()` without checking for a driver, which would crash without an INDI server.
- The CONFIGURATION state (`1x`) branch of `appLogic()` sleeps 15 s, and unknown states sleep 1 s, so they are not run.
- The POWERON, NODEVICE, NOTCONNECTED and ERROR branches look up and open the USB device, so they are not run.
- The full `appStartup()` is not run because it starts the telemetry log thread.
- The `position` callback with only a `current` element (or neither element) is not tested: its `-1e55` float sentinel overflows to -inf in a float, so the "missing" check never matches and the callback would move to -inf.
