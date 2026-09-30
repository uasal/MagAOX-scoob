# sshDigger unit tests

Catch2 unit tests for the `sshDigger` MagAO-X application (`apps/sshDigger/sshDigger.hpp`).

## Test files

| File | Covers |
|------|--------|
| `sshDigger_test.cpp` | Tunnel configuration parsing and tunnel exec preparation |

## What is tested

- **Configuration**: one- and two-tunnel config files: fully specified tunnels (with and without a monitor port), no matching tunnel for the config name, and missing remote host, local port, or remote port in either tunnel.
- **Exec preparation**: the `tunnelSpec()` string and the exec argv vector for the ssh tunnel.

## Test harness

- `sshDigger_test` subclass of `sshDigger` that sets the config name and exposes the remote host and the local, remote, and monitor ports.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../sshDigger.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/sshDigger/tests/sshDigger_test
../apps/sshDigger/tests/sshDigger_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- No ssh process is started, and the tunnel monitoring in `appLogic()` is not tested.
- The test defines `SSHDIGGER_TEST_NOINDI` and `SSHDIGGER_TEST_NOLOG`, but `sshDigger.hpp` does not currently use these macros.
