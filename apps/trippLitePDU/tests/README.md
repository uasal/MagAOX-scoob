# trippLitePDU unit tests

Catch2 unit tests for the `trippLitePDU` MagAO-X application (`apps/trippLitePDU/trippLitePDU.hpp`).

## Test files

| File | Covers |
|------|--------|
| `trippLitePDU_test.cpp` | Configuration, `devstatus` response parsing, outlet and channel control, the channel INDI callback, `appStartup()` and the `appLogic()` state machine, all against the built-in simulator |

## What is tested

- **Construction**: 8 outlets, 1-based outlet numbering, 5 s channel state delay, 2 s loop pause.
- **Configuration**: defaults and overrides of `device.*` (address, port, username, passfile, powerAlertVersion, read/write timeouts), all `limits.*` thresholds, and a `dev::outletController` channel section.
- **Status parsing**: `parsePDUStatus()` on complete responses (no outlets, some, all, CRLF endings, unlisted outlets turned off, out-of-range outlet numbers ignored, no outlets line, skipped header/prompt lines, first line always skipped, empty response) and every error code (-1 to -13), including that values parsed before an error are kept.
- **Device interface**: `devConnect()`, `devLogin()`, `devPostLogin()` and `devStatus()` in simulation, with the simulator output parsed back.
- **Outlet control**: `turnOutletOn()`/`turnOutletOff()` change the simulated outlets, reject invalid outlets and release `m_indiMutex`; `updateOutletState()` updates all outlets through `updateOutletStates()`.
- **Channels**: `turnChannelOn()`/`turnChannelOff()` with multi-outlet channels and intermediate states, and the per-channel 5 s state delay that silently ignores a second change.
- **INDI**: `newCallBack_channels()` rejected when not READY; case-insensitive `target`, fallback to `state`, unknown targets ignored.
- **Startup**: `appStartup()` registers `status`, `load` and the outletController properties and goes to NOTCONNECTED.
- **State machine**: `appLogic()` goes NOTCONNECTED -> CONNECTED -> LOGGEDIN -> READY in one pass and polls the status; skips the poll while `m_indiMutex` is held; unhandled states fail. `updateAlarmsAndWarnings()` is run in every threshold band.

## Test harness

- The test defines `XWC_SIM_MODE` before including `../trippLitePDU.hpp`, so the app uses `trippLitePDU_simulator` (public `m_simulator`) instead of a telnet connection. The tests set the simulated outlets, voltage, frequency and current directly.
- `trippLitePDU_test` subclass: sets `m_configName`, runs `setupConfig()`/`readConfig()`/`loadConfig()`, and exposes the protected members with `using` declarations.
- Callback bodies run live (`testMacrosINDI.hpp` is not used): the only new callback is the outletController `newCallBack_channels()`, which does not validate the device.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../trippLitePDU.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/trippLitePDU/tests/trippLitePDU_test
../apps/trippLitePDU/tests/trippLitePDU_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The telnet (non-simulator) paths of `devConnect()`, `devLogin()`, `devPostLogin()`, `devStatus()` and `turnOutletOn()`/`turnOutletOff()` are not compiled in this test; they need a real PDU.
- `updateAlarmsAndWarnings()` only logs, so its thresholds are exercised but the chosen log priorities are not checked.
- `parsePDUStatus()` is not given short `I`/`O` lines (fewer than 8 characters), which index past the end of the line.
