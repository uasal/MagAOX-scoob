# kcubeCtrl unit tests

Catch2 unit tests for the `kcubeCtrl` MagAO-X application (`apps/kcubeCtrl/kcubeCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `kcubeCtrl_test.cpp` | INDI callback validation and bodies, configuration, `appStartup()`, axis initialization, enable/voltage commands, `set()`/`rest()`, the `appLogic()` state machine, and the `tmcCon` error reporting |
| `stubs/tmcController.hpp` | Stand-in for the Thorlabs `tmcController` library header (and the libftdi1 pieces it exposes) |

## What is tested

- **INDI validation**: every `newCallBack_m_indiP_*` callback (`axis1_identify`, `axis1_enable`, `axis1_voltage`, the axis 2 equivalents, and `set`) rejects the wrong device or name and accepts its own.
- **Construction and configuration**: power management, the axis flags, and the `axis1.serial`/`axis2.serial` options via `loadConfig()` and `loadConfigImpl()` (unset options leave the serial unchanged).
- **appStartup()**: the request, toggle and number properties with their elements and zeroed voltages, and the move to NODEVICE.
- **Axis initialization**: the `axis1Initialize()`/`axis2Initialize()` call sequence, channel 1 disabled, display brightness set to 0, the 150 V output limit, the flags cleared, and an early hardware-query error.
- **Commands**: `axisNEnable()`/`axisNDisable()` (enable state, flags, error), and `axisNVoltage()` clamping to 0–150 V and conversion to a fraction of 150 V.
- **set()/rest()**: both axes enabled at 75 V (0.5) and rested at 0 V then disabled, and the no-op cases.
- **appLogic()**: both devices missing (NODEVICE), one missing or an open error (ERROR), the full open/connect/initialize/voltage-poll sequence to READY in one pass, retry from ERROR, READY/OPERATING from the enable flags, a connect failure while powered off, and a voltage-poll failure (ERROR).
- **INDI callbacks**: requests ignored before READY, identify only in READY (and its return value), enable toggles, voltage targets (clamped and converted), set/rest, and rejection of wrong devices/names.
- **Error reporting**: `tmcCon::ftdiErrmsg()` and `otherErrmsg()` override the base class and log through MagAOX, the former using `ftdi_get_error_string()`.

## Test harness

- `stubs/tmcController.hpp` shadows `/opt/MagAOX/source/tmcController/tmcController.hpp` (the app includes it as `"tmcController.hpp"`, which is not next to the app header, so the stub directory on the include path wins).  It declares only what kcubeCtrl uses: the `EnableState` and `VoltLimit` enums, the `HWInfo`, `KMMIParams` and `TPZIOSettings` structs, the controller calls, the error message virtuals, and `ftdi_context`/`ftdi_get_error_string()`.
- The stub bodies are defined in `kcubeCtrl_test.cpp`.  Each controller has its own fake state (`tmcAxisStub`, keyed by controller address): per-function return codes, a call log, the enable state and channel, the device-side MMI and I/O settings, and the last output voltage fraction.  No libftdi1, library or hardware is needed.
- `kcubeCtrl_test` subclass: exposes the protected axis controllers, flags, configurator and power state with `using` declarations, and gives the INDI properties their device and names.
- `testMacrosINDI.hpp` is included after the app header, so callback bodies are live.  `XWCTEST_INDI_NEW_CALLBACK` still applies because every callback returns 0 for an empty property when the app is not READY or OPERATING.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../kcubeCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/kcubeCtrl/tests/kcubeCtrl_test
../apps/kcubeCtrl/tests/kcubeCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The stub mirrors how kcubeCtrl calls `tmcController`; the real library's exact types (return types of `serial()`, `vendor()`, `product()`, the enum values) are not available here, so only the app's use of them is exercised, not the APT protocol encoding in the library itself.
- Every error path in the app sleeps 1 s before checking the power state, so only a few are exercised (about 4 s in total).
