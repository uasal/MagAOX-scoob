# nnReconstructor unit tests

Catch2 unit tests for the `nnReconstructor` MagAO-X application (`apps/nnReconstructor/nnReconstructor.hpp`).
The old implementation in `../OLD/` is not tested.

## Test files

| File | Covers |
|------|--------|
| `nnReconstructor_test.cpp` | Half-precision helpers, the TensorRT logger, configuration, engine loading and context creation, device memory, `allocate()`, `processImage()` in fp32/fp16 and with the explicit tip/tilt input, the frameGrabber hooks, the fps source callback, `appStartup()` with a missing engine, `appShutdown()` and telemetry, against stubbed TensorRT and CUDA |

## What is tested

- **Half precision**: `floatToHalfArray()`/`halfToFloatArray()` round trips (exact values, rounding, subnormals, element count).
- **Logger**: the TensorRT `Logger` prints internal errors, errors and warnings, but not info or verbose messages.
- **Configuration**: defaults and overrides of every `parameters.*` key, plus `shmimMonitor.shmimName`, `framegrabber.shmimName`, `framegrabber.circBuffLength` and `telemeter.maxInterval`; the output stream is not owned (`m_ownShmim` false).
- **Engine loading**: `load_engine()` reads the whole (binary) file, gives empty data for an empty file, and throws for a missing file; `create_engine_context()` passes the app logger to the runtime, deserializes the loaded data, and takes the input C/H/W and output size from the tensor shapes.
- **Device memory**: `prepare_engine_memory()` buffer sizes for fp32/fp16 with and without the second input; `cleanup_engine_memory()`/`cleanup_engine_context()` free and delete what exists.
- **`allocate()`**: host buffer sizes from the stream and config, zeroed, with half-precision buffers only in fp16 mode.
- **`processImage()`**: four zero-padded pupils with `imageNorm`, the host-to-device copy of the whole preprocessed image (as half in fp16 mode), inference with 2 or 3 bindings, the device-to-host copy into `modeval`, the update flag, timestamp and semaphore post.
- **frameGrabber hooks**: `acquireAndCheckValid()` (updated, posted without update, 1 s timeout), `loadImageIntoStream()`, `fps()`, `startAcquisition()`, `reconfig()`, and `configureAcquisition()` with a missing output stream.
- **INDI**: `setCallBack_m_indiP_fpsSource()` rejects the wrong device or name and a missing `current`, and otherwise sets the fps.
- **Startup/shutdown**: `appStartup()` registers the fps properties and then throws on a missing engine file; `appShutdown()` releases the engine, device memory and host buffers.
- **Telemetry**: `recordTelem()` and `checkRecordTimes()`.

## Test harness

- `nnReconstructor_test` subclass: sets `m_configName`, resets the stubs, initializes the frame semaphore (normally done by `appStartup()`), runs the configuration, exposes the protected members (the same-named `shmimMonitor`/`frameGrabber` members through accessors), and releases the buffers the app only frees in `appShutdown()`. Its `shutdown()` resets the pointers `appShutdown()` frees without resetting.
- `stubs/NvInfer.h`: the TensorRT subset used (`ILogger`, `Dims`, `IRuntime`, `ICudaEngine`, `IExecutionContext`, `createInferRuntime()`), as abstract interfaces. `nnReconstructor_test.cpp` implements fake runtime/engine/context classes driven by `trtStubState` (tensor names and shapes, scripted output, captured inputs, delete counts).
- `stubs/cuda_runtime_api.h` and `stubs/cuda_fp16.h`: `cudaMalloc`/`cudaFree`/`cudaMemcpy` on host memory (`cudaStubState` records sizes, directions and live allocations) and an IEEE binary16 `half` with host conversions. No GPU, CUDA or TensorRT library is needed.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../nnReconstructor.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/nnReconstructor/tests/nnReconstructor_test
../apps/nnReconstructor/tests/nnReconstructor_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Real TensorRT inference and GPU memory are not exercised.
- `create_engine_context()` dereferences a null runtime or engine after printing the error, so those failure paths cannot be tested.
- A successful `appStartup()` starts the shmimMonitor and framegrabber threads, and `appLogic()` checks them, so neither is run.
- One `acquireAndCheckValid()` case waits for its 1 second timeout.
