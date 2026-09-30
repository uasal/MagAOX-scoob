# bmcCtrl unit tests

Catch2 unit tests for the `bmcCtrl` MagAO-X application (`apps/bmcCtrl/bmcCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `bmcCtrl_test.cpp` | Configuration, calibration and actuator-mapping file reading, the `dev::dm` hooks (`initDM`, `zeroDM`, `commandDM`, `releaseDM`) against a stubbed Boston Micromachines SDK, shutdown/power-off, and the `dev::dm` INDI request callbacks |

## What is tested

- **Configuration**: defaults (`satThresh`, empty `calibRelDir`, power management enabled) and overrides of `dm.serialNumber`, `dm.calibRelDir`, `dm.satThresh` and `dm.shmimName` (derived stream names and calibration paths).
- **Calibration files**: `parse_calibration_file()` success and a missing file, `appStartup()` failing on a missing file or a zero actuator gain/volume factor, and `get_actuator_mapping()` decoding a FITS map of 1-based actuator numbers into linear grid indices (and the missing-file case).
- **initDM**: upper-case serial number passed to `BMCOpen`, high resolution mode enabled (failure not fatal), actuator count from the handle, default map loading, zeroing, the mapping initialized to "ignored", state `READY`, refusal of a second init, and failures (open, map load, zeroing, zero actuators).
- **zeroDM**: not-open and zero-actuator errors, an all-zero command, and SDK failures.
- **commandDM**: gain scaling (`volume_factor/act_gain`), the square-root voltage conversion, clamping to [0, 1], ignored (`-1`) actuators, the output shape, the saturation counter and map (including a zero command counting as saturated), and SDK failures.
- **releaseDM / appShutdown / onPowerOff**: zero, clear, disable high resolution, and close sequence, the `NOTHOMED` state, and clear/close/zeroing errors.
- **INDI**: device/name validation of the `initDM`, `zeroDM`, `releaseDM` and `zeroAll` callbacks, and the effect of `initDM` (state checks, open failure) and `zeroDM` requests.

## Test harness

- `bmcCtrl_test` subclass exposes protected members, sets `m_calibDir` under `/tmp/bmcCtrl_test`, creates the `dev::dm` request properties, and provides `fakeInit()` to put the app in an opened state without `initDM()`. `fakeInit()` also sets `m_shutdown` so `releaseDM()` does not signal the never-started shmim monitor thread.
- `stubs/BMCApi.h` and `stubs/BMC_PCIeApi.h`: stand-ins for the Boston Micromachines SDK headers, declaring only the types and functions used by the app. The stub bodies and their fake state (`bmcStubState`: return codes, call counts, captured serial number and command array) are defined in `bmcCtrl_test.cpp`, so the test does not link the BMC libraries.
- `testMacrosINDI.hpp` is included after the app header so callback bodies stay live.
- The `commandDM` test creates a 2x2 output-shape stream with `MILK_SHM_DIR` set to `/tmp/bmcCtrl_test/shm`, removed afterwards.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../bmcCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/bmcCtrl/tests/bmcCtrl_test
../apps/bmcCtrl/tests/bmcCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The app is built with `-DXWC_DMTIMINGS`, but the tests are not, so the timing code in `commandDM()` is not compiled here.
- `appStartup()` success and `appLogic()` are not tested: they start (or join) the saturation and shmim monitor threads, and `appLogic()` sleeps for 5 s on power on.
- `releaseDM()` sleeps for 1 s, so the release tests take several seconds.
