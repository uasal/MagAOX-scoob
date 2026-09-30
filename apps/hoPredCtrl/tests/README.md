# hoPredCtrl unit tests

Catch2 unit tests for the `hoPredCtrl` MagAO-X application (`apps/hoPredCtrl/hoPredCtrl.hpp`), the high-order
data-driven predictive controller built on the CUDA DDSPC classes.

## Test files

| File | Covers |
|------|--------|
| `hoPredCtrl_test.cpp` | DDSPC inline helpers, configuration, pupil mask, dark handling, `allocate()`, `processImage()`, DM command helpers, all INDI callbacks, `appShutdown()` |

## What is tested

- **DDSPC inline code** (from the `.cuh` headers): `find_next_power_of_2()` (which returns the bit length, not a
  power of 2), the `IDX2C`/`BIDX2C` index macros, the `Matrix` accessors (`num_elements()`, `set()`, `get()`,
  `get_data_ptr()`, including the default batch index of 1), and the controller's `set_integrator()`,
  `set_new_gamma()` and `get_command()`.
- **Configuration**: defaults (including both shmimMonitor names and the dark monitor's `getExistingFirst`) and
  overrides of every key read by `loadConfigImpl()`, including the `calib_directory` prefix on the calibration
  file names and `learning_steps` also setting the learning counter.
- **Pupil mask**: `set_pupil_mask()` measurement-vector sizing for the full-image and illuminated-pixel
  reconstructors.
- **Dark frames**: dark `allocate()` sizing and pixel-getter selection (and rejection of an unsupported type), and
  dark `processImage()` storing the frame.
- **allocate()**: reading the pupil mask, interaction matrix (normalized per mode), reference and mapping matrix from
  FITS files, the temporary command, opening and zeroing the DM, and the controller construction arguments,
  matrices, exploration buffer and integrator settings; failure for a missing DM stream or one with too few
  semaphores.
- **processImage()**: the open-loop measurement (dark subtraction, pupil-weighted normalization and reference
  subtraction); in closed loop, sending the command to the DM, the learning schedule (fixed number of steps,
  end-of-cycle reset with regularization / 10, `-1` for always learning), and `use_actuators == false`.
- **DM helpers**: `map_command_vector_to_dmshmim()`, `send_dm_command()` and `zero()`.
- **INDI**: every `newCallBack_*`: wrong property name, missing elements, `target`/`current` handling, closed-loop
  refusal for lambda, clipval and gamma, the control and predictor toggles, and all request switches (reset buffer,
  exploration, model, clean, controller update, zero in open and closed loop, save and load).
- **appShutdown()**: zeroing the DM and deleting the controller.

## Test harness

- `hoPredCtrl_test` subclass exposes the protected members with using-declarations, initializes the members the app
  leaves uninitialized (`controller`, `m_temp_command`, the loop flags, ...), names the INDI properties as
  `appStartup()` does, sets the shmimMonitor geometry directly, and frees the controller and temporary command
  (which the app only frees in `appShutdown()`).
- `stubs/`: stand-ins for the CUDA headers included by the `.cuh` files (`cuda_runtime.h`, `cublas_v2.h`,
  `device_launch_parameters.h`). `__global__`, `__device__` and `__host__` are empty macros, and the cuBLAS
  functions are only declared, since they are only called from unused `static inline` wrappers. The `.cuh` headers
  themselves are the real ones (they contain no kernel launches, so they parse under g++); their quoted CUDA
  includes are not found next to them, so they resolve to `stubs/`.
- The DDSPC `.cu` sources cannot be built by g++, so the `PredictiveController`, `DistributedAutoRegressiveController`
  and `Matrix` members the tests need are defined in `hoPredCtrl_test.cpp` as host-only stubs that record their
  arguments in a `hoPredCtrlStubState`; `get_command()` returns a test-controlled vector.
- Calibration FITS files are written to `/tmp/hoPredCtrl_test_*.fits`, and the DM channel is a real 50x50
  ImageStreamIO stream in a private `MILK_SHM_DIR` (`/tmp/hoPredCtrl_test_shm`); all are removed afterwards.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../hoPredCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/hoPredCtrl/tests/hoPredCtrl_test
../apps/hoPredCtrl/tests/hoPredCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations and known app issues

- The DDSPC numerics (recursive least squares, controller update, command computation, exploration) run on the GPU
  in the `.cu` files and are not tested; only the app's use of the controller is.
- `appStartup()` and `appLogic()` are not called: they start, or join-check, the shmimMonitor threads.
- Many members (`controller`, `m_temp_command`, `m_is_closed_loop`, `m_use_predictive_control`, ...) are not
  initialized by the app, so `appShutdown()` or the callbacks can use garbage pointers if `allocate()` has not run.
- `send_dm_command()` always copies 2500 floats and `map_command_vector_to_dmshmim()` loops over `m_dmHeight` for
  both axes, so only a 50x50 DM works; `appShutdown()` writes to the DM even if it was never opened.
- `processImage()` indexes the dark image (sized width x height) with (row, column) of the height x width WFS
  image, so only square WFS images are used.
- The interaction-matrix normalization divides each mode by its sum of squares rather than its norm.
- `appShutdown()` releases the `new[]` temporary command with `delete`.
- The callbacks check only the property name, not the device; the toggle callbacks throw if the `toggle` element is
  missing. The quadrant (`use_full_image_reconstructor == false`) path of `processImage()` is not reachable, since
  `allocate()` always selects the full-image reconstructor.
