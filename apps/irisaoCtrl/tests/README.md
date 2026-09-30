# irisaoCtrl unit tests

Catch2 unit tests for the `irisaoCtrl` MagAO-X application (`apps/irisaoCtrl/irisaoCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `irisaoCtrl_test.cpp` | Configuration, the `dev::dm` hooks (`initDM`, `zeroDM`, `commandDM`, `releaseDM`) against a stubbed IrisAO SDK, shutdown/power-off, and the `dev::dm` INDI request callbacks |

## What is tested

- **Configuration**: defaults (power management enabled, empty serial numbers and `calibRelDir`) and overrides of `dm.mserialNumber`, `dm.dserialNumber`, `dm.hardwareDisable`, `dm.calibRelDir` and `dm.shmimName` (derived stream names and calibration paths).
- **initDM**: upper-case mirror and driver serial numbers and the hardware-disable flag passed to `MirrorConnect`, segment counting (3 actuators per segment), zeroing, state `OPERATING`, refusal of a second init, and an exception from `MirrorConnect`.
- **zeroDM**: not-open and zero-actuator errors, and zeroing every segment followed by one `MirrorSendSettings`.
- **commandDM**: piston/tip/tilt of each segment taken from consecutive stream values, one `MirrorSendSettings`, and the instantaneous saturation map flagging all three actuators of an unreachable segment.
- **releaseDM / appShutdown / onPowerOff**: the not-open error, zero and release sequence, handle reset and state `READY`, and the zeroing-failure abort.
- **INDI**: device/name validation of the `initDM`, `zeroDM`, `releaseDM` and `zeroAll` callbacks, and the effect of `initDM` (state check), `zeroDM` and `releaseDM` requests.

## Test harness

- `irisaoCtrl_test` subclass exposes protected members, sets `m_calibDir` under `/tmp/irisaoCtrl_test`, initializes `m_hardwareDisable` (which the app leaves uninitialized unless configured), creates the `dev::dm` request properties, and provides `fakeInit()` to put the app in a connected state without `initDM()`. `fakeInit()` also sets `m_shutdown` so `releaseDM()` does not signal the never-started shmim monitor thread.
- `stubs/irisao.mirrors.h`: stand-in for the IrisAO SDK header, declaring only the types and functions used by the app. The stub bodies and their fake state (`irisaoStubState`: segment count, unreachable segments, a connection exception, captured serial numbers, positions, and command/release counts) are defined in `irisaoCtrl_test.cpp`, so the test does not link the IrisAO library.
- `testMacrosINDI.hpp` is included after the app header so callback bodies stay live.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../irisaoCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/irisaoCtrl/tests/irisaoCtrl_test
../apps/irisaoCtrl/tests/irisaoCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `initDM()` sleeps for 2 s up to three times and `releaseDM()` sleeps for 1 s, so these tests take about 20 s in total. The `initDM` INDI request is only tested for the wrong-state rejection to avoid another slow `initDM()`.
- `appStartup()` and `appLogic()` are not tested: they start (or join) the saturation and shmim monitor threads, and `appLogic()` sleeps for 5 s on power on.
