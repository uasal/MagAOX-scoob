# zaberCtrl unit tests

Catch2 unit tests for the `zaberCtrl` MagAO-X application (`apps/zaberCtrl/zaberCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `zaberCtrl_test.cpp` | INDI callback validation, power-off telemetry, homing transitions, and preset-name aliases |

## What is tested

- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `pos`, `rawPos`, `preset`, `presetName`, `home` and `stop`, and `XWCTEST_INDI_SET_CALLBACK` checks for the low-level stage properties (`curr_state`, `max_pos`, `curr_pos`, `tgt_pos`, `temp`, `parked`).
- **Power off**: telemetry synced for parked and unparked stages.
- **Homing**: `READY` after homing moves the FSM on promptly, with and without a pending post-home preset move.
- **Preset aliases**: the selected alias for a shared preset position is tracked while moving, reported in telemetry, and cleared on power off.

## Test harness

- `zaberCtrl_test` subclass sets up the callback properties and provides setters for position, presets, parked/moving state and home preset, plus wrappers for the power-off, telemetry-sync, stage-state and alias-resolution code.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../zaberCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/zaberCtrl/tests/zaberCtrl_test
../apps/zaberCtrl/tests/zaberCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Communication with `zaberLowLevel` through a running INDI server is not covered.
