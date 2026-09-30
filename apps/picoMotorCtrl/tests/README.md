# picoMotorCtrl unit tests

Catch2 unit tests for the `picoMotorCtrl` MagAO-X application (`apps/picoMotorCtrl/picoMotorCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `picoMotorCtrl_test.cpp` | Response splitting, configuration and motor-section parsing, INDI callback checks, channel count files, and telemetry |

## What is tested

- **Response splitting**: the free function `splitResponse()` for responses with no address (address 1), one- and two-digit address prefixes, and the three error codes (leading `>`, no response after `>`, non-numeric address).
- **Construction and trivial hooks**: power management enabled, the `dev::ioDevice` default timeouts, `onPowerOff()`, `whilePowerOff()`, `appLogic()` with no connection state, and `appShutdown()` with configured but unstarted channel threads.
- **Configuration**: `loadConfigImpl()` return codes for no motor sections (`PICOMOTORCTRL_E_NOMOTORS`), channel 0 or above `device.nChannels`, address 0 and motor type 0 (`PICOMOTORCTRL_E_BADCHANNEL`), ignored sections without a `channel` key, a valid two-motor config with presets, `loadConfig()` setting `m_shutdown` on error, and the `device.readTimeout`/`writeTimeout` and `telemeter.maxInterval` options.
- **INDI callbacks**: `newCallBack_picopos()` rejects names without `_pos`, unknown channels, and channels whose property has not been created; `newCallBack_presetName()` rejects unknown channels and multiple selected presets, and a single configured preset reaches the channel property.  Both static dispatchers are checked.
- **Channel count files**: `readChannelCounts()`/`writeChannelCounts()` in `<sysPath>/<configName>/`, including a missing file (reads 0), a missing directory (write fails), negative counts and overwriting.
- **Telemetry**: `recordPico()` change detection and forcing, `recordTelem()`, and `checkRecordTimes()` for `telem_pico`.

## Test harness

- `picoMotorCtrl_test` subclass: sets the configuration name, calls `loadConfigImpl()` with the app's configurator, reads config files, and exposes `m_shutdown`, `m_powerMgtEnabled`, `m_sysPath` and `m_maxInterval`.
- The channel map, device address and telnet connection are private members of `picoMotorCtrl`, and the app declares no test friend, so the tests observe them only through the public interface (return codes, callback behaviour, telemetry).
- The INDI callbacks do not use `INDI_VALIDATE_CALLBACK_PROPS`, so there is no `testMacrosINDI.hpp` validation-mode test.
- The count-file test uses a unique directory under `/tmp` that it creates and removes.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../picoMotorCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/picoMotorCtrl/tests/picoMotorCtrl_test
../apps/picoMotorCtrl/tests/picoMotorCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not called: it starts one thread per channel and the telemetry log thread.  Without it the channel INDI properties do not exist, so successful position and preset requests (which also signal the channel thread with `pthread_kill`) are not tested.
- The connection and motor-scan logic of `appLogic()` needs a telnet connection to a controller (and sleeps 4 s), so it is not tested.  `channelThreadExec()` needs a running channel thread and a connection.
- `recordPico()` sizes a static record from `m_nChannels` on its first call, so every app in the telemetry test uses the default 4 channels.
