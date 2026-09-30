# hwpTracker unit tests

Catch2 unit tests for the `hwpTracker` MagAO-X application (`apps/hwpTracker/hwpTracker.hpp`).

## Test files

| File | Covers |
|------|--------|
| `hwpTracker_test.cpp` | INDI callback validation, configuration, HWP angle math, `appStartup()`, the INDI callbacks, and `appLogic()` |

## What is tested

- **INDI validation**: the `tracking` and `hwp_position` NEW callbacks and the `teldata`, stage `position` and stage `fsm` SET callbacks reject wrong device/property names.
- **Configuration**: defaults and overrides of the `hwp`, `tcs`, `tracking` and `telemeter.maxInterval` settings.
- **Angle math**: `getHwpStatus()` position names and tolerance, `getHwpTrackingOffset()` (`-0.5*parang + altitude + pupilOffset`), and `updateHwpPos()` stage targets (`zero + sign*(setPos + offset)`).
- **Startup**: `appStartup()` builds and registers every INDI property and sets `READY`.
- **Callbacks**: `teldata` sets altitude and parallactic angle; `hwp_position` stores the set angle and commands the stage; `tracking` applies and clears the offset; the stage `position` callback derives the actual and current HWP angles (with zero snapping and 0.01 deg rounding) and the position name; the stage `fsm` callback mirrors the stage state.
- **appLogic**: stage updates while tracking, the update interval, re-arming after a toggle, and the failure path when the telemetry thread is not running.

## Test harness

- `hwpTracker_test` subclass exposes the protected members with `using` declarations, builds the outbound stage property without a full startup, reads back the stage target, and inspects the registered callback maps.
- `startup()` points the telemetry logger at `/tmp/hwpTracker_test_telem` with a short write pause before calling `appStartup()`. The telemetry log level stays at INFO, so telemetry records are dropped and no file is written.
- `testMacrosINDI.hpp` is included after the app header, so the callback bodies run; the validation macros still pass because every callback returns 0 for a property with no elements.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../hwpTracker.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/hwpTracker/tests/hwpTracker_test
../apps/hwpTracker/tests/hwpTracker_test
```

The tests need a `../apps/hwpTracker/tests/hwpTracker_test` line in `tests/tests.list` to run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appLogic()` keeps its update timestamp in a function-local static, so the tests reset it by running once with tracking off.
- The content of telemetry records (`recordPolTrack()`) is not checked, since the records are dropped at the default log level.
