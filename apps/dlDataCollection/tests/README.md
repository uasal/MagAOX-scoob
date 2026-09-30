# dlDataCollection unit tests

Catch2 unit tests for the `dlDataCollection` MagAO-X application (`apps/dlDataCollection/dlDataCollection.hpp`).

## Test files

| File | Covers |
|------|--------|
| `dlDataCollection_test.cpp` | `loadCSV()`, `ImageBuffer`, configuration, `loadRandomAmps()`, `allocate()`, `send_to_shmim()`, `processImage()` (pupil extraction, frame skipping, dataset saving) and `appShutdown()` |

## What is tested

- **`loadCSV()`**: reads one value per line, returns false for a missing file, and throws `std::invalid_argument` when the file has too few lines.
- **`ImageBuffer`**: starts zeroed, `add()` fills slots in order, `clear()` rewinds without zeroing, `save()` writes all slots as raw floats and does not throw for an unwritable path.
- **Configuration**: defaults of the initialized parameters (and the `shmimMonitor.shmimName` default of the config name) and overrides of every `parameters.*` key and `shmimMonitor.shmimName`.
- **`loadRandomAmps()`**: reads `Nperset*Nmodes` values from `<ampsDir>modeval_dataset_<n>.csv`; a missing file leaves the amplitudes unchanged.
- **`allocate()`**: sizes the buffers from the stream size and config, zeroes them, loads dataset 0, sizes the shaped command, and returns -1 when the modeval channel does not exist.
- **`send_to_shmim()`**: fills the shaped command column-major and copies `Nmodes` values into the modeval stream, bumping `cnt0` and clearing `write`.
- **`processImage()`**: extracts the four zero-padded pupils with `imageNorm`, sends the per-frame modal amplitudes, stores one frame every `NumFrameSkip` frames, saves `<dataDirs>images_dataset_<n>.bin` when `Nperset` frames are stored, loads the next amplitude file, and calls `appShutdown()` after the last dataset.
- **`appShutdown()`**: sends a zero modal command and frees the buffers.

## Test harness

- `dlDataCollection_test` subclass: sets `m_configName`, exposes the protected members with `using` declarations, frees the buffers the app never frees, and resets the pointers after `appShutdown()` (which frees them without resetting).
- The modeval output is an in-memory `IMAGE` (a local `IMAGE_METADATA` with no semaphores and a `std::vector<float>` buffer), attached after `allocate()` fails to open the (non-existent) channel.
- Test inputs are 16x16 frames with 6x6 pupils (2 pixel zero pad, so a 2x2 region per pupil), 4 modes and `Nact_across = 2`, so the shaped command stays within the modal buffer.
- `stubs/`: empty stand-ins for `NvInfer.h` (declares only `namespace nvinfer1`), `cuda_fp16.h` and `cuda_runtime_api.h`; the app includes them but calls no TensorRT or CUDA API.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dlDataCollection.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dlDataCollection/tests/dlDataCollection_test
../apps/dlDataCollection/tests/dlDataCollection_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` starts the shmimMonitor thread and `appLogic()` calls `pthread_tryjoin_np()` on a thread that was never started, so neither is called.
- The successful `allocate()` path needs a real shared-memory modeval stream with at least 10 semaphores, so it is not tested.
- `imageNorm`, `modalNorm`, `m_pupPix` and the pupil offsets have no default value, so their defaults are not checked.
