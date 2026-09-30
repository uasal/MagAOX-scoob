# observerCtrl unit tests

Catch2 unit tests for the `observerCtrl` MagAO-X application (`apps/observerCtrl/observerCtrl.hpp`).

## Test files

| File | Covers |
|------|--------|
| `observerCtrl_test.cpp` | INDI callback validation and stream-writer management |

## What is tested

- **INDI callbacks**: device/name validation of the observers, obsName, observing, and sws callbacks.
- **Stream writers**: tracking the remote writing state, stopping only the writers this app started, default writers being managed but not selectable, and not stopping writers whose initial state is unknown.

## Test harness

- `observerCtrl_test` subclass of `observerCtrl` that sets the INDI property device/names and configures the stream-writer list and properties directly.
- `tests/testMacrosINDI.hpp` is included before the app header (validation mode).

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../observerCtrl.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/observerCtrl/tests/observerCtrl_test
../apps/observerCtrl/tests/observerCtrl_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Commands to the stream writers are not sent, since no INDI server is running.
- Observer file loading and the observation-log logic are not tested.
