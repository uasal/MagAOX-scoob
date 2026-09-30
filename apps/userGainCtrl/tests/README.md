# userGainCtrl unit tests

Catch2 unit tests for the `userGainCtrl` MagAO-X application (`apps/userGainCtrl/userGainCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `userGainCtrl_test.cpp` | INDI callback validation and `blockModes()` block partitioning |

## What is tested

- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `zeroAll`, `singleModeNo`, `singleGain` and `singleMC`, and `XWCTEST_INDI_ARBNEW_CALLBACK` checks for the per-block gain, multiplicative-coefficient and limit callbacks.
- **Blocks**: `blockModes()` with no Zernikes, full and partial blocks, Zernikes in one, two or three blocks with and without splitting, and the full MagAO-X mode layout.

## Test harness

- `userGainCtrl_test` subclass sets up the callback properties for the INDI validation macros (`testMacrosINDI.hpp` included before the app header, so callback bodies do not run).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../userGainCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/userGainCtrl/tests/userGainCtrl_test
../apps/userGainCtrl/tests/userGainCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Gain updates to the shared-memory gain/coefficient streams are not covered.
