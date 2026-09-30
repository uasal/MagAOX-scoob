# rhusbMon unit tests

Catch2 unit tests for the `rhusbMon` MagAO-X application (`apps/rhusbMon/rhusbMon.hpp`).

## Test files

| File | Covers |
|------|--------|
| `rhusbMon_test.cpp` | Configuration, `readProbe()` against a fake probe, error handling, `appLogic()`, and telemetry |
| `rhusbMonParsers_test.cpp` | The `RH::parseC()` and `RH::parseH()` reply parsers in `rhusbMonParsers.hpp` |

## What is tested

- **Configuration**: defaults and overrides of the temperature and humidity limits, and of the `usb`, `device` and
  `telemeter` base class sections, via `setupConfig()`/`loadConfig()`.
- **Probe reads**: `readProbe()` sends `C` and `H` to a fake probe and parses positive and negative temperatures and
  the humidity.
- **Error handling**: write timeouts, missing replies (read timeouts), and every temperature and humidity parse error,
  checking the return value and the `-999` values left behind.
- **appLogic**: a successful read in `CONNECTED`, a failed read setting `ERROR`, and the warning/alert/emergency limit
  checks.  With no telemetry thread in the unit tests, the `telemeter` step reports `FAILURE` and sets `m_shutdown`.
- **Telemetry**: `recordTelem()`, `recordRH()` recording only changed values unless forced, and `checkRecordTimes()`
  recording only after the maximum interval (observed through `telem_rhusb::lastRecord`).
- **Parsers**: valid and malformed replies to the `C` and `H` commands.

## Test harness

- `rhusbMon_test` subclass sets the device name, exposes the protected members with `using` declarations, wraps the
  configuration steps, and creates the temperature and humidity properties as `appStartup()` does.
- `fakeProbe` is the fault-injected I/O: the app's `m_fileDescrip` is one end of a `socketpair()`, and a thread on the
  other end answers `C` and `H` with configurable replies (an empty reply is never sent, causing a read timeout).
  A write failure is injected with a descriptor of -1, which makes the write poll time out.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../rhusbMon.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/rhusbMon/tests/rhusbMon_test
../apps/rhusbMon/tests/rhusbMon_test
make -f Makefile.one t=../apps/rhusbMon/tests/rhusbMonParsers_test
../apps/rhusbMon/tests/rhusbMonParsers_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` and `connect()` are not run: they start the telemetry log thread and search udev for the USB probe.
- The INDI property states set by the limit checks are not observable, since `updateIfChanged()` does nothing
  without an INDI driver.
