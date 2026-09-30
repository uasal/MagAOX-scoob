# pi335Ctrl unit tests

Catch2 unit tests for the `pi335Ctrl` MagAO-X application (`apps/pi335Ctrl/pi335Ctrl.hpp`), which controls a
PI E-727 controller with an S-335 (2-axis) or S-325 (3-axis) tip/tilt stage over a USB tty.

## Test files

| File | Covers |
|------|--------|
| `pi335Ctrl_test.cpp` | Configuration, controller queries and response parsing, identification, moves, homing, DM interface, INDI callbacks |

## What is tested

- **Construction and configuration**: defaults and overrides of `stage.*`, `dm.calibRelDir`, `dm.shmimName`,
  `usb.baud` and `device.*` timeouts via `setupConfig()`/`loadConfigImpl()`.
- **Queries and parsing**: `getCom()` command formatting, `getPos()`/`getSva()` `N=value` parsing, malformed
  responses and read timeouts.
- **Identification**: `testConnection()` for S-335 and S-325 stages (controller/stage strings, number of axes,
  ATZ usage, axis limits from `TMN?`/`TMX?`, flat command from the home positions, axis-3 INDI property) and its
  rejection of unknown controllers, unknown stages, bad limit responses and no response.
- **Moves**: `move_1()`, `move_2()`, `move_3()` command strings, set points and range checks.
- **Homing**: `home()`, `home_1()`..`home_3()` sequencing guards and `ATZ` commands, `homeState()` parsing,
  `finishInit()` guards, and `initDM()` turning servos off and starting homing.
- **DM interface**: `zeroDM()`, `commandDM()` (only when OPERATING, 2 and 3 axes, stops at an out-of-range axis),
  `releaseDM()`, and `updateFlat()`.
- **INDI callbacks** (live bodies): `pos_1`, `pos_2`, `pos_3` device/name checks, target vs current handling,
  READY (moves the stage) vs OPERATING (updates the flat only) behavior, and range rejection.

## Test harness

- `MagAOX::app::pi335Ctrl_test` (the class the app befriends) exposes protected members with using-declarations
  and private ones through accessors, builds the `pos_N` INDI properties and a 3x1 flat command, and wraps
  `setupConfig()`/`loadConfigImpl()` for config-file tests.
- `fakePI335` is a fake E-727 on the far end of a `socketpair()`. The app's `m_fileDescrip` is set to one end; a
  responder thread records each newline-terminated command and answers the ones with a scripted response, so
  query/response code paths run without a tty. Unscripted queries time out (the tests shorten `m_readTimeout`).
- `testMacrosINDI.hpp` is not used, because the callbacks do not use `INDI_VALIDATE_CALLBACK_PROPS`, and the
  axis-3 callback does not check the device name.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../pi335Ctrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/pi335Ctrl/tests/pi335Ctrl_test
../apps/pi335Ctrl/tests/pi335Ctrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The full `finishInit()` sequence is not run because it sleeps for more than 5 seconds; only its guards are tested.
- `appStartup()`, `appLogic()` and `appShutdown()` are not called: they start the DM, shmimMonitor and telemetry
  threads and, in `appLogic()`, look up and open the USB device.
- Telemetry (`recordPI335()`, `recordTelem()`, `checkRecordTimes()`) and `setFlat()` with a real flat stream are
  not covered.
