# zaberLowLevelBinary unit tests

Catch2 unit tests for the `zaberLowLevelBinary` MagAO-X application (`apps/zaberLowLevelBinary/zaberLowLevelBinary.hpp`).

## Test files

| File | Covers |
|------|--------|
| `zaberLowLevelBinary_test.cpp` | INDI callback validation, power-off snapshots, last-home refresh, stage discovery, and transport-error recovery |

## What is tested

- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `tgt_pos`, `req_home`, `req_home_all`, `req_halt`, `req_ehalt` and `knob_enable`.
- **Power off**: the INDI snapshot keeps the stage state read from the saved state file.
- **Homing**: `zaberBinaryStage::updateLastHomed()` refreshes the last-home time after homing completes.
- **Discovery**: `loadStages()` resets stale device addresses and finds devices that appear later.
- **Recovery**: `recoverFromError()` goes to `NOTCONNECTED` when the tty is present and `NODEVICE` when it is missing.

## Test harness

- `zaberLowLevelBinary_test` subclass sets up the callback properties, builds a temporary MagAO-X directory tree and driver FIFOs for power-off tests, and wraps stage configuration, discovery, recovery and FSM state.
- `zaberBinaryStage_test` subclass sets the homing fields and calls `updateLastHomed()`.
- `zb_serial.c` is included into the test inside `extern "C"`, so the test is one translation unit.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../zaberLowLevelBinary.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/zaberLowLevelBinary/tests/zaberLowLevelBinary_test
../apps/zaberLowLevelBinary/tests/zaberLowLevelBinary_test
```

The test is listed in `tests/tests.list`, so it also runs with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Serial I/O with real stages is not covered.
- The test creates driver FIFOs under `/tmp` and an `indiDriver` for the power-off test.
