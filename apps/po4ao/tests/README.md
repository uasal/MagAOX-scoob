# po4ao unit tests

Catch2 unit tests for the `po4ao` MagAO-X application (`apps/po4ao/po4ao.hpp`), a TensorRT policy-inference
controller.

## Test files

| File | Covers |
|------|--------|
| `po4ao_test.cpp` | `CircularBuffer`, the TensorRT `Logger`, configuration, engine loading/reloading/switching/cleanup, the `reload_engine` INDI callback, `allocate()`, `processImage()` and `appShutdown()` |

## What is tested

- **CircularBuffer**: `add()` ring storage and overwrite, `num_elements()` saturating at the history size,
  `add_eigenimage()` advancing the write position.
- **Logger**: kINTERNAL_ERROR, kERROR and kWARNING messages are printed to `std::cout`, kINFO and kVERBOSE are not.
- **Configuration**: defaults and overrides of every `parameters.*` key read by `loadConfigImpl()`, and the input
  `shmimMonitor.shmimName`.
- **Engine handling**: `load_engine()` reading a binary file (and throwing `std::length_error` for a missing file),
  `create_engine_context()` deriving `inputC/H/W`, `inputSize`, `outputSize`, `Nact`, `Nact_across` and `Nfeatures`
  from the tensor shapes, `prepare_engine_memory()`/`cleanup_engine_memory()`, `reload_engine()` building a second
  engine with the existing runtime, `switch_engine()` swapping the engines, and `cleanup_engine_context()` deleting
  all TensorRT objects.
- **INDI**: `newCallBack_m_indiP_reloadToggle` with a wrong property name, a missing `toggle` element (throws), toggle
  off (no action) and toggle on (reloads the engine).
- **Stream handling**: `allocate()` host-buffer creation and opening of the output and observation streams, and its
  failures when a stream is missing or has fewer than 10 semaphores.
- **Frame processing**: `processImage()` during warmup (integrator `gain * input`, episode counting, publishing of the
  observation stream once per episode) and after warmup (inference via `executeV2()` with host-to-device and
  device-to-host copies, and switching to a reloaded engine after the frame).
- **Shutdown**: `appShutdown()` deleting the runtime, both engines and contexts, and freeing the device buffers.

## Test harness

- `po4ao_test` subclass exposes the protected members with using-declarations, creates the `reload_engine` toggle
  property, and in `shutdownForTest()` (also run by its destructor) closes the streams `allocate()` opened, calls
  `appShutdown()`, and frees `m_output` and resets the pointers that `appShutdown()` leaves dangling.
- `stubs/`: stand-ins for the TensorRT (`NvInfer.h`) and CUDA runtime (`cuda_runtime_api.h`) headers. The fake
  runtime, engine and context classes and the CUDA functions are defined in `po4ao_test.cpp`: "device" memory is host
  memory, and a `po4aoStubState` sets the tensor shapes and inference output and records calls.
- `using mx::improc::eigenImage;` is declared before including `po4ao.hpp`, because `CircularBuffer` uses
  `eigenImage` unqualified at global scope, before anything brings it into scope.
- `allocate()`/`processImage()` tests create real ImageStreamIO streams in a private `MILK_SHM_DIR`
  (`/tmp/po4ao_test_shm`), which is removed afterwards. Engine files are written to `/tmp/po4ao_test_*.plan`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../po4ao.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/po4ao/tests/po4ao_test
../apps/po4ao/tests/po4ao_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations and known app issues

- `appStartup()` and `appLogic()` are not called: they start, or join-check, the shmimMonitor thread.
- `CircularBuffer::add_eigenimage()` copies from the address of the `Eigen::Map` object instead of its data, so the
  stored contents are not checked (images are kept to 4 elements so the copy stays within the object).
- `CircularBuffer` and `appShutdown()` release `new[]` arrays with `delete`, and `appShutdown()` neither frees
  `m_output` nor closes the output streams.
- `loadConfigImpl()` reads `po4ao_act_Channel` from `parameters.observation_channel`; the test asserts this current
  behavior. `parameters.dataDirs` and `parameters.modal_filt_matrix` are registered but never read.
- The INDI callback checks only the property name, not the device, and throws if the `toggle` element is missing.
- `processImage()` takes `frame_counter % iterations_per_ep`, which divides by zero with the default of 0, so tests
  always set `iterations_per_ep`.
- `load_engine()` does not stop on a missing file, and `create_engine_context()`/`reload_engine()` dereference null
  runtime/engine pointers on failure, so those failure paths are not exercised.
- The state vector passed to inference is all zeros, as its history assembly is commented out in the app.
