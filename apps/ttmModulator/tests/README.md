# ttmModulator unit tests

Catch2 unit tests for the `ttmModulator` MagAO-X application (`apps/ttmModulator/ttmModulator.hpp`).

## Test files

| File | Covers |
|------|--------|
| `ttmModulator_test.cpp` | Configuration, `calcState()`, `waitValue()`, the INDI callback bodies, offsets, `restTTM()`, `setTTM()`, `modTTM()` and the `appLogic()` state machine |
| `ttmModulator_indi_test.cpp` | INDI callback device/name validation (validation mode) |

## What is tested

- **Configuration**: the `limits.maxfreq` and `cal.*` keywords, defaults and overrides, the rotation angle converted from degrees to radians, and `cal.rotParity` normalized to +/-1.
- **calcState**: rest (either output off or unknown), set (no modulation, zero phase), midset (non-zero phase, different channel frequencies, one channel modulating), and modulating, with the radius from the calibration table at a calibration frequency, interpolated between frequencies, below the first frequency, at the last frequency, and scaled by the calibration radius.
- **waitValue**: exact and tolerance matches, and timeouts.
- **Callbacks**: `modState`, `modRadius` and `modFrequency` requests (target or current, only positive radius/frequency accepted, missing elements and wrong device/name rejected); the function generator `C<n>outp` (On/Off/other), `C<n>freq` and `C<n>amp` (current), `C<n>ofst` and `C<n>phse` (value) set callbacks, with missing elements and wrong device/name rejected.
- **Offsets**: `offset12()` and the `offset12` callback add to each channel's offset; `offsetXY()` and the `offset` callback apply the rotation and parity; without INDI they fail.
- **restTTM**: sends the ten rest requests and ends with the outputs off and minimum amplitude and offset; a send failure stops it.
- **setTTM**: does nothing when set; from rest turns the outputs on and sets the offsets; from modulating stops the modulation and clears the modulation parameters; from midset rests first; refuses set voltages above 10 V; a send failure stops it.
- **modTTM**: ignores negative requests; from set or rest starts modulating with the calibrated amplitudes and phase (at and between calibration frequencies, with the frequency ramp); applies the frequency and voltage limits; changing the modulation stops and restarts it; the current parameters again do nothing.
- **appLogic**: does nothing at power off; the FSM state follows the modulator state (rest: NOTHOMED, set: READY, midset: ERROR, modulating: OPERATING); set, modulate (including a request with no radius/frequency) and rest requests are carried out and cleared; without INDI a request fails but `appLogic()` continues.
- **INDI validation** (`ttmModulator_indi_test.cpp`): every new and set callback rejects the wrong device and property names.

## Test harness

- `ttmModulator_test` subclass exposes the protected members with `using` declarations. `setupProperties()` creates the INDI properties the way `appStartup()` does (without registering them), and `setChannels()`, `setRested()` and `setSet()` put the function generator parameters in a known state. `useFastSetting()` uses 1 V set voltages with a 1 V step, so `setTTM()` has no ramp steps and does not sleep.
- `fakeFxnGen` is an `indiDriver` whose virtual `sendNewProperty()` records each request and answers it immediately by calling the matching ttmModulator set callback with the value the function generator would report (amplitudes of at least 0.002 V, offsets of at least 0.001 V). `installFxnGen()` installs it as `m_indiDriver` (MagAOXApp deletes it). It opens no FIFOs, and its output goes to `/dev/null`, so INDI set-property messages are discarded.
- `ttmModulator_test.cpp` does not include `testMacrosINDI.hpp`, so the callback bodies run. `ttmModulator_indi_test.cpp` includes it before the app header, so each callback returns right after the device/name check.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../ttmModulator.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/ttmModulator/tests/ttmModulator_test
../apps/ttmModulator/tests/ttmModulator_test
make -f Makefile.one t=../apps/ttmModulator/tests/ttmModulator_indi_test
../apps/ttmModulator/tests/ttmModulator_indi_test
```

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- `appStartup()` is not tested: it registers the INDI properties with the running driver.
- The function generator timeouts are not tested: `waitValue()` is called with its default 5 s timeout.
- Multi-step voltage ramps in `setTTM()` and `modTTM()` are not tested, since each step sleeps for 1 s. The frequency ramp is tested with one step, and the setting from midset includes the 1 s pause after resting, so those tests take about a second each.
- `calcState()` with a frequency above the last calibration frequency is not tested: it reads past the end of the calibration table.
