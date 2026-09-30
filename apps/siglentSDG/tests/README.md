# siglentSDG unit tests

Catch2 unit tests for the `siglentSDG` MagAO-X application (`apps/siglentSDG/siglentSDG.hpp`) and its device response parsers (`apps/siglentSDG/siglentSDG_parsers.hpp`).

## Test files

| File | Covers |
|------|--------|
| `siglentSDG_test.cpp` | INDI new-callback validation for the channel properties, `autoPulseWidthFromFrequency()`, and scenario tests of the OUTP, BSWV, MDWV, SWWV, BTWV and ARWV parsers |
| `siglentSDG_parsers_test.cpp` | Parser-only tests (no app object) of every parser in `siglentSDG_parsers.hpp`, including every error code |

## What is tested

- **INDI validation** (`siglentSDG_test.cpp`): `XWCTEST_INDI_NEW_CALLBACK` for `C1`/`C2` `outp`, `freq`, `amp`, `ofst`, `phse`, `wdth`, `wvtp` and `sync`.
- **Pulse width** (`siglentSDG_test.cpp`): `autoPulseWidthFromFrequency()` below, at and above 2 kHz, and at 0 Hz.
- **parseOUTP**: ON/OFF, numeric loads, minimal and newline-terminated responses, multi-digit channels; error codes -1 to -6 and the reset of channel/output to -1.
- **parseBSWV**: SINE (manual example, negative offsets, phase, unit suffixes, exponent notation, trailing newline), PULSE (with `WIDTH` after `DUTY`, and -18 without it), DC (offset only, other outputs reset to 0, -7/-8); header errors -1 to -5, unsupported waveform types (`SDG_PARSEERR_WVTP`), too few fields (-9), each wrong field name (-10 to -17), and that fields before an error are parsed and fields after are not.
- **parseMDWV / parseSWWV / parseBTWV**: the same checks for each (valid ON/OFF with extra fields, unvalidated state values, missing channel number parsing as 0, error codes -1 to -4, the state left unchanged on error).
- **parseARWV**: valid indices, error codes -1 to -4 and the reset of the index to -1.
- **parseSYNC**: ON/OFF with extra fields, error codes -1 to -4 and the reset of `sync` to false.

## Test harness

- `siglentSDG_test.cpp` includes `tests/testMacrosINDI.hpp` before the app header (callback validation mode), and its `siglentSDG_test` subclass sets the device/name of each channel property.
- `siglentSDG_parsers_test.cpp` includes only `../siglentSDG_parsers.hpp`, so it does not construct an app. `bswvResult` holds the 11 `parseBSWV()` outputs, and `makeBSWV()` builds responses from key/value lists so single fields can be altered.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../siglentSDG.hpp or ../siglentSDG_parsers.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/siglentSDG/tests/siglentSDG_test
../apps/siglentSDG/tests/siglentSDG_test
make -f Makefile.one t=../apps/siglentSDG/tests/siglentSDG_parsers_test
../apps/siglentSDG/tests/siglentSDG_parsers_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- The app's telnet communication, `appLogic()` state machine and the live INDI callback bodies are not tested; they need a connected function generator.
- PULSE responses with exactly 20 fields are not parsed in the tests: `parseBSWV()` only checks for 20 fields but reads fields 20 and 21 for PULSE.
