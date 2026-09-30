# mcp3008Ctrl unit tests

Catch2 unit tests for the `mcp3008Ctrl` MagAO-X application (`apps/mcp3008Ctrl/mcp3008Ctrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `mcp3008Ctrl_test.cpp` | Configuration, the MCP3008 SPI driver, frame-grabber acquisition hooks, INDI fps callbacks and telemetry |

## What is tested

- **Construction defaults**: channel count, target fps, trigger interval and gain.
- **Configuration**: defaults and overrides of the `fps.*` and `accel.numChannels` options, plus the
  `framegrabber.shmimName`/`circBuffLength` and `telemeter.maxInterval` base-class options.
- **MCP3008 driver** (`dependencies/MCP3008.cpp`, compiled into the test): `connect()`/`disconnect()` argument
  forwarding, idempotence and error exceptions, destructor close; `read()` command-byte encoding
  (start bit, SGL/DIFF bit, 3-bit channel mask) and 10-bit response decoding with the ignored bits set.
- **Frame grabber hooks**: `configureAcquisition()` frame geometry and data type, `startAcquisition()` start
  time, `acquireAndCheckValid()` channel readout, trigger adjustment, shutdown/reconfig early return and SPI
  failure propagation, `loadImageIntoStream()` copy, `reconfig()`.
- **INDI callbacks**: `fps` new-property callback (target, current-only, wrong name/device, empty property) and
  the `fpsSource` set-property callback (default and configured element, missing element, wrong device/name).
- **Telemetry**: `recordTelem()` and `checkRecordTimes()` for `telem_fgtimings`.

## Test harness

- `mcp3008Ctrl_test` subclass: sets the configuration name and initializes the `fps`/`fpsSource` INDI properties
  as `appStartup()` would. Protected members are reached via the `#define protected public` include precedent.
- INDI callback bodies are live (`testMacrosINDI.hpp` is not used), so device/name validation is checked
  explicitly.
- `stubs/lgpio.h`: stand-in for the lgpio SPI API (`lgSpiOpen`, `lgSpiClose`, `lgSpiXfer`). The stub bodies and
  their fake state (return codes, call counts, captured arguments and transmitted bytes, per-channel values) are
  defined in `mcp3008Ctrl_test.cpp`, so no lgpio library or SPI hardware is needed.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../mcp3008Ctrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/mcp3008Ctrl/tests/mcp3008Ctrl_test
../apps/mcp3008Ctrl/tests/mcp3008Ctrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called: they start the frame-grabber and telemetry
  threads and need elevated privileges and an INDI driver.
- The closed-loop trigger adjustment in `acquireAndCheckValid()` depends on wall-clock timing, so it is only
  checked against loose bounds.
