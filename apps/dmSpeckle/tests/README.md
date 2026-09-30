# dmSpeckle unit tests

Catch2 unit tests for the `dmSpeckle` MagAO-X application (`apps/dmSpeckle/dmSpeckle.hpp`).

## Test files

| File | Covers |
|------|--------|
| `dmSpeckle_test.cpp` | Configuration, speckle pattern generation (sparkle and arbitrary-cube modes), DM channel connection, INDI callbacks, the free-running modulator, and speckle telemetry |

## What is tested

- **Configuration**: defaults, overrides of every `dm.*` and `modulator.*` key (including the `trigger`/`cross` flags and the `arbcube` mode), the DM name following the channel name, and an unknown `opMode` falling back to sparkle.
- **Sparkle patterns**: `generateSpeckles()` builds four planes (+cos, -cos, +sin, -sin) scaled by the amplitude, checked pixel by pixel against the Fourier-mode formula; the angle is measured from `angleOffset` (a 90 degree change moves the speckles to the other axis); `cross` adds the 90 degree rotated pair; `single` zeroes all but the selected plane.
- **Arbitrary cube**: a FITS cube of the DM size is loaded and scaled by the amplitude; a wrong-size or missing cube is rejected.
- **DM channel**: `appLogic()` connects to a float channel and records its size and type, rejects a double channel, stays disconnected for a missing channel, and is blocked by a trigger channel with too few semaphores.
- **INDI**: `delay`, `separation`, `angle`, `amp`, `frequency`, `dwell` and `single` set their values and flag a pattern restart, and ignore missing or invalid values (negative frequency, zero dwell, `single` outside -1..3); `trigger`, `cross` and `modulating` switch their flags; `zero` is refused while modulating and otherwise clears the DM channel; wrong property names are rejected; `cross`, `dwell` and `single` also pass the `XWCTEST_INDI_NEW_CALLBACK` device/name checks.
- **Modulator**: with no trigger channel the modulator switches to free-running mode, writes the speckle planes to the DM channel at the set frequency, and zeroes the channel when modulation stops.
- **Telemetry**: `recordDmSpeck()` records only on a change (amplitude, modulating, cross, ...) or when forced; `recordTelem()` always records; `checkRecordTimes()` respects the telemetry interval.

## Test harness

- `dmSpeckle_test` subclass exposes the protected members with using-declarations, sets the INDI property keys, runs the modulator thread body (`modThreadExec()`) on demand, and closes the DM channel image opened by `appLogic()`.
- `testMacrosINDI.hpp` is included after the app header, so callback bodies are live. Most callbacks check only the property name (not the device), so the standard validation macro is used only for `cross`, `dwell` and `single`.
- The DM channel tests create real shared-memory images with `MILK_SHM_DIR=/tmp/dmSpeckle_test_shm`, which is removed afterwards. `generateSpeckles()` writes `/tmp/specks.fits`, which the tests remove.
- There is no telemetry thread in unit tests, so each `appLogic()` call ends in the `FAILURE` state with shutdown requested; the modulator test resets these before running the modulator.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dmSpeckle.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dmSpeckle/tests/dmSpeckle_test
../apps/dmSpeckle/tests/dmSpeckle_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Triggered modulation (waiting on a camera semaphore, with a trigger delay) is not tested.
- `appStartup()` is not run: it starts the modulator and telemetry threads with real-time priority.
