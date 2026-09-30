# xindiserver unit tests

Catch2 unit tests for the `xindiserver` MagAO-X application (`apps/xindiserver/xindiserver.hpp`).

## Test files

| File | Covers |
|------|--------|
| `xindiserver_test.cpp` | Construction of the `indiserver` command line and the local/remote driver argument lists |

## What is tested

- **indiserver options**: `-m`, `-n`, `-p`, `-v` (0 to 4 levels, capped at `-vvv`) and `-x`, singly and all together.
- **Local drivers**: one to three drivers, and rejection of names containing `@`, `/` or `:` and of duplicates.
- **Remote drivers**: one or more drivers on one or two hosts (in order and in arbitrary order) resolved through the ssh tunnel map, and rejection of bad hosts, bad driver names and duplicates.
- **Mixed**: local plus remote drivers, including duplicates across the two lists.

## Test harness

- `xindiserver_test` helper sets the protected `indiserver_*` options, `m_local` and `m_remote`, and returns the tunnel map.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../xindiserver.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/xindiserver/tests/xindiserver_test
../apps/xindiserver/tests/xindiserver_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- Starting `indiserver` and the driver FIFOs is not covered.
