# template unit tests

Catch2 unit tests for the `template` MagAO-X application skeleton (`apps/template/template.hpp`).

These files are the starting point for the tests of a new app: they are copied along with the rest of the
`template` directory.

## Test files

| File | Covers |
|------|--------|
| `template_test.cpp` | Configuration defaults and overrides, the INDI device name, and the lifecycle functions |

## What is tested

- **Configuration**: `setupConfig()`/`loadConfig()` with a defaults-only file and with an override file, checking that
  `loadConfigImpl()` succeeds and `m_shutdown` stays 0.
- **INDI**: the harness sets the INDI device name (`m_configName`), as needed by the INDI test macros.
- **Lifecycle**: `appStartup()`, `appLogic()` and `appShutdown()` return 0.

## Test harness

- `template_test` subclass sets the device name, wraps `setupConfig()`/`readConfig()`/`loadConfig()`, calls
  `loadConfigImpl()` on the app's configurator, and reads `m_shutdown`.  Protected members of a new app can be exposed
  with `using` declarations in the harness.
- The INDI test macros in `tests/testMacrosINDI.hpp` require the harness to be named `<app>_test` and to be
  constructible from a device name.

## Adapting these tests for a new app

After copying `template` to the new app directory (e.g. `hardwareCtrl`, see `../readme.md`):

1. Rename `template_test.cpp` to `hardwareCtrl_test.cpp`.
2. In `hardwareCtrl_test.cpp`, this `README.md`, and `Makefile`, replace `template` with `hardwareCtrl` and
   `TEMPLATE` with `HARDWARECTRL` (the replacement must be case sensitive; the upper case name is used in the
   `HARDWARECTRL_TEST_DOXYGEN_REF` blocks).
3. For each configurable parameter, add its default to the defaults test and a section/keyword/value to the
   `writeConfigFile()` call in the overrides test, and check the resulting member values.
4. For each INDI NEW property, set its device and name in the harness constructor
   (`XWCTEST_SETUP_INDI_NEW_PROP( prop )`) and add `XWCTEST_INDI_NEW_CALLBACK( hardwareCtrl, prop )` checks, with
   `tests/testMacrosINDI.hpp` included before the app header.  To test what a callback does, use a separate test file
   that includes `testMacrosINDI.hpp` after the app header (or not at all), so the callback bodies run.
5. Add tests for the app's own logic: parsers, unit conversions, state handling, and error paths.  Put stand-ins for
   vendor SDK headers in `stubs/` (it is put first on the include path).
6. List each test in `TESTS` in the `Makefile`, update this README, and add
   `../apps/hardwareCtrl/tests/hardwareCtrl_test` to `tests/tests.list`.
7. Add the `\defgroup hardwareCtrl_unit_test` block (already present after the rename) and keep a `///` brief and
   `\ingroup` block, plus a `#ifdef HARDWARECTRL_TEST_DOXYGEN_REF` block, for each `TEST_CASE`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../template.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/template/tests/template_test
../apps/template/tests/template_test
```

## Limitations

- `template` is a C++ keyword, so neither `template.hpp` nor these tests compile until the name is replaced.  For the
  same reason `template_test` is not listed in `tests/tests.list`.
- The skeleton has no configurable parameters and no INDI properties, so the tests only show the pattern; the
  commented examples mark where the new app's checks go.
