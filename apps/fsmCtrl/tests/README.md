# fsmCtrl unit tests

Catch2 unit tests for the `fsmCtrl` MagAO-X application (`apps/fsmCtrl/fsmCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `fsmCtrl_test.cpp` | INDI new-callback device/name validation (`XWCTEST_INDI_NEW_CALLBACK`) |
| `binaryUart_test.cpp` | `BinaryUart` packet framing (`ProcessByte`, `CheckPacketStart`, `CheckPacketEnd`, `TxBinaryPacket`) with mock UART, packet, and callback classes |

## What is tested

- **INDI callbacks**: device/name validation of the val1-3, dac1-3, conversion_factors, input, and query callbacks.
- **BinaryUart**: construction defaults, byte processing, packet start/end detection, buffer overflow and invalid-packet callbacks, and binary packet transmission.

## Test harness

- `fsmCtrl_test` subclass of `fsmCtrl` (in namespace `FSMTEST`) that sets the INDI property device/names. `testMacrosINDI.hpp` is included before the app header (validation mode).
- `binaryUart_test.cpp` defines `MockIUart`, `MockIPacket`, `MockBinaryUartCallbacks`, and `MockQuery` to drive `BinaryUart` without a serial device.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../fsmCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/fsmCtrl/tests/fsmCtrl_test
make -f Makefile.one t=../apps/fsmCtrl/tests/binaryUart_test
../apps/fsmCtrl/tests/fsmCtrl_test
../apps/fsmCtrl/tests/binaryUart_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- **These tests are out of date with the app and are not expected to build as written.** `fsmCtrl_test.cpp` includes `../../tests/testMacrosINDI.hpp` (should be `../../../tests/testMacrosINDI.hpp`). `binaryUart_test.cpp` includes `../binaryUart.hpp` and `../iPacket.hpp`, which now live in `libMagAOX/app/dev/summerDeviceUtils/` in namespace `MagAOX::app::dev`, and uses `PZTQuery`, which is commented out in `fsmCommands.hpp` (replaced by `dev::sdevQuery`).
- Neither file follows the current `libXWCTest` namespace and Doxygen conventions.
- The conversion logic, `appLogic()`, shmim monitoring, and telemetry are not tested.
