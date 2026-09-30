# xt1121Ctrl unit tests

Catch2 unit tests for the `xt1121Ctrl` MagAO-X application (`apps/xt1121Ctrl/xt1121Ctrl.hpp`) and its channel/register helper (`apps/xt1121Ctrl/xtChannels.hpp`).

## Test files

| File | Covers |
|------|--------|
| `xt1121Ctrl_test.cpp` | Configuration, INDI properties and new callbacks, the channel target/current logic, `getState()`, the `appLogic()` state machine and the power-off/shutdown hooks |
| `xtChannels_test.cpp` | `xt1121Channels` conversion between modbus register bitmasks and channel states, including input-only channels |

## What is tested

- **Construction**: port 502, no address, no modbus object, callbacks enabled, `m_powerOnWait` of 2 s, 16 channels in 4 registers, all channels clear.
- **Configuration**: defaults, overrides of `device.address`, `device.port` and `device.inputOnly`, and out-of-range input-only channels being skipped.
- **Startup**: `appStartup()` creates the 16 `chXX` Number properties with `current` (-1) and `target` elements and registers their callbacks; a second call fails on the duplicate registration.
- **INDI validation**: `XWCTEST_INDI_NEW_CALLBACK` for `ch00`-`ch15` (wrong device, wrong name, right device.name), plus a callback given another channel's property.
- **Channel callbacks**: each `newCallBack_m_indiP_chXX` sets/clears only its own channel; `channelSetCallback()` uses `target` over `current`, treats any non-zero value as set, ignores requests with neither, respects input-only channels, does nothing when callbacks are disabled or during shutdown, releases `m_indiMutex`, and produces the expected register bits.
- **getState()**: -1 without a modbus connection, 0 during shutdown.
- **State machine**: POWERON waits `m_powerOnWait` loop periods before NOTCONNECTED; NOTCONNECTED/ERROR do nothing while powered off; CONNECTED, READY and OPERATING go to ERROR when the register read fails while powered, READY stays READY when powered off.
- **Power off/shutdown**: `onPowerOff()` and `whilePowerOff()` return 0 and release `m_indiMutex`; `appShutdown()` disables the callbacks.
- **xtChannels**: register-to-channel reads, channel-to-register writes, and input-only masking (see `xtChannels_test.cpp`).

## Test harness

- `xt1121Ctrl_test` subclass: sets `m_configName` and the channel property device/names (as `appStartup()` does), runs `setupConfig()`/`readConfig()`/`loadConfig()`, exposes protected members with `using` declarations, and gives by-number access to the channel properties and callbacks.
- `tests/testMacrosINDI.hpp` is included after the app header, so the callback bodies run live. A matching property with no elements makes `channelSetCallback()` return 0, so `XWCTEST_INDI_NEW_CALLBACK` still applies.
- No modbus connection is made: `m_mb` stays `nullptr`, so `channelSetCallback()` updates the channel state and returns before writing, and `getState()` fails. `appLogic()` is only driven in states or power states that do not construct a `modbus` object.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../xt1121Ctrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/xt1121Ctrl/tests/xt1121Ctrl_test
../apps/xt1121Ctrl/tests/xt1121Ctrl_test
make -f Makefile.one t=../apps/xt1121Ctrl/tests/xtChannels_test
../apps/xt1121Ctrl/tests/xtChannels_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The modbus read/write paths (a successful `getState()`, register writes in `channelSetCallback()`, and NOTCONNECTED -> CONNECTED) are not tested: `modbus` is a concrete class that opens a TCP socket, and there is no injection point for a fake.
- Published INDI values (`updateIfChanged()` of `current`/`target`) are not checked because there is no INDI driver in the tests.
