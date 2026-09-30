# sysMonitor unit tests

Catch2 unit tests for the `sysMonitor` MagAO-X application (`apps/sysMonitor/sysMonitor.hpp`).

## Test files

| File | Covers |
|------|--------|
| `sysMonitor_test.cpp` | Parsers for CPU temperature, CPU load, disk temperature, disk usage and RAM usage, and INDI callback validation |

## What is tested

- **Parsers**: `parseCPUTemperatures()`, `parseCPULoads()`, `parseDiskTemperature()`, `parseDiskUsage()` and `parseRamUsage()` with valid lines (including hard drive/SSD and `/`, `/data`, `/boot` variants), blank lines, wrong lines and corrupted lines.
- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `setlat`.

## Test harness

- The parser tests use a default-constructed `sysMonitor`.
- `sysMonitor_test` subclass sets up the `setlat` property for the INDI validation macro (`testMacrosINDI.hpp` included before the app header, so callback bodies do not run).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../sysMonitor.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/sysMonitor/tests/sysMonitor_test
../apps/sysMonitor/tests/sysMonitor_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The commands that produce the parsed output (`sensors`, `mpstat`, `hddtemp`, `df`, `free`) are not run.
- A test comment notes that CPU-temperature lines with leading whitespace are rejected.
