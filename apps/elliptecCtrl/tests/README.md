# elliptecCtrl unit tests

Catch2 unit tests for the `elliptecCtrl` MagAO-X application (`apps/elliptecCtrl/elliptecCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `elliptecCtrl_test.cpp` | Configuration, conversions, serial helpers, protocol parsing and formatting, motion/preset logic, live INDI callbacks, `appLogic()`, and telemetry |
| `elliptecCtrl_indi_test.cpp` | Device/name validation of every INDI callback (validation mode) |

## What is tested

- **Configuration**: defaults and overrides via `setupConfig()`/`loadConfig()`, including the address nibble, the busy-timeout floor, velocity and poll-miss clamping, and presets mirrored from `dev::stdMotionStage` (with its default positions).
- **Conversions**: `degToPulses_()`/`pulsesToDeg_()` with and without a pulses-per-revolution value, `frame_()`, and `to_termios_baud_()`.
- **Serial helpers**: `writeAll_()`, `readFrame_()` (CRLF frames, soft timeout), `drainInput_()`, `txrx_()` (including the busy timeout), `openPort_()`/`closePort_()` failure paths.
- **Protocol parsing**: `q_position_()` signed hex decoding and bad-reply handling, `q_status_()` status byte decoding, `q_info_()` pulses-per-revolution discovery.
- **Command formatting**: `ma`/`mr` 32-bit hex encoding, `sv` velocity clamping, `ho`, `st`, and the configurable `us`/`om` aliases, and the pending state each sets.
- **Motion logic**: absolute-angle wrapping, relative moves, `presetNumber()`, `moveTo()` (preset and degree), `buildStageNamePosText_()`, `stop()`, `startHoming()`, and the `pollDevice_()` resolution of pending commands, the home offset, and the soft-miss budget.
- **INDI callbacks**: `absDeg`, `relDeg`, `velocity`, `relMove`, `optimize`, `save`, `home`, `stop`, and `stageGoto`, checking the commands sent and the resulting state, plus rejection of wrong devices/names and missing targets.
- **appLogic**: the `INITIALIZED` error, the power-off early return, a missing port, and the full connection sequence (`in`, `gp`, `gs`, `sv`, `gs`, then a poll) on a pseudo-terminal.
- **Telemetry**: `recordStage()`, `recordTelem()` and `checkRecordTimes()` updating the `telem_stage` record time.

## Test harness

- `elliptecCtrl_test.cpp` includes the app header with `#define protected public`.  The `elliptecCtrl_test` subclass gives the INDI properties their device and names so the live callbacks accept requests.
- `fakeElliptec`: a fake device on a `socketpair()` (the app side is given to the app as `m_fd`) or on a pseudo-terminal (the app opens the slave by path in `openPort_()`).  A responder thread records each command and answers from a table of canned replies.
- `elliptecCtrl_indi_test.cpp` includes `testMacrosINDI.hpp` before the app header, so the callbacks return right after validation, and uses `XWCTEST_INDI_ARBNEW_CALLBACK` because the callbacks are named `newCallBack_m_ip*` rather than `newCallBack_m_indiP_*`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../elliptecCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/elliptecCtrl/tests/elliptecCtrl_test
../apps/elliptecCtrl/tests/elliptecCtrl_test
make -f Makefile.one t=../apps/elliptecCtrl/tests/elliptecCtrl_indi_test
../apps/elliptecCtrl/tests/elliptecCtrl_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not called because it starts the telemetry log thread, so the INDI properties are not created by the app and `updateStatus_()` (which returns early without an INDI driver) is not checked.
- The connection test needs a pseudo-terminal (`posix_openpt()`); it is skipped with a warning when none is available.
