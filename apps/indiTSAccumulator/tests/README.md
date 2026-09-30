# indiTSAccumulator unit tests

Catch2 unit tests for the `indiTSAccumulator` MagAO-X application (`apps/indiTSAccumulator/indiTSAccumulator.hpp`).

## Test files

| File | Covers |
|------|--------|
| `indiTSAccumulator_test.cpp` | Parsing of the `elements` list, and time-series accumulation in `setCallBack_all()` |

## What is tested

- **Configuration**: `device.property.element` entries are grouped by `device.property`, element names keep any further dots, and an empty list or an entry with fewer than two dots returns an error and sets `m_shutdown`.
- **Accumulation**: a Number update writes the value, acquisition time, write time and counters into the next slice of the element's circular buffer; repeated timestamps are ignored; the buffer rolls over at its depth; only the elements present in the update are written.
- **Rejection**: non-Number properties return an error, unconfigured properties and elements are ignored, and elements without a stream are skipped.
- **Static wrapper**: `st_setCallBack_all()` forwards to `setCallBack_all()`.
- **Lifecycle**: `appLogic()` and `appShutdown()` succeed.

## Test harness

- `indiTSAccumulator_test` subclass exposes the protected members with `using` declarations and wraps `setupConfig()`/`loadConfigImpl()`.
- `fakeStream` is an in-process `IMAGE` with heap-backed data, counter and time arrays and no semaphores (so `ImageStreamIO_sempost()` does nothing). The harness attaches fake streams to elements in place of the shared memory `appStartup()` would create.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../indiTSAccumulator.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/indiTSAccumulator/tests/indiTSAccumulator_test
../apps/indiTSAccumulator/tests/indiTSAccumulator_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not tested: it creates real ImageStreamIO shared-memory streams (and never frees them), which unit tests should not do.
