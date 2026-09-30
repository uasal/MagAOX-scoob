# mcp3208Ctrl unit tests

Catch2 unit tests for the `mcp3208Ctrl` MagAO-X application (`apps/mcp3208Ctrl/mcp3208Ctrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `mcp3208Ctrl_test.cpp` | Configuration, INDI callbacks, timer and synchronized acquisition modes, trigger-timing and delay-controller math, and frame loading |

## What is tested

- **Configuration**: defaults and overrides of the synchronized-mode settings, and clamping of the synchronized alpha.
- **INDI callbacks**: fps, fps source, alpha (with clamping), and synchroDelay (signed offsets) callbacks update the trigger metadata.
- **Timing diagnostics**: published loop metrics, mode transitions, wrapped phase error, lock thresholds, and nanosecond/`timespec` conversions.
- **updateTriggerTiming**: measured semaphore period, positive and negative synchroDelay offsets with wrap, EMA-period fallback when fps is invalid, modulo wrapping, configurable timing constants, and the non-positive-period guard.
- **Timer mode**: `configureAcquisition()` sizes the output frame, configured channels are read, and the trigger interval initializes then measures.
- **Frames**: `loadImageIntoStream()` copies the current values and updates the frame-mapping counters.
- **Synchronized mode**: reads on semaphore wake, producer cadence from stream metadata, read-latency and service-time EMAs, the delay controller (clamp to zero, anti-windup at the cap), stale-stream detection, reconfig requests on timeout, and clearing the cached synchronization state on reconfig.

## Test harness

- The app header is included with `#define protected public`; `mcp3208Ctrl_test` adds helpers on top.
- The `MCP3208Lib::MCP3208` device class (`../dependencies/MCP3208.h`) is stubbed in the test source, with an `mcp3208StubState` that sets the channel values and records the read order and connect calls, so no SPI device is needed.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../mcp3208Ctrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/mcp3208Ctrl/tests/mcp3208Ctrl_test
../apps/mcp3208Ctrl/tests/mcp3208Ctrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No SPI hardware is used, and real-time scheduling and long-running acquisition threads are not exercised.
