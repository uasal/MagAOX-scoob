# refRMS unit tests

Catch2 unit tests for the `refRMS` MagAO-X application (`apps/refRMS/refRMS.hpp`).

## Test files

| File | Covers |
|------|--------|
| `refRMS_test.cpp` | Configuration, reference/mask allocation, masked mean/RMS computation, and the fps source SET callback |

## What is tested

- **Configuration**: defaults (empty `rms.fpsSource`, both shmim names defaulting to the device name,
  `getExistingFirst` forced true by the constructor) and overrides of `rms.fpsSource`, `refShmim.*` and
  `maskShmim.*`.
- **Mask stream**: `allocate(maskShmimT)` sizing, `processImage(maskShmimT)` copying the column-major mask and
  computing the mask sum, and the `m_maskValid` flag.
- **Reference stream**: `allocate(refShmimT)` sizing the image, resetting a mismatched mask to ones, and sizing the
  RMS/mean circular buffers to `11*fps`; `processImage(refShmimT)` computing the masked mean and RMS for uniform
  and partial masks, circular buffer wrapping, and skipping frames when the mask size does not match.
- **INDI**: `setCallBack_m_indiP_fpsSource` rejecting a wrong property name, ignoring a property without
  `current`, and storing a changed fps (which sets the reference monitor's restart flag).

## Test harness

- `refRMS_test` subclass exposes the protected members, sets the stream sizes the shmimMonitor would normally set,
  and calls `allocate()`/`processImage()` directly with in-memory frames, so no ImageStreamIO stream is created.
  Callback bodies are live (`testMacrosINDI.hpp` is not included).

## Known limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called: they start (or `pthread_tryjoin_np`) the
  shmimMonitor threads. The 1/2/5/10 second averaging in `appLogic()` is therefore not covered.
- `allocate(refShmimT)` sleeps in a loop while `m_fps == 0`, so the tests always set a non-zero fps first.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../refRMS.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/refRMS/tests/refRMS_test
../apps/refRMS/tests/refRMS_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).
