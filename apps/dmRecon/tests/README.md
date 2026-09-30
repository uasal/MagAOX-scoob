# dmRecon unit tests

Catch2 unit tests for the `dmRecon` MagAO-X application (`apps/dmRecon/dmRecon.hpp`).

## Test files

| File | Covers |
|------|--------|
| `dmRecon_test.cpp` | Configuration, mask processing, the modes pseudo-inverse, command reconstruction and output streams, the frameGrabber interface, `setGPU()`, the INDI callbacks, and `appShutdown()` |

## What is tested

- **Configuration**: the `recon.*` keywords, defaults and overrides, and the stream names derived from `recon.loopNumber` (`aol<N>_CMmodesDM`, `dm<NN>disp_delta`, `dm<NN>disp_actmask`, `aol<N>_modevalDMf` and its `_mon` monitor), including overrides through the `dmModes`, `dmMask`, `dmCommand` and `framegrabber` sections.
- **Mask**: the mask `processImage()` builds the good-pixel index; a new mask once ready restarts all three monitors.
- **Modes allocation**: the modes `allocate()` takes the size from the stream, limits the number of modes with `recon.numModes`, and waits (restart) until the mask is ready with the same size.
- **Pseudo-inverse**: on a 3x3 mask with 6 good pixels and two orthogonal modes (with large values at the masked-out pixels), the modes `processImage()` gives the expected pseudo-inverse; `recon.inverseNumModes` truncates it to the strongest mode, `recon.numModes` uses only the first modes, and an identity response matrix gives the expected renormalization.
- **Command allocation**: waits (restart and frameGrabber reconfigure) until the modes and mask are ready and the frameGrabber is waiting.
- **Reconstruction**: a command of `3 A + 0.5 B` (with large values at the masked-out pixels) gives mode amplitudes (3, 0.5), which are written to the monitor stream and, relative to `aol<N>_modevalDM`, to the diff stream; `loadImageIntoStream()` copies them; with `writeDMf` on, the frameGrabber semaphore is posted and `acquireAndCheckValid()` returns a valid frame; a command before allocation is ignored.
- **frameGrabber interface**: `configureAcquisition()` waits for the command, sets the image size and type, waits for a missing stream and opens an existing one; `acquireAndCheckValid()` is not valid before the command is ready; `fps()`, `startAcquisition()` and `reconfig()`.
- **setGPU**: returns 0 when no GPU is requested; without CUDA a GPU request fails and turns `m_useGPU` off.
- **Callbacks**: the `fps` source set callback and the `writeDMf` toggle, including wrong device, wrong name and missing element.
- **Shutdown**: `appShutdown()` succeeds when no threads were started.

## Test harness

- `dmRecon_test` subclass exposes the protected members and callbacks with `using` declarations, and adds accessors for the sizes, restart flags, shmim names and `getExistingFirst` flags of the three shmimMonitor bases and for the frameGrabber members (these have the same names in several bases). It initializes the frameGrabber semaphore (normally done in `appStartup()`), and closes any stream opened by `configureAcquisition()`.
- `loadMask()`, `loadTwoModes()` and `allocateCommand()` drive the real `allocate()`/`processImage()` methods with in-memory frames, in the order the monitor threads would.
- The modes `processImage()` writes `PInv.fits` (and `wmodes.fits` with a response matrix) to the current directory, so those calls run in `/tmp/dmRecon_test_work`, which is removed afterwards.
- The command allocation and the frameGrabber tests create real shared-memory streams with `MILK_SHM_DIR=/tmp/dmRecon_test_shm`, which is removed afterwards.
- `testMacrosINDI.hpp` is not included, so the callback bodies run, and the device/name checks are tested explicitly.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dmRecon.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dmRecon/tests/dmRecon_test
../apps/dmRecon/tests/dmRecon_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The GPU path (`MXLIB_CUDA` with `recon.useGPU`) is not tested: it needs a CUDA device.
- `appStartup()` and `appLogic()` are not tested: they start the shmimMonitor, frameGrabber and telemetry threads, and `appLogic()` calls `pthread_tryjoin_np()` on them.
- Telemetry (`checkRecordTimes()`, `recordTelem()`) is not tested.
- Several tests take about a second each, because `allocate()` sleeps for 1 s when its inputs are not ready.
- Build requirement: without `MXLIB_CUDA`, `dmRecon.hpp` itself does not currently compile (the CPU `m_modevals = ...` statement in the command `processImage()` is missing its `;`, and the `mx::cuda::cublasHandle m_cublas` member is not inside `#ifdef MXLIB_CUDA`). These tests target the CPU path and build once those two lines are fixed.
