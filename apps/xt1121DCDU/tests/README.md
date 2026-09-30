# xt1121DCDU unit tests

Catch2 unit tests for the `xt1121DCDU` MagAO-X application (`apps/xt1121DCDU/xt1121DCDU.hpp`).

## Test files

| File | Covers |
|------|--------|
| `xt1121DCDU_test.cpp` | Configuration, channel/outlet mapping, outlet and channel state logic, INDI set/new callbacks, `appStartup()` and `appLogic()` |

## What is tested

- **Construction**: 8 outlets, 1-based outlet numbering (`m_firstOne`), all outlets start unknown.
- **Configuration**: `device.name`, default (`0-7`) and overridden `device.channelNumbers`, and the `dev::outletController` channel sections (`outlet`/`outlets`, `onOrder`, `offOrder`, `onDelays`, `offDelays`) with the 1-based to 0-based outlet conversion.
- **Channel config errors**: `dev::outletController::loadConfig()` returns `OUTLET_E_NOCHANNELS`, `OUTLET_E_NOVALIDCH`, and -1 for outlet 0, an out-of-range outlet, and order/delay size mismatches.
- **Mapping helpers**: `xtChannelName()` (`ch00`-`ch16`, empty otherwise) and `xtChannelProperty()` (one distinct property per outlet 0-7, `nullptr` otherwise).
- **Outlet state**: `updateOutletState()` maps the xt1121Ctrl `current` element (0 off, 1 on, otherwise or missing unknown) and rejects bad outlet numbers; `updateOutletStates()` updates every outlet.
- **Set callbacks**: `setCallBack_ip_ch0`-`setCallBack_ip_ch7` store the received property and update only their own outlet (they do not validate the device or name).
- **Outlet commands**: `turnOutletOn()`/`turnOutletOff()` reject bad outlets and return -1 for valid ones because no INDI driver exists; the INDI mutex is released afterwards.
- **Channels**: `channelState()` aggregation (unknown, off, intermediate, on); `turnChannelOn()`/`turnChannelOff()` are no-ops when the channel is already in the requested state and otherwise attempt the outlet requests.
- **Channel new callback**: `newCallBack_channels()` is rejected when not READY, is case-insensitive, prefers `target` over `state`, ignores unknown targets, and is reached through `st_newCallBack_channels()`.
- **Startup**: `appStartup()` registers the eight xt1121Ctrl set properties as `<device>.chXX` from `device.channelNumbers`, sets up the outlet, channel, outlet-list and delay properties, and goes to NOTCONNECTED; it fails for other than 8 channel numbers and for duplicate channel numbers.
- **State machine**: `appLogic()` goes from POWERON to READY and updates outlet states, skips the update while `m_indiMutex` is held, and fails from other states; `appShutdown()` returns 0.

## Test harness

- `xt1121DCDU_test` subclass: sets `m_configName`, runs `setupConfig()`/`readConfig()`/`loadConfig()` (or only the `dev::outletController` part, to see its return code), and exposes the protected members, helpers and set callbacks with `using` declarations.
- Callback bodies run live (`testMacrosINDI.hpp` is not used): the set callbacks do no device/name validation, so `XWCTEST_INDI_SET_CALLBACK` does not apply.
- No INDI driver exists, so requests sent to the xt1121Ctrl (`sendNewProperty()`) fail with -1; the tests use this to see whether a request was attempted.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../xt1121DCDU.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/xt1121DCDU/tests/xt1121DCDU_test
../apps/xt1121DCDU/tests/xt1121DCDU_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Successful outlet requests (and the on/off ordering and delays) are not tested because `sendNewProperty()` needs a running INDI driver.
- `newCallBack_channels()` is not called with an unconfigured channel name: `channelState()` indexes the empty outlet list of the new map entry.
