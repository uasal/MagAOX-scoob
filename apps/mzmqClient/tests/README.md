# mzmqClient unit tests

Catch2 unit tests for the `mzmqClient` MagAO-X application (`apps/mzmqClient/mzmqClient.hpp`).

## Test files

| File | Covers |
|------|--------|
| `mzmqClient_test.cpp` | Configuration, the image thread lifecycle in `appStartup()`/`appLogic()`/`appShutdown()`, and status reporting |

## What is tested

- **Configuration**: defaults and overrides of `server.address`, `server.imagePort` and `server.shmimNames`, the
  invoked name set from the configuration name, and power management being disabled.
- **Startup**: `appStartup()` adds one image stream per configured name and starts one image thread for each; a
  failed thread start stops startup and returns -1.
- **appLogic**: returns 0 while the image threads run, and -1 once an image thread has exited.
- **Shutdown**: `appShutdown()` sets `m_timeToDie`, signals every image thread, and joins them.
- **Reporting**: the `reportInfo()`/`reportNotice()`/`reportWarning()`/`reportError()` log overrides, also when
  called through the milkzmq base class.

## Test harness

- `mzmqClient_test` subclass sets the device name, resets the stand-in state and `m_timeToDie`, exposes the protected
  members with `using` declarations, wraps the configuration steps, and inspects the image threads.  After
  `appLogic()` has joined an exited thread with `pthread_tryjoin_np()`, the harness replaces (and intentionally leaks)
  the stale `std::thread` object so it is not joined twice or destroyed while joinable.
- `stubs/milkzmqClient.hpp`: a stand-in for the milkzmq client header (the real one is in
  `/opt/MagAOX/source/milkzmq`, which is not on the test include path, and needs ZeroMQ).  It declares only what the
  app uses.  The member functions are defined in `mzmqClient_test.cpp`: `imageThreadStart()` records its calls and
  starts a thread that waits for `m_timeToDie` (or exits immediately, for a chosen thread), or returns an injected
  error; `imageThreadKill()` records its calls.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../mzmqClient.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/mzmqClient/tests/mzmqClient_test
../apps/mzmqClient/tests/mzmqClient_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The ZeroMQ transfer of images (`milkzmqClient::imageThreadExec()`) is part of milkzmq and is not tested here.
