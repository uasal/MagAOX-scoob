# streamCircBuff unit tests

Catch2 unit tests for the `streamCircBuff` MagAO-X application (`apps/streamCircBuff/streamCircBuff.hpp`).

## Test files

| File | Covers |
|------|--------|
| `streamCircBuff_test.cpp` | Configuration, pixel getter selection, output geometry, semaphore frame handoff, float conversion, producer-burst characterization |

## What is tested

- **Configuration**: defaults (input and output shmim names derived from the config name, circular buffer length 1,
  latency buffer settings, telemetry interval, log name and extension) and overrides of the `shmimMonitor`,
  `framegrabber` and `telemeter` keys through `setupConfig()`/`loadConfigImpl()`, plus clamping of
  `framegrabber.circBuffLength = 0` and a negative `framegrabber.latencyTime`.
- **allocate()**: pixel getter selection for uint16, int16 and double inputs, a null getter for an unsupported type,
  and setting the framegrabber `m_reconfig` flag.
- **configureAcquisition()**: copying the input width/height to the framegrabber with a float output type, and the
  `-1` return when no input stream is connected; `startAcquisition()`, `reconfig()` and `fps()` trivial returns.
- **Frame handoff**: `processImage()` storing the source pointer and posting the semaphore exactly once;
  `acquireAndCheckValid()` returning 0 with a timestamp for a posted frame (including a frame posted from another
  thread) and 1 on the 1 s timeout.
- **loadImageIntoStream()**: `-1` with no source frame, and uint16/int32/float to float conversion in pixel order,
  writing exactly `width*height` pixels.
- **Producer burst**: characterizes the current single-pointer handoff described in
  `agents/plans/2026-03/streamCircBuff-concurrency-hardening.md` (two queued frames are both delivered as the newest
  frame). This test must be updated when the ordered frame queue is implemented.

## Test harness

- `streamCircBuff_test` subclass initialises the handoff semaphore (normally done in `appStartup()`) and destroys it
  on destruction, sets the input stream geometry directly on the shmimMonitor base, and exposes the protected
  framegrabber hooks and base-class configuration members through small accessors.
- No shared memory streams, INDI server or app threads are used.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../streamCircBuff.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/streamCircBuff/tests/streamCircBuff_test
../apps/streamCircBuff/tests/streamCircBuff_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()`, `appLogic()` and `appShutdown()` are not called, since they start the shmimMonitor, framegrabber
  and telemetry threads and register INDI properties.
- `recordTelem()`/`checkRecordTimes()` are not tested (they write telemetry).
- The "no input stream" and timeout cases each wait about 1 second.
