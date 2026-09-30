# koolanceCtrl unit tests

Catch2 unit tests for the `koolanceCtrl` MagAO-X application (`apps/koolanceCtrl/koolanceCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `koolanceCtrl_test.cpp` | Configuration, protocol detection, status decoding, set-level commands and checksum, INDI callbacks, telemetry, and the `appLogic()` state machine |

## What is tested

- **Configuration**: defaults (9600 baud Koolance default, `telemeter.maxInterval` 10 s) and overrides of `usb.*` and `telemeter.maxInterval`.
- **Protocol detection**: `initialConnect()` sends `CF 01 08`, detects protocol 1 (43-byte reply, read-only `pump_level`/`fan_level` with no callbacks) or protocol 2 (51-byte reply, standard R/W numbers with callbacks registered), rejects other lengths, and closes the device and returns to NOTCONNECTED on no reply.
- **Status decoding**: `getStatus()` decodes liquid temperature (including the -200.0 C offset), flow rate, fan/pump RPM and levels for both protocols; write errors, timeouts and wrong-size replies close the device and return to NOTCONNECTED.
- **Set commands**: `setPumpLvl()`/`setFanLvl()` build the 51-byte protocol 2 command (0xAA disabled fields, preserved units, sum mod 100 checksum) and do nothing for protocol 1; write errors close the device.
- **INDI**: `newCallBack_m_indiP_pumplvl`/`newCallBack_m_indiP_fanlvl` reject a wrong property name, ignore out-of-range levels (pump 1-10, fan 0-100), use `target` over `current`, and send the set command.
- **Telemetry**: `recordCooler()` records only on change or when forced, `recordTelem()` forces a record, and `checkRecordTimes()` records only when `telem_cooler::lastRecord` is stale.
- **State machine**: `appStartup()` fails from UNINITIALIZED; `appLogic()` NODEVICE and NOTCONNECTED with unmatched USB IDs end in NODEVICE; READY reads and decodes the status.

## Test harness

- `koolanceCtrl_test` subclass: sets `m_configName` and the level property names, runs `setupConfig()`/`readConfig()`/`loadConfig()`, exposes the protected telemetry members, the level properties, and the registered new-callback map.
- `fakeTty`: a `socketpair()` whose app end is assigned to `m_fileDescrip`; replies are queued before the call and the app's commands are read back from the other end. Write errors use `m_fileDescrip = -1`.
- Callback bodies run live (`testMacrosINDI.hpp` is not used): the callbacks do not check the device, so `XWCTEST_INDI_NEW_CALLBACK` does not apply.
- Telemetry is observed through `telem_cooler::lastRecord`; the telemetry log thread is not started, so entries are queued and written when the app is destroyed, to `/tmp/koolanceCtrl_test_telem` (the harness points the telemetry logger there, and the directory is removed at exit).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../koolanceCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/koolanceCtrl/tests/koolanceCtrl_test
../apps/koolanceCtrl/tests/koolanceCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `initialConnect()` sleeps 1 s before reading and the timeout cases wait 1 s, so this test takes several seconds.
- `appStartup()` past the UNINITIALIZED check is not called because `telemeter::appStartup()` starts the telemetry log thread, which writes files.
- The NOTCONNECTED -> CONNECTED -> READY path is not tested because `connect()` opens a real tty.
- `setPumpLvl()`/`setFanLvl()` are not called with `m_protocolChars == 0` (before `initialConnect()`), which indexes an empty vector.
- The no-device `appLogic()` cases rely on `/sys/class/tty` being readable (true on normal Linux hosts).
