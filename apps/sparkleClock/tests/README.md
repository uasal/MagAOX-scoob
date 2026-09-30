# sparkleClock unit tests

Catch2 unit tests for the `sparkleClock` MagAO-X application (`apps/sparkleClock/sparkleClock.hpp`).

## Test files

| File | Covers |
|------|--------|
| `sparkleClock_test.cpp` | Configuration, sparkle-clock pattern generation, connected-stream checks in `appLogic()`, and all INDI NEW callbacks |

## What is tested

- **Configuration**: defaults and overrides of the `dm.*` keys (channel, trigger channel/semaphore/mode/delay, angle,
  angle offset, amplitude, cross, frequency, dwell) and the `modulator.*` thread settings; `m_dmName` defaulting to the
  channel name; telemetry defaults.
- **Pattern generation** (`generateSparkleClock()`): cube size from `interval*frequency` rounded down to a multiple of
  4, and the first frame checked pixel by pixel against the cosine Fourier mode formula for a single speckle pair,
  the cross pair, a rotated angle, an angle cancelled by the angle offset, and linear amplitude scaling.
- **appLogic()**: reading the width, height and type of a connected in-memory DM stream, rejecting a non-float
  stream, and the telemeter's failure path when no telemetry thread is running.
- **INDI**: device/name validation of `cross`, `dwell` and `single` with `XWCTEST_INDI_NEW_CALLBACK`, name validation of
  the other callbacks, and the effect of each callback: current/target precedence and missing values for the float
  parameters, rejection of negative frequency/interval, zero dwell and out-of-range `single`, the `trigger`, `cross`
  and `modulating` toggles, `m_restartSp` requests, and the `zero` request writing zeros to the DM stream (refused
  while modulating).

## Test harness

- `MagAOX::app::sparkleClock_test` (the friend class declared by the app) creates the INDI properties the callbacks
  validate against, backs `m_imageStream` with an in-memory `IMAGE_METADATA` and pixel buffer with no semaphores, and
  makes the protected members public with using-declarations.
- `tests/testMacrosINDI.hpp` is included after the app header, so the callback bodies run.
- No shared memory streams, INDI server or app threads are used.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../sparkleClock.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/sparkleClock/tests/sparkleClock_test
../apps/sparkleClock/tests/sparkleClock_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The modulator thread (`modThreadExec()`), `appStartup()`, `appShutdown()` and the `NOTCONNECTED` stream-opening
  path of `appLogic()` are not tested, since they start threads or open real shared memory streams.
- Only the first frame of the pattern is checked: the later frames overlap because the frame index is not scaled by
  the four frames per position (`baseIdx` in `generateSparkleClock()`).
- `generateSparkleClock()` writes a debug cube to `/tmp/specks.fits`; the tests remove it afterwards.
- `recordSparkleClock()` is not tested (it writes telemetry and keeps function-static state).
