# cacaoInterface unit tests

Catch2 unit tests for the `cacaoInterface` MagAO-X application (`apps/cacaoInterface/cacaoInterface.hpp`).

## Test files

| File | Covers |
|------|--------|
| `cacaoInterface_test.cpp` | INDI callback validation, configuration, the CACAO fpsCTRL command/reply helpers, gain/mult/limit and loop commands, and the INDI callbacks |

## What is tested

- **INDI validation**: the `loop_state`, `loop_gain`, `loop_zero`, `loop_multcoeff` and `loop_max_limit` callbacks reject wrong device/property names.
- **Configuration**: `loop.number` and `telemeter.maxInterval` defaults and overrides; `appStartup()` fails without a loop number.
- **FPS commands**: `setFPSVal()` writes `setval <fps>-<loop>.<param> <value>` (string and numeric values), and fails without a FIFO.
- **FPS replies**: `getFPSValStr()`/`getFPSValNum()` send `fwrval` and parse the value from the reply file (value after the last separating space; the numeric form keeps the leading space), and handle empty replies, badly formatted replies and a missing FIFO.
- **Loop controls**: `setGain()`, `setMultCoeff()`, `setMaxLim()`, `loopOn()`, `loopOff()` and `loopZero()` send the expected commands and fail without a FIFO.
- **Callbacks**: the loop state toggle (on, off, unknown state, missing element), the zero request, and the gain/mult/limit callbacks (target, fallback to current, no usable value, failed send).
- **Other**: `checkLoopProcesses()`, `getAOCalib()` with no calibration source, and the telemetry hooks.

## Test harness

- `cacaoInterface_test` subclass exposes the protected members with `using` declarations.
- The CACAO fpsCTRL FIFO is replaced by a regular file (`/tmp/cacaoInterface_test_fpsCTRL.fifo`) so the commands written by the app can be read back. CACAO replies are faked by writing the reply file under `/dev/shm` before calling `getFPSValStr()`/`getFPSValNum()`; the app removes it after reading.
- The telemetry log level is left at INFO (and reset after `loadConfig()`), so telemetry records are dropped and no file is written.
- `testMacrosINDI.hpp` is included after the app header, so the callback bodies run; the validation macros still pass because every callback returns 0 for a property with no elements.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../cacaoInterface.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/cacaoInterface/tests/cacaoInterface_test
../apps/cacaoInterface/tests/cacaoInterface_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- A full `appStartup()`, `appLogic()` and the file-monitoring thread (`fmThreadExec()`) are not tested: they start long-running threads, and `appLogic()` calls `pthread_tryjoin_np()` on a thread that is never started in a unit test.
- `getAOCalib()` reads fixed `/milk/shm` paths, so only the missing-source path is tested.
- The `getFPSVal` tests need `/dev/shm`.
