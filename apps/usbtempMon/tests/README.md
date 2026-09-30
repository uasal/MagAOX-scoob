# usbtempMon unit tests

Catch2 unit tests for the `usbtempMon` MagAO-X application (`apps/usbtempMon/usbtempMon.hpp`) and its DS18B20
USB 1-wire driver (`apps/usbtempMon/usbtemp.c`, `linux.c`).

## Test files

| File | Covers |
|------|--------|
| `usbtempMon_test.cpp` | Configuration and probe parsing, the DS18B20 driver (CRC, ROM, temperature conversion, commands, errors) against a fake sensor, telemetry, `checkConnections()` and `appLogic()` |

## What is tested

- **Configuration**: default thresholds (40/50/55 C) and USB IDs (`067b:2303`), overrides of `temp.*`, `usb.*` and `telemeter.maxInterval`; probe sections (serial lower-cased, location defaulting to the section name, sections without a serial skipped, probes sorted by location); a config with no probe sections sets `m_shutdown` and fails.
- **CRC**: `lsb_crc8()` against the Maxim AN27 ROM example and the CRC-8/MAXIM check value.
- **Driver**: `DS18B20_open()` on a missing path; `DS18B20_rom()` reads the ROM (formatted as `checkConnections()` does) and detects a bad CRC or a missing sensor; `DS18B20_acquire()` converts the scratchpad word to degrees C for several values and rejects missing sensors, bad CRC and a non-DS18B20 configuration byte; `DS18B20_measure()` and `DS18B20_setprecision()` send the expected 1-wire commands.
- **Telemetry**: `recordTemps()` records on the first call, on a change, when forced, and when a probe is added; `recordTelem()` forces a record; `checkRecordTimes()` records only when `telem_temps::lastRecord` is stale.
- **Connections and state machine**: `checkConnections()` sets NOTCONNECTED when no adapter matches; `appLogic()` reads a connected probe and starts a new conversion, closes and clears a probe that stops answering, and calls `checkConnections()` for unconnected probes.

## Test harness

- `usbtemp.c` (which includes `linux.c`) is `#include`d into the test inside `extern "C"`, since the app Makefile builds it as a separate object (`OTHER_OBJS = usbtemp.o`) and the test must be one translation unit.
- `usbtempMon_test` subclass: sets `m_configName`, runs `setupConfig()`/`readConfig()`/`loadConfig()` (or `loadConfigImpl()` to check its return value), and exposes the thresholds, probes, and `recordTemps()`.
- `fakeDS18B20`: the master side of a pseudo-terminal (`posix_openpt()`). The driver opens the pty slave with `DS18B20_open()`, so the real termios and 1-wire bit-slot code runs. A responder thread answers the reset pulse (0xF0) with a presence byte (or no-presence), echoes write slots, and answers read slots with the ROM (after 0x33) or scratchpad (after 0xCC 0xBE), and records the command bytes.
- USB IDs `fffe:fffd` are used wherever `checkConnections()` may run, so no real adapter is ever opened.
- Telemetry is observed through `telem_temps::lastRecord`; the telemetry log thread is not started, so entries are queued and written when the app is destroyed, to `/tmp/usbtempMon_test_telem` (the harness points the telemetry logger there, and the directory is removed at exit).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../usbtempMon.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/usbtempMon/tests/usbtempMon_test
../apps/usbtempMon/tests/usbtempMon_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not called because `telemeter::appStartup()` starts the telemetry log thread, which writes files.
- `checkConnections()` matching of real adapters is not tested: it enumerates `/sys/class/tty/ttyUSB*` through udev, which cannot be pointed at a pseudo-terminal.
- Negative temperatures are not tested: `DS18B20_acquire()` negates an `unsigned short`, so negative readings convert to large positive values (see the report).
- The warning/alert/emergency log levels in `appLogic()` are not checked (log contents are not inspected).
- The pty tests need `/dev/ptmx`, which is available on normal Linux hosts and containers.
- Because the telemetry thread is not running, `telemeter::appLogic()` sets FAILURE and `m_shutdown` at the end of each `appLogic()` call.
