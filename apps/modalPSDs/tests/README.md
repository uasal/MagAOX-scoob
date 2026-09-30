# modalPSDs unit tests

Catch2 unit tests for the `modalPSDs` MagAO-X application (`apps/modalPSDs/modalPSDs.hpp`).

## Test files

| File | Covers |
|------|--------|
| `modalPSDs_test.cpp` | INDI callbacks, PSD windowing, rolling mean/PSD sums, loop-state gating, and circular-buffer snapshots |

## What is tested

- **INDI callbacks**: device/name validation of psdTime, psdAvgTime, meanTime, and the fps-source and loop-state set callbacks.
- **Windows**: PSD averaging and mean windows are decoupled, and the PSD input windows use one validated snapshot and the exact required history depth.
- **Rolling sums**: the rolling mean and rolling PSD-sum updates match a full recompute.
- **Loop-state gating**: optional gating blocks open-loop frames.
- **Snapshots**: circular-buffer loads reject stale snapshots.

## Test harness

- `modalPSDs_test` subclass of `modalPSDs` that sets the INDI property device/names and exposes window sizes, mode counts, and circular-buffer snapshots.
- `tests/testMacrosINDI.hpp` is included before the app header (validation mode).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../modalPSDs.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/modalPSDs/tests/modalPSDs_test
../apps/modalPSDs/tests/modalPSDs_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The FFT/PSD worker thread and shared-memory streams are not run end to end.
