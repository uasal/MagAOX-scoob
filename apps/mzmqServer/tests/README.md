# mzmqServer unit tests

Catch2 unit tests for the `mzmqServer` MagAO-X application (`apps/mzmqServer/mzmqServer.hpp`).

## Test files

| File | Covers |
|------|--------|
| `mzmqServer_test.cpp` | Configuration, compression, the server and image thread lifecycle in `appStartup()`/`appLogic()`/`appShutdown()`, and status reporting |

## What is tested

- **Configuration**: defaults and overrides of `server.imagePort`, `server.shmimNames`, `server.usecSleep`,
  `server.fpsTgt`, `server.fpsGain` and `server.compress`, the invoked name set from the configuration name, and power
  management being disabled.
- **Compression**: `appStartup()` selects the default xrif compression only when `compress` is set.
- **Startup**: `appStartup()` adds one image stream per configured name, starts the server thread and one image thread
  per stream; a failed server or image thread start returns -1.
- **appLogic**: returns 0 while all threads run, and -1 once the server thread or an image thread has exited.
- **Shutdown**: `appShutdown()` sets `m_timeToDie`, signals the server and every image thread, and joins them.
- **Reporting**: the `reportInfo()`/`reportNotice()`/`reportWarning()`/`reportError()` log overrides, also when
  called through the milkzmq base class.

## Test harness

- `mzmqServer_test` subclass sets the device name, resets the stand-in state and `m_timeToDie`, exposes the protected
  members with `using` declarations, wraps the configuration steps, and inspects the threads.  After `appLogic()` has
  joined an exited thread with `pthread_tryjoin_np()`, the harness moves the stale `std::thread` into an intentionally
  leaked object so it is not joined twice or destroyed while joinable.
- `stubs/milkzmqServer.hpp`: a stand-in for the milkzmq server header (the real one is in
  `/opt/MagAOX/source/milkzmq`, which is not on the test include path, and needs ZeroMQ and ImageStreamIO).  It
  declares only what the app and the tests use, and takes the compression constants from `xrif/xrif.h`.  The member
  functions are defined in `mzmqServer_test.cpp`: the thread start functions record their calls and start threads that
  wait for `m_timeToDie` (or exit immediately, when selected), or return an injected error; the kill functions and
  `defaultCompression()` record their calls.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../mzmqServer.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/mzmqServer/tests/mzmqServer_test
../apps/mzmqServer/tests/mzmqServer_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The ZeroMQ serving of images (`milkzmqServer::serverThreadExec()` and `imageThreadExec()`) is part of milkzmq and is
  not tested here.
