# dmCtrl unit tests

Catch2 unit tests for the `dmCtrl` MagAO-X application (`apps/dmCtrl/dmCtrl.hpp` and `apps/dmCtrl/dmCommands.hpp`).

## Test files

| File | Covers |
|------|--------|
| `dmCtrl_test.cpp` | The DM command and telemetry structures in `dmCommands.hpp`: payload types, telemetry payload comparison/assignment, and `TelemetryQuery` reply parsing and logging |

## What is tested

- **Payload types**: the `CGraphPayloadTypeDM*` command codes and their uniqueness.
- **Telemetry payload**: `CGraphDMTelemetryPayload` equality (by value and by pointer, every field) and assignment (by reference and by pointer).
- **Query base class**: `PZTQuery` payload accessors, `setPayload()`/`resetPayload()` (null and non-null defaults), and virtual dispatch through a `std::vector<PZTQuery*>` as used by the app.
- **Telemetry query**: `TelemetryQuery` construction (payload type, log strings), `processReply()` decoding exact-length and over-length replies, rejection of short, empty, and null replies (telemetry left unchanged), `errorLogString()`, and `logReply()`.

## Test harness

- `dmCommands.hpp` is included directly after `libMagAOX.hpp`, with the `MagAOXAppT` typedef it needs for logging (normally provided by `dmCtrl.hpp`).
- `testQuery` is a minimal concrete `PZTQuery` that counts calls and allows a non-null default payload.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../dmCtrl.hpp or ../dmCommands.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/dmCtrl/tests/dmCtrl_test
../apps/dmCtrl/tests/dmCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The `dmCtrl` class itself (configuration, `appLogic()`, serial connection, telemetry recording, shmim callbacks) is not tested because `dmCtrl.hpp` does not currently compile: it uses `CGraphPacket`, `IUart`, `BinaryUart`, `linux_pinout_uart` and `PinoutConfig` from `MagAOX::app::dev` without qualification, passes an undefined `PacketCallbacks`, passes a `std::vector<PZTQuery*>` where `BinaryUart` expects `std::vector<dev::sdevQuery*>`, and `commandDM()` does not return a value. The app is also not in the top-level `Makefile` app lists.
- `dmCtrl` does not derive from `dev::dm` and has no INDI properties or callbacks of its own, so there are no INDI callback tests.
