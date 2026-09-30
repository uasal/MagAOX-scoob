# zaberLowLevel unit tests

Catch2 unit tests for the `zaberLowLevel` MagAO-X application (`apps/zaberLowLevel/zaberLowLevel.hpp`), its `zaberStage` class and `zaberUtils.hpp` helpers.

## Test files

| File | Covers |
|------|--------|
| `zaberLowLevel_test.cpp` | INDI callback validation, power-off snapshots, stage discovery, and transport-error recovery |
| `zaberStage_test.cpp` | Classification of decoded ASCII messages and parsing of the warnings response |
| `zaberUtils_test.cpp` | `parseSystemSerial()` for valid and malformed `system.serial` responses |

## What is tested

- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `tgt_pos`, `req_home`, `req_home_all`, `req_halt`, `req_ehalt`, `knob_enable` and `led_enable`.
- **Power off**: the INDI snapshot keeps the stage state read from the saved state file.
- **Discovery**: `loadStages()` resets stale device addresses and finds devices that appear later.
- **Recovery**: `recoverFromError()` goes to `NOTCONNECTED` when the tty is present and `NODEVICE` when it is missing.
- **Stage messages**: replies, alerts and info messages for the same device, replies for another device, and warning lists of 0 to 10 flags including truncated responses.
- **Utilities**: parsing of addresses and serial numbers from `system.serial`, and rejection of malformed snapshots.

## Test harness

- `zaberLowLevel_test` subclass sets up the callback properties, builds a temporary MagAO-X directory tree and driver FIFOs for power-off tests, and wraps stage configuration, discovery, recovery and FSM state.
- `za_serial.c` is included into `zaberLowLevel_test.cpp` and `zaberStage_test.cpp` inside `extern "C"`, so each test is one translation unit.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../zaberLowLevel.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/zaberLowLevel/tests/zaberLowLevel_test
../apps/zaberLowLevel/tests/zaberLowLevel_test
```

Substitute any of the other test names (`zaberStage_test`, `zaberUtils_test`) for `zaberLowLevel_test`.

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Serial I/O with real stages is not covered.
- `zaberLowLevel_test.cpp` creates driver FIFOs under `/tmp` and an `indiDriver` for the power-off test.
