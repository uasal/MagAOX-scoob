# dmMode unit tests

Catch2 unit tests for the `dmMode` MagAO-X application (`apps/dmMode/dmMode.hpp`).

## Test files

| File | Covers |
|------|--------|
| `dmMode_test.cpp` | Configuration, mode-cube loading and truncation, DM channel validation in `appLogic()`, the amplitude INDI callbacks, and DM mode telemetry |

## What is tested

- **Configuration**: defaults (`maxModes = 50`, empty cube/channel/name), overrides of every `dm.*` key and `telemeter.maxInterval`, and `dm.name` defaulting to `dm.channelName`.
- **Startup**: a missing mode cube makes `appStartup()` fail; a synthetic FITS cube is loaded, truncated to `maxModes` (with the kept planes checked pixel by pixel) or kept whole for `maxModes <= 0` or larger than the cube; the amplitude vector, shape and `dm` INDI property are set up.
- **FSM**: `appLogic()` stays `NOTCONNECTED` when the channel does not exist or has too few semaphores, and returns -1 from `CONNECTED` for a channel with the wrong data type, row count or column count; in `READY` without a telemetry thread the app goes to `FAILURE` and requests shutdown.
- **Commands**: `sendCommand()` does nothing when the channel is not open.
- **INDI**: `current_amps` and `target_amps` reject the wrong property name, ignore unknown elements, and set only the named mode amplitudes.
- **Telemetry**: `recordDmModes()` records only on change or when forced, `recordTelem()` always records, and `checkRecordTimes()` respects `telemeter.maxInterval`.

## Test harness

- `dmMode_test` subclass exposes the protected members with using-declarations, sets the INDI property names, fills a synthetic mode cube like `appStartup()` does, and closes the DM channel image opened by `appLogic()`.
- To test the mode-cube loading in `appStartup()` without starting the telemetry thread, the harness registers a placeholder `target_amps` property first, so the app's own registration fails right after the cube is loaded.
- The DM channel tests create real shared-memory images with `MILK_SHM_DIR=/tmp/dmMode_test_shm`, which is removed afterwards.
- `testMacrosINDI.hpp` is not used: the callbacks check only the property name (not the device), so the standard wrong-device check would fail. The checks are written out explicitly instead.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dmMode.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dmMode/tests/dmMode_test
../apps/dmMode/tests/dmMode_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `sendCommand()` with an open channel (the mode-sum and write to the DM) is not tested: it calls `m_indiDriver->sendSetProperty()` without a null check, and there is no INDI driver in unit tests. For the same reason the successful `CONNECTED` to `READY` step of `appLogic()`, and callbacks that change amplitudes while the channel is open, are not tested.
- `dmMode::recordDmModes()` sizes a function-local `static` vector on its first call, so all tests that call it use the same number of modes.
