# streamWriter unit tests

Catch2 unit tests for the `streamWriter` MagAO-X application (`apps/streamWriter/streamWriter.hpp`).

## Test files

| File | Covers |
|------|--------|
| `streamWriter_test.cpp` | INDI callback validation, writing-state toggles, buffer allocation, XRIF encoding and fault injection, configuration defaults |
| `streamWriterSizing_test.cpp` | Circular-buffer and write-chunk sizing from the configured maximum buffer size |
| `streamWriter_lifecycle_test.cpp` | Config loading, `appStartup()`/`appLogic()`/`appShutdown()` lifecycle, and the `fgThreadExec()` ingest loop against a real temporary shmim |

## What is tested

- **Configuration**: defaults, shmim-derived paths and names, explicit overrides, and clamping of large LZ4 accelerations to the XRIF limit.
- **Buffer sizing**: default and large frame sizes, frames too large for the maximum size, LOWFS-like 32x32 to 3200x3200 cases, rounding of odd circular buffers and promotion/shrinking of write chunks so they divide the circular buffer.
- **Writing state**: valid and invalid start/stop transitions, stopping with and without queued frames.
- **Encoding**: full, partial, single-frame and shorter follow-up saves are decoded and compared to the source data; uncompressed XRIF; rejection of zero first timestamps; directory creation and file-open failures.
- **Fault injection**: `xrif_configure`, `xrif_set_size`, `xrif_allocate_*`, LZ4 setup, encode and header-write failures or warnings, and short `fwrite()` calls.
- **Lifecycle**: `appStartup()` failures (save directory, chunk/buffer mismatch), a nominal idle lifecycle, start-writing-on-startup, `appLogic()` backlog/skip reporting, and save telemetry helpers.
- **Frame-grabber loop**: `fgThreadExec()` ingests cube and 2D streams, counts skipped frames, posts save work on chunk boundaries, timeouts and stop requests, handles shmim replacement, and does not hang on restart cleanup.
- **INDI**: `XWCTEST_INDI_NEW_CALLBACK` validation for `writing`.

## Test harness

- `streamWriter_test.cpp` redirects the XRIF calls and `fwrite()` with macros to `streamWriter_test_*` wrappers (state in `streamWriterFaultInject`) before including the app header with `protected` exposed, so individual calls can fail, warn or short-write on a chosen call number.
- `streamWriter_lifecycle_test.cpp` uses `streamWriterLifecycleTest`, which builds an isolated MagAO-X directory tree under `/tmp`, points ImageStreamIO at a sandbox, and runs `fgThreadExec()` on its own thread; `tempStream` creates a temporary uint16 shmim, and `startupScope`/`fgHarnessScope` guarantee shutdown.
- `streamWriterSizing_test.cpp` calls the sizing code on a default-constructed app.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../streamWriter.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/streamWriter/tests/streamWriter_test
../apps/streamWriter/tests/streamWriter_test
```

Substitute any of the other test names (`streamWriterSizing_test`, `streamWriter_lifecycle_test`) for `streamWriter_test`.

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The lifecycle tests create real ImageStreamIO shared memory and threads, with short timeouts; they run slower than the other tests.
- Writing to a real telemetry/log system is not covered.
