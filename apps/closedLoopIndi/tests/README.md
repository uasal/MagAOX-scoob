# closedLoopIndi unit tests

Catch2 unit tests for the `closedLoopIndi` MagAO-X application (`apps/closedLoopIndi/closedLoopIndi.hpp`).

## Test files

| File | Covers |
|------|--------|
| `closedLoopIndi_test.cpp` | Configuration and its error checks, `appStartup()`, `appLogic()`, loop state, the control law in `updateLoop()`, and the INDI callback bodies |
| `closedLoopIndi_indi_test.cpp` | INDI callback device/name validation (validation mode) |

## What is tested

- **Configuration**: the `input.*`, `ctrl.*` and `loop.*` keywords, defaults (including the `x`/`y`/`counter` elements, zero references, `current`/`target` elements and the identity interaction matrix) and overrides, and a single `ctrl.devices` entry being used for both axes.
- **Configuration errors**: no input device or property, the wrong number of input elements or references, and the wrong number of ctrl devices, properties, targets or currents all make `loadConfigImpl()` return -1 and set the shutdown flag.
- **Startup**: `appStartup()` creates the reference, gain, loop state, counter reset and deltas properties (with the configured initial values), registers the input, controller, controller FSM and upstream properties with the configured device/property names (one FSM property for a shared controller), expands a single `loop.gains` value to both axes, and rejects three gains or no gains.
- **State**: `appLogic()` reports READY with the loop open and OPERATING with it closed; `toggleLoop()` changes state only on a transition.
- **Control law**: `updateLoop()` does nothing until every controller reports READY (or OPERATING with `ctrl.operatingOK`), then computes the residuals `measurements - references` and the commands `intMat * residuals`, with the loop open and closed.
- **Input callback**: a new frame counter updates the measurements (and runs the loop), a repeated counter is ignored, and missing elements or the wrong device are rejected.
- **New-property callbacks**: `reference0`/`reference1` (target, current fallback, no value, wrong device), `loop_gain`, `loop_state` and `counter_reset` (including accepting a restarted counter after a reset).
- **Controller callbacks**: FSM states are stored per device (missing `state` ignored, wrong device rejected), and the current values are cached per axis using the configured element names.
- **Upstream callback**: follows the upstream loop open, closes only with `loop.upstreamFollowClosed`, ignores a property without a toggle, and rejects the wrong device.
- **INDI validation** (`closedLoopIndi_indi_test.cpp`): every new and set callback rejects the wrong device and property names.

## Test harness

- `closedLoopIndi_test` subclass exposes the protected members with `using` declarations. The tests write a configuration file, run `setupConfig()`/`loadConfigImpl()`, and then the real `appStartup()`: with no INDI driver it only creates the properties and inserts them in the callback maps, so it is safe in a unit test.
- `closedLoopIndi_test.cpp` does not include `testMacrosINDI.hpp`, so the callback bodies run. `closedLoopIndi_indi_test.cpp` includes it before the app header, so each callback returns right after the device/name check; it has its own harness that only names the properties.
- With no INDI driver, `sendCommands()` logs a failed send for each controller but still returns 0, so the command values sent are not observable; the tests check `m_commands` and the residuals instead.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../closedLoopIndi.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/closedLoopIndi/tests/closedLoopIndi_test
../apps/closedLoopIndi/tests/closedLoopIndi_test
make -f Makefile.one t=../apps/closedLoopIndi/tests/closedLoopIndi_indi_test
../apps/closedLoopIndi/tests/closedLoopIndi_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The values actually sent to the controllers are not checked, since `sendNewProperty()` needs an INDI driver.
