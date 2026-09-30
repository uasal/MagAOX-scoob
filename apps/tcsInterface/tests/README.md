# tcsInterface unit tests

Catch2 unit tests for the `tcsInterface` MagAO-X application (`apps/tcsInterface/tcsInterface.hpp`).

## Test files

| File | Covers |
|------|--------|
| `tcsInterface_test.cpp` | INDI callback validation and `parse_xms()` time parsing |

## What is tested

- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `pyrNudge`, `acqFromGuider`, `labMode` and the tip/tilt and focus offload properties (`offlTT*`, `offlF*`).
- **Parsing**: `parse_xms()` for positive/negative and integer/decimal `x:m:s` strings, and rejection of empty strings, missing or misplaced `:` separators and invalid `x`, `m` or `s` fields.

## Test harness

- `tcsInterface_test` subclass sets up the callback properties for the INDI validation macros (`testMacrosINDI.hpp` included before the app header, so callback bodies do not run).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../tcsInterface.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/tcsInterface/tests/tcsInterface_test
../apps/tcsInterface/tests/tcsInterface_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Communication with the telescope control system is not covered.
- The `teldata` set-callback check is commented out in the test.
