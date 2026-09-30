# acronameUsbHub unit tests

Catch2 unit tests for the `acronameUsbHub` MagAO-X application (`apps/acronameUsbHub/acronameUsbHub.hpp`).

## Test files

| File | Covers |
|------|--------|
| `acronameUsbHub_test.cpp` | Configuration, port/outlet state logic, channel control and INDI channel callback, `appStartup()`, the `appLogic()` connection state machine, and the power-off hooks, all against a stubbed BrainStem2 API |

## What is tested

- **Construction**: 8 outlets, 0-based outlet numbering, all outlets start unknown, power management disabled; the destructor disconnects the hub.
- **Configuration**: `device.serialNumber` (default 0, override, full `uint32` range) and the `dev::outletController` channel sections (`outlet`/`outlets`, `onOrder`, `offOrder`, `onDelays`, `offDelays`) with 0-based outlets.
- **Channel config errors**: `dev::outletController::loadConfig()` returns `OUTLET_E_NOCHANNELS`, `OUTLET_E_NOVALIDCH`, and -1 for an out-of-range outlet and order/delay size mismatches.
- **Outlet state**: `updateOutletState()` maps bit 0 of the hub port state (other bits ignored), returns -1 for timeout and connection errors, and for other errors silently reports the outlet off; `updateOutletStates()` reads ports 0-7 in order and stops at the first failure.
- **Outlet commands**: `turnOutletOn()`/`turnOutletOff()` call `setPortEnable()`/`setPortDisable()` on the right port, return -1 for timeout and connection errors, and 0 for other errors.
- **Channels**: `channelState()` aggregation; `turnChannelOn()`/`turnChannelOff()` command the ports in the configured on/off order, skip channels already in the requested state, respect `m_stateDelay`, and stop at a failing port.
- **Channel new callback**: `newCallBack_channels()` is rejected when not READY, is case-insensitive, prefers `target` over `state`, ignores unknown targets, and is reached through `st_newCallBack_channels()`.
- **Startup**: `appStartup()` sets up the 0-based `outlet` property, the channel properties and callbacks, and the outlet-list/delay properties, and goes to NOTCONNECTED without connecting.
- **State machine**: `appLogic()` connects over `USB` with the configured serial number, queries the system entity, goes READY and reads the ports in the same call; retries failed connections; disconnects and goes NOTCONNECTED on a lost connection; ignores port read errors; does nothing in other states.
- **Power off**: `onPowerOff()` disconnects if connected and marks all outlets off; `whilePowerOff()` and `appShutdown()` return 0.

## Test harness

- `acronameUsbHub_test` subclass: sets `m_configName`, resets the BrainStem stub, runs `setupConfig()`/`readConfig()`/`loadConfig()` (or only the `dev::outletController` part, to see its return code), and exposes the protected members with `using` declarations.
- `stubs/BrainStem2/BrainStem-all.h`: a stand-in for the vendored BrainStem2 SDK header, declaring only the API subset the app uses (`aErr`, `linkType`, `Module::connect/isConnected/disconnect`, `SystemClass`, `USBClass` port functions, `aUSBHub3p`, `aDefs_GetModelName`, `aVersion_ParseString`). The bodies and a fake hub (`brainStemStubState`: return codes, port state bits, call counts, captured arguments) are defined in `acronameUsbHub_test.cpp`.
- Why a stub rather than the vendored SDK: the shared test build does not add `-I libs/BrainStem2/` nor link `libs/BrainStem2/libBrainStem2.a`, and the real library would try to talk to USB hardware. The stub keeps the test a single translation unit with no extra libraries, and lets the tests inject errors and check the exact port commands.
- Callback bodies run live (`testMacrosINDI.hpp` is not used): the channel callbacks come from `dev::outletController` and dispatch on the property name, not through `INDI_NEWCALLBACK_DECL`, so the `XWCTEST_INDI_*` macros do not apply.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../acronameUsbHub.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/acronameUsbHub/tests/acronameUsbHub_test
../apps/acronameUsbHub/tests/acronameUsbHub_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The real BrainStem2 library and hub hardware are not exercised; the stub only models the calls the app makes.
- INDI publication (`updateINDI()`, `updateIfChanged()`) is a no-op without an INDI driver, so published values are not checked.
- The `outletController` range check accepts outlet 8 on this 8-port (0-based) hub (it uses `>` rather than `>=`), so that boundary is not asserted.
