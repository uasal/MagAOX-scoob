# xInstGraph unit tests

Catch2 unit tests for the `xInstGraph` MagAO-X application (`apps/xInstGraph/xInstGraph.hpp`) and for its instrument-graph node classes (`apps/xInstGraph/xigNodes/`).

The node tests live in `../xigNodes/tests/`; this directory's `Makefile` builds them too.

## Test files

| File | Covers |
|------|--------|
| `xInstGraph_test.cpp` | Configuring and running the app with a minimal draw.io graph holding one node of each type |
| `../xigNodes/tests/xigNode_test.cpp` | `xigNode` base class: construction with a valid graph, node missing from the XML, null parent graph |
| `../xigNodes/tests/fsmNode_test.cpp` | `fsmNode` configuration: default config and per-state actions (`threshOff`, `active`) |
| `../xigNodes/tests/indiPropNode_test.cpp` | `indiPropNode` configuration (tolerance, text/number/switch properties) and invalid configurations |
| `../xigNodes/tests/pwrOnOffNode_test.cpp` | `pwrOnOffNode` configuration with and without the power key |
| `../xigNodes/tests/staticNode_test.cpp` | `staticNode` configuration |
| `../xigNodes/tests/stdMotionNode_test.cpp` | `stdMotionNode` configuration, invalid configurations, and handling of sent properties with tracking |

## What is tested

- **App configuration** (`xInstGraph_test.cpp`): `setupConfig()`/`loadConfig()` with a graph file and output path, one config section per node type, followed by `appStartup()`, `appLogic()` and `appShutdown()`.
- **Node construction**: nodes found or not found in the draw.io XML, null parent graphs, and wrong node types.
- **Node configuration**: required keys (`propKey`, `propEl`, `propVal`, `pwrKey`, `device`, `presetName`, `presetDir`, `presetPutName`, tracking keys/elements) and rejection of missing, empty or inconsistent values.
- **Property handling**: `indiPropNode` and `stdMotionNode` responses to INDI property updates.

## Test harness

- `xInstGraph_test.cpp` defines a local `xInstGraph` subclass that sets the config directory and exposes `config`; the graph XML and config file are written under `/tmp/xInstGraph_test/`.
- The node tests write small draw.io XML and config files to `/tmp` and construct nodes against a parent `ingr::instGraphXML`; `xigNode_test.cpp` uses a `test_xigNode` subclass of the abstract base.
- All of these tests link `-linstGraph`; `tests/Makefile.one` adds it by test name, so `TESTLIBS` is not set here.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository), and the instGraph library must be installed.

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../xInstGraph.hpp or ../xigNodes/*.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/xInstGraph/tests/xInstGraph_test
../apps/xInstGraph/tests/xInstGraph_test
make -f Makefile.one t=../apps/xInstGraph/xigNodes/tests/xigNode_test
../apps/xInstGraph/xigNodes/tests/xigNode_test
```

Substitute any of the other node test names (`fsmNode_test`, `indiPropNode_test`, `pwrOnOffNode_test`, `staticNode_test`, `stdMotionNode_test`) for `xigNode_test`.

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `xInstGraph_test.cpp` has a single configuration case; graph output and live INDI updates through the app are not checked.
- `../xigNodes/tests/making_tests.md` refers to an older `utils/instGraph` path.
