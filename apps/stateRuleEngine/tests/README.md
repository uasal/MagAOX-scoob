# stateRuleEngine unit tests

Catch2 unit tests for the `stateRuleEngine` MagAO-X application (`apps/stateRuleEngine/stateRuleEngine.hpp`), including its rule classes (`indiCompRules.hpp`) and rule configuration loader (`indiCompRuleConfig.hpp`).

## Test files

| File | Covers |
|------|--------|
| `stateRuleEngine_test.cpp` | App-level notification formatting, published rule-state helpers, and `appLogic()` clear notifications |
| `indiCompRules_test.cpp` | Evaluation of every `indiCompRule` type against INDI properties |
| `indiCompRuleConfig_test.cpp` | Loading rules from configuration files, including the demo config and error handling |

## What is tested

- **Rule evaluation** (`indiCompRules_test.cpp`): string, number (with tolerance), switch and time-difference element-value rules for each comparison operator; element-vs-element comparisons within and across properties; `multiSwitchCombo` rules (matches, mismatches, zero/multiple active switches, diagnostics drained by compound rules); rule-vs-rule compound logic such as `(A && B) || C`.
- **Rule configuration** (`indiCompRuleConfig_test.cpp`): each rule type with default and non-default settings, the shipped demo configuration, and error cases (no rule sections, invalid rule type, `ruleComp` rules with missing or self-referencing operands, invalid `multiSwitchCombo` settings: zero switches, missing property keys, conflicting source types, bad operator, placeholder count mismatch).
- **App logic** (`stateRuleEngine_test.cpp`): construction, active/clear notification text (priority label, configured message, fallback to rule name), selection of the published state property and On-state detection, and `appLogic()` issuing exactly one clear notification per On-to-Off transition.

## Test harness

- `stateRuleEngine_test` subclass adds rules directly, provisions and forces their published switch elements, and overrides the notification send hook to capture messages instead of sending them through an INDI driver.
- `fixedRule` is an `indiCompRule` with a settable boolean value, used to drive `appLogic()` deterministically.
- The rule and rule-config tests use the rule classes directly with hand-built `pcf::IndiProperty` objects and config files written with `mx::app::writeConfigFile`.

## Building and running

The tests need libMagAOX and its dependencies built first (`make libs_all` at the top of the repository).

From this directory:

```
make          # build the tests
make test     # build and run the tests
make rebuild  # rebuild after editing ../stateRuleEngine.hpp
make clean    # remove the test objects and executables
```

Or with the shared test build, from the top-level `tests/` directory:

```
make -f Makefile.one t=../apps/stateRuleEngine/tests/stateRuleEngine_test
../apps/stateRuleEngine/tests/stateRuleEngine_test
```

Substitute any of the other test names (`indiCompRules_test`, `indiCompRuleConfig_test`) for `stateRuleEngine_test`.

The tests are listed in `tests/tests.list`, so they also run with the full suite (`tests/testMagAOX.bash`).

## Limitations

- INDI callbacks that feed live properties into the rules are not driven through a real INDI server.
- `stateRuleEngine_test.cpp` still contains a construction-only "placeholder harness" `TEST_CASE`.
