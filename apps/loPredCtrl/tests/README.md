# loPredCtrl unit tests

Catch2 unit tests for the `loPredCtrl` MagAO-X application (`apps/loPredCtrl/loPredCtrl.hpp`) and its DDSPC
predictive-control sources (`recursive_least_squares`, `ar_controller`, `utils`).

## Test files

| File | Covers |
|------|--------|
| `loPredCtrl_test.cpp` | DDSPC utilities, recursive least squares, predictive controller, app configuration, `allocate()`/`processImage()`, INDI callbacks |

## What is tested

- **DDSPC utilities**: `find_next_power_of_2()` (returns the power of 2 strictly greater than its argument) and a
  `save_matrix()`/`load_matrix()` round trip for a column vector and a symmetric matrix.
- **Recursive least squares**: constructor sizes and initial covariance, `reset()`, one update checked against a hand
  calculation, convergence to known 2x3 coefficients from noise-free synthetic data (with the `Matrix` and `eigenImage`
  overloads giving the same result), and a forgetting factor < 1 tracking a change in the coefficients.
- **Predictive controller** (1 mode): initial prediction-matrix size, the integrator-only command
  `-gain * m + noise`, all measurement/command buffer accessors, learning the plant `m_k = a m_{k-1} + b c_{k-1}` in
  closed loop to the exact prediction matrix, `update_controller()` matching the regularized least-squares formula,
  `set_regularization()` switching the regularization, and `reset()`.
- **Configuration**: defaults and overrides for all `parameters.*` keys, `outputShmim.shmimName` and the input
  `shmimMonitor.shmimName`.
- **Stream processing**: `allocate()` buffer sizes and controller creation; `processImage()` command for the controlled
  modes with pass-through of the rest, learning after a warm-up, the reset request, and consumption of an exploration
  sequence (set switching, step counting, noise and regularization steps).
- **INDI**: `newCallBack_m_indiP_exploration` (device/name validation, missing target, parsing into the inactive set),
  the `learn` and `predict` toggles, and the `reset_model` request.

## Test harness

- The DDSPC `.cpp` files are built as `OTHER_OBJS` for the app; the test `#include`s them so it stays a single
  translation unit.
- `loPredCtrl_test` subclass names the INDI properties, sets the stream geometry directly on the shmimMonitor base,
  wraps the protected `allocate()`/`processImage()`, and exposes internal state. It deletes the controller in its
  destructor (the app only does so in `appShutdown()`), and before re-allocating.
- No shared memory streams, threads or INDI server are used.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../loPredCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/loPredCtrl/tests/loPredCtrl_test
../apps/loPredCtrl/tests/loPredCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The predictive controller is only exercised with one mode: with more than one mode,
  `calculate_command()` sizes its past vector as `2*history-1` rather than `(2*history-1)*num_modes`, and the buffer
  accessors index a row block as `dat(j, 0)`, both of which trip Eigen assertions.
- `load_matrix()` maps row-major CSV data into a column-major matrix, so general (non-symmetric, multi-column)
  matrices do not round trip; only shapes unaffected by this are tested.
- `appStartup()`, `appLogic()` and `appShutdown()` are not called, since they start the shmimMonitor thread.
- `save()`/`load()` are declared but not defined in the app and are not tested.
