# magAOXMaths unit tests

Catch2 unit tests for the `magAOXMaths` MagAO-X application (`apps/magAOXMaths/magAOXMaths.hpp`).

## Test files

| File | Covers |
|------|--------|
| `magAOXMaths_test.cpp` | Configuration, `appStartup()` property setup, the `updateVals()` maths, and the INDI callbacks |

## What is tested

- **Configuration**: defaults and overrides of `myVal`, `otherDevName`, `otherValName` and `startVal` via `setupConfig()`/`loadConfig()`.
- **Startup**: `appStartup()` creates the `myVal`, `maths`, `other_val` and other-device properties with the configured
  device/names, registers their NEW/SET callbacks, applies `startVal`, computes the initial maths, and sets `READY`.
  `appLogic()` and `appShutdown()` return 0.
- **Maths**: `updateVals()` fills `value`, `sqr`, `sqrt`, `abs` and `prod` (value times the other app's value) for
  positive, negative and zero values, including the -1 to -5 values that log at escalating priority.
- **SET callback**: `setCallBack_m_indiP_otherVal()` copies the other app's value and recomputes the product.
- **NEW callbacks**: `newCallBack_m_indiP_myVal()` rejects a wrong property name; `newCallBack_m_indiP_setOtherVal()`
  rejects a wrong name and, for the matching name, sets the `target` element and recomputes the maths.

## Test harness

- `magAOXMaths_test` subclass sets the device name (`m_configName`), exposes the protected members with `using`
  declarations, wraps `setupConfig()`/`readConfig()`/`loadConfig()`, and reports which callbacks are registered.
- The callback bodies run live (`tests/testMacrosINDI.hpp` is not included), because the callbacks compare only the
  property name and do not use `INDI_VALIDATE_CALLBACK_PROPS`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../magAOXMaths.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/magAOXMaths/tests/magAOXMaths_test
../apps/magAOXMaths/tests/magAOXMaths_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- A matching property is never sent to `newCallBack_m_indiP_myVal()`: its body calls `m_indiDriver->sendSetProperty()`
  without a null check, and there is no INDI driver in the unit tests.
- Sending the new value to the other app (`sendNewProperty()`) needs an INDI driver, so only the local `target` update
  of `newCallBack_m_indiP_setOtherVal()` is checked.
- `sqrt` of a negative value is not checked, since NaN handling is unreliable under `-ffast-math`.
