# flowRPM unit tests

Catch2 unit tests for the `flowRPM` MagAO-X application (`apps/flowRPM/flowRPM.hpp`).

## Test files

| File | Covers |
|------|--------|
| `flowRPM_test.cpp` | Configuration, flow/RPM log parsing helpers, file reading, `appStartup()`/`appShutdown()`, `appLogic()`, and display-state reconciliation |

## What is tested

- **Configuration**: defaults and overrides via `setupConfig()`/`loadConfig()`, and `m_shutdown` set on a configuration failure.
- **Parsing helpers**: token parsing, logical-line splitting (CRLF, blank lines), the invalid default `parseResult`, timestamp parsing, record-line parsing of every branch, and whole-file parsing.
- **File reading**: reading and parsing the configured data files from `/tmp`.
- **Lifecycle**: `appStartup()` initializes the state and published status, `appShutdown()` completes cleanly.
- **appLogic**: end-to-end display-state updates, error returns when internal steps fail, per-key log back-off, and `recordTelem()` refreshing the telemetry bookkeeping.
- **Display state**: reconciliation of the displayed state and the `statusKey()` mapping of parse statuses.

## Test harness

- The app header is included with `#define protected public`.
- `flowRPMLoadConfigFailure` forces a configuration failure so the inherited `loadConfig()` sets `m_shutdown`.
- `flowRPMFaultInject` overrides the virtual steps of the `appLogic()` call chain to force failures at chosen sites.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../flowRPM.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/flowRPM/tests/flowRPM_test
../apps/flowRPM/tests/flowRPM_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The tests read files written under `/tmp`, not the real sensor logs.
