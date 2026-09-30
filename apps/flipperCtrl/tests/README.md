# flipperCtrl unit tests

Catch2 unit tests for the `flipperCtrl` MagAO-X application (`apps/flipperCtrl/flipperCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `flipperCtrl_test.cpp` | Configuration, flipper command bytes and status parsing, telemetry recording, the position INDI callback, and the `appLogic()` state machine |

## What is tested

- **Construction**: default in/out/current/target positions, power management enabled, the `presetName` selection switch.
- **Configuration**: defaults (115200 baud MCBL default, 1000 ms timeouts, `in`=1/`out`=2) and overrides of `flipper.reverse` (swaps in/out), `usb.*`, `device.*` timeouts, and `telemeter.maxInterval`.
- **Commands**: `moveTo()` writes `6A 04 00 0p 50 01` for positions 1 and 2 and rejects anything else without writing; `getPos()` writes `80 04 00 00 50 01` and maps status byte 16 (1 -> position 1, anything else -> position 2).
- **Telemetry**: `recordStage()` records only on a position/moving change or when forced; `recordTelem()` forces a record; `checkRecordTimes()` records only when `telem_stage::lastRecord` is stale.
- **INDI**: `newCallBack_m_indiP_position` rejects a wrong property name and ignores requests with no switch on.
- **State machine**: `appLogic()` POWERON -> NODEVICE and NOTCONNECTED -> NODEVICE when the USB IDs match no device; CONNECTED sets the target to the read position; READY updates the current position.

## Test harness

- `flipperCtrl_test` subclass: sets `m_configName`, creates the `presetName` switch as `appStartup()` does, runs `setupConfig()`/`readConfig()`/`loadConfig()`, and exposes the protected position members.
- `fakeFlipper`: a `socketpair()` whose app end is assigned to `m_fileDescrip`. A responder thread reads each 6-byte command and answers with a 20-byte status message, so `tty::ttyWrite()`/`tty::ttyRead()` run unmodified without a serial device.
- Telemetry is observed through `telem_stage::lastRecord`; the telemetry log thread is not started, so entries are queued and written when the app is destroyed, to `/tmp/flipperCtrl_test_telem` (the harness points the telemetry logger there, and the directory is removed at exit).
- `testMacrosINDI.hpp` is not used: the callback does not check the device, and has no `INDI_VALIDATE_CALLBACK_PROPS`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../flipperCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/flipperCtrl/tests/flipperCtrl_test
../apps/flipperCtrl/tests/flipperCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not called because `telemeter::appStartup()` starts the telemetry log thread, which writes files.
- Position requests that turn a switch on are not tested: the callback calls `m_indiDriver->sendSetProperty()` unconditionally, and `m_indiDriver` is null in unit tests.
- The no-device `appLogic()` cases rely on `/sys/class/tty` being readable (true on normal Linux hosts).
- Because the telemetry thread is not running, `telemeter::appLogic()` sets FAILURE and `m_shutdown` at the end of the READY branch; the tests check positions, not the final state, in that branch.
