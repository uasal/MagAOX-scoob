# kTracker unit tests

Catch2 unit tests for the `kTracker` MagAO-X application (`apps/kTracker/kTracker.hpp`).

## Test files

| File | Covers |
|------|--------|
| `kTracker_test.cpp` | INDI callback validation, configuration, `appStartup()`, the tracking and `teldata` callbacks, and `appLogic()` update bookkeeping |

## What is tested

- **INDI validation**: `newCallBack_m_indiP_tracking` and `setCallBack_m_indiP_teldata` reject wrong device/property names.
- **Configuration**: defaults and overrides of `k.zero`, `k.sign`, `k.devName`, `tcs.devName` and `tracking.updateInterval`.
- **Startup**: `appStartup()` builds the `tracking` toggle, the `teldata` subscription and the outbound stage `position` property, registers the callbacks, and sets `READY`.
- **Tracking toggle**: turning tracking on/off sets `m_tracking` and resets the update timestamp; requests without a `toggle` element are ignored.
- **teldata**: valid zenith distances are stored and flagged; wrong-device updates and updates without `zd` are ignored.
- **appLogic**: idles and resets the timestamp when not tracking, waits for a first zenith distance, dispatches once and then honors `m_updateInterval`, and re-arms after a toggle.

## Test harness

- `kTracker_test` subclass exposes the protected members with `using` declarations and adds helpers to read the registered callback maps.
- `testMacrosINDI.hpp` is included after the app header, so the callback bodies run; the validation macros still pass because both callbacks return 0 for a property with no elements.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../kTracker.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/kTracker/tests/kTracker_test
../apps/kTracker/tests/kTracker_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The computed K-mirror target (`m_zero + m_sign * 0.5 * zd`) is not observable: `sendNewProperty()` fails without an INDI driver and sends a copy of `m_indiP_kpos`, so only the dispatch bookkeeping is checked.
