# aguc8Ctrl unit tests

Catch2 unit tests for the `aguc8Ctrl` MagAO-X application (`apps/aguc8Ctrl/aguc8Ctrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `aguc8Ctrl_test.cpp` | Configuration, channel parsing, INDI callback validation, channel counts files, serial command strings, `appLogic()` states, and telemetry |

## What is tested

- **Configuration**: `loadConfigImpl()` return codes for no motor sections, sections without a `channel` keyword, out-of-range channels, and the `device.nChannels` override; the full `loadConfig()` loading the USB and `dev::ioDevice` settings and setting `m_shutdown` on a missing baud rate or a bad channel.
- **INDI callbacks**: `newCallBack_pos()`, `newCallBack_stepSize()` and `newCallBack_presetName()` (and their static wrappers) rejecting property names without the expected suffix, unknown channels, and multiple preset selections, and finding the configured channels.
- **Channel counts**: `writeChannelCounts()`/`readChannelCounts()` round trips in a temporary system directory, the missing-file default, and the unwritable-directory error.
- **Serial I/O**: `writeQuery()`, `writeReadError()` (including the `TE` error query and its read timeout), `Read()`, and the `CC`/`SU+`/`SU-` sequence sent by `setStepSize()`.
- **appLogic**: `INITIALIZED` to `NOTCONNECTED` when no device can be opened, `CONNECTED` to `READY` after the `MR` remote-mode command (or `ERROR` without a reply), and the `READY` poll sending `<axis>TS` for each channel and storing the channel counts.
- **Telemetry**: `recordAGUC8()`, `recordTelem()` and `checkRecordTimes()` updating the `telem_pico` record time.

## Test harness

- `aguc8Ctrl_test` subclass: sets `m_configName`, and exposes the configurator, `m_sysPath`, and the `dev::ioDevice` timeouts.
- `fakeAguc8Port`: a `socketpair()` whose app side is given to the app as `m_fileDescrip`.  A responder thread records every CRLF-terminated command and answers from a table of canned replies, so the tests check the exact command strings without a tty.
- The app header uses `sysPath` as a data member in `readChannelCounts()`/`writeChannelCounts()`, which does not compile against `MagAOXApp` (where `sysPath()` is a function and the member is `m_sysPath`).  The test includes libMagAOX first and then includes the app header with `#define sysPath m_sysPath`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../aguc8Ctrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/aguc8Ctrl/tests/aguc8Ctrl_test
../apps/aguc8Ctrl/tests/aguc8Ctrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `writeReadError()` sleeps for one second per command, so the serial tests take about ten seconds.
- `appStartup()` is not called: it starts one thread per channel and the telemetry log thread.  Because of that the INDI properties of each channel are never created, and the move paths of the callbacks (which signal the channel thread with `pthread_kill()`) and `channelThreadExec()` are not covered.
- The channel map and per-channel state are private with no `friend` declaration, so channel parsing is checked through the callbacks' behaviour rather than directly.
- The valid `loadConfig()` case relies on `/sys/class/tty` existing, so the USB device lookup reports "no device" rather than an error.
