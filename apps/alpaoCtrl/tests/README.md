# alpaoCtrl unit tests

Catch2 unit tests for the `alpaoCtrl` MagAO-X application (`apps/alpaoCtrl/alpaoCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `alpaoCtrl_test.cpp` | Configuration, calibration and actuator-mapping file reading, the `dev::dm` hooks (`initDM`, `zeroDM`, `commandDM`, `releaseDM`) against a stubbed ALPAO SDK, shutdown/power-off, and the `dev::dm` INDI request callbacks |

## What is tested

- **Configuration**: defaults (`satThresh`, calibration directory `dm/alpao_`), and overrides of `dm.serialNumber` (lower-cased into the calibration directory), `dm.satThresh`, `dm.shmimName` (derived stream names), and `dm.calibPath`.
- **Calibration files**: `parse_calibration_file()` success, the lower-case file name, and a missing file; `appStartup()` failing on a missing file or a zero stroke/volume factor; `get_actuator_mapping()` decoding a FITS actuator map (row order and linear indices) and the missing-file case.
- **initDM**: upper-case serial number passed to `asdkInit`, actuator count from `asdkGet`, buffer allocation, zeroing, state `READY`, the already-initialized no-op, and failures (SDK error code, null handle, `asdkGet` failure, zero actuators, send failure).
- **zeroDM**: uninitialized and zero-actuator errors, an all-zero command, and SDK send failures.
- **commandDM**: gain scaling (`volume_factor/max_stroke`), mean (piston) removal, actuator mapping, clipping to +/-1 with the saturation counter, the physical-unit output shape, the instantaneous saturation map, and SDK send failures.
- **releaseDM / appShutdown / onPowerOff**: zero, reset and release sequence, reset/release/zeroing errors, release on shutdown, and the `NOTHOMED` state after power off.
- **INDI**: device/name validation of the `initDM`, `zeroDM`, `releaseDM` and `zeroAll` callbacks, and the effect of `initDM` (state checks, SDK failure) and `zeroDM` requests.

## Test harness

- `alpaoCtrl_test` subclass exposes protected members, sets `m_calibDir` under `/tmp/alpaoCtrl_test`, creates the `dev::dm` request properties, and provides `fakeInit()` to put the app in an initialized state without `initDM()`. `fakeInit()` also sets `m_shutdown` so `releaseDM()` does not signal the never-started shmim monitor thread.
- `stubs/asdkWrapper.h`: stand-in for the ALPAO SDK header (`asdkWrapper.h`), declaring only the types and functions used by the app. The stub bodies and their fake state (`asdkStubState`: return codes, error codes, call counts, captured serial number, parameter name and command vector) are defined in `alpaoCtrl_test.cpp`, so the test does not link `-lASDK`.
- `testMacrosINDI.hpp` is included after the app header so callback bodies stay live.
- The `commandDM` test creates a 2x2 output-shape stream with `MILK_SHM_DIR` set to `/tmp/alpaoCtrl_test/shm`, removed afterwards.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../alpaoCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/alpaoCtrl/tests/alpaoCtrl_test
../apps/alpaoCtrl/tests/alpaoCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` success and `appLogic()` are not tested: they start (or join) the saturation and shmim monitor threads.
- `releaseDM()` sleeps for 1 s, so the release tests take several seconds.
