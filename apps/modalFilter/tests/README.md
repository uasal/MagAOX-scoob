# modalFilter unit tests

Catch2 unit tests for the `modalFilter` MagAO-X application (`apps/modalFilter/modalFilter.hpp`).

## Test files

| File | Covers |
|------|--------|
| `modalFilter_test.cpp` | Configuration, filter-parameter streams, size checks, integrator and PC filter math, INDI callbacks |

## What is tested

- **Configuration**: defaults (`circBuff.*`, `loop.number`, `loop.name`), the `aol<N>_*` shmim names derived from the
  loop number for all seven shmimMonitors and the framegrabber output, explicit shmim-name overrides, and the
  `getExistingFirst` flags set by the constructor.
- **Parameter streams**: `allocate()`/`processImage()` for the gain, mult, PC gain and PC mult factor streams (height
  must be 1), and parsing of the a/b coefficient streams into per-mode counts (`m_Na`, `m_Nb`) and coefficient matrices.
- **Sizes**: modeval `allocate()` (height check, `m_modevalSz`, circular buffer lengths) and `checkSizes()` for both the
  integrator (`m_sizesMatch`) and PC (`m_pcSizesMatch`) flags.
- **Framegrabber hooks**: `configureAcquisition()` output geometry (and exiting the wait on shutdown),
  `startAcquisition()`, `reconfig()`, `fps()`, `loadImageIntoStream()` copying the latest DM command.
- **Filter math**: `acquireAndCheckValid()` on synthetic 2-mode vectors: size mismatch and open loop return 1 (zeros
  written), the closed-loop leaky integrator `dm = -g*gf*wfs + mc*mf*dm_prev` checked against known values and a
  reference recursion across a circular-buffer wrap, the output timestamp, the PC branch using the PC gain/mult
  parameters, and the 1 s semaphore timeout.
- **INDI**: `setCallBack_m_indiP_fpsSource` (device/name validation, missing element, `circBuff.fpsTol` tolerance and
  `m_reconfig`), `newCallBack_m_indiP_loop`/`pcOn` toggles, and `newCallBack_m_indiP_gain`/`mult`/`pcGain`/`pcMult`
  (target, current fallback, missing value, device/name validation).

## Test harness

- `MagAOX::app::modalFilter_test` is the befriended harness class. It must be in `MagAOX::app` because `modalFilter`
  inherits its shmimMonitor, frameGrabber and telemeter bases privately. It initialises the filter semaphore
  (normally done in `appStartup()`), names the INDI properties, sets stream geometry directly on the base-class members,
  and exposes internal state through accessors.
- Callback bodies are live (`tests/testMacrosINDI.hpp` is not included), and device/name validation is checked with
  explicit mismatched properties.
- No shared memory streams, threads or INDI server are used.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../modalFilter.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/modalFilter/tests/modalFilter_test
../apps/modalFilter/tests/modalFilter_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called, since they start the shmimMonitor, framegrabber and
  telemetry threads.
- The PC branch currently computes the a/b coefficient sums and then discards them, so the test checks the behaviour
  as written (a PC-parameter integrator), not a full ARMA filter.
- Closing the loop before the DM circular buffer holds at least one entry reads out of range (`m_modevalDM[-2]`), so the
  tests always seed the buffer with one open-loop frame first.
- `recordTelem()`/`checkRecordTimes()` are not tested (they write telemetry).
