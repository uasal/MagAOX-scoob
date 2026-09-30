For each C++ app in the MagAOX-scoob "app" directory, I would like a 'tests' directory to be created, and unit tests also written in C++ created which test the functionality of the C++ apps. On the claude/unit_tests branch for each 'app' add a commit which contains the 'tests' directory which contains the source code for running the test along with with a corresponding README and Makefile

Follow-up: Instead of building a full environment, write the unit tests and then once the code is finished and committed, the user will compile locally and provide any compile errors to continue.

Plan:

- Scope: every app under `apps/` with C++ sources.  Python-only apps (adcCtrl, aoSim, audibleAlerts, camtipSR,
  corAlign, dbIngest, efcControl, hamCtrl, magAOXMathsPy, pupilCorAlign, pythonIndiExample, sparkleTracker, visxCtrl)
  are out of scope.
- Shared infrastructure (one commit, ahead of the per-app commits):
  - `tests/magAOX_test.mk`: a makefile fragment included by each `apps/<app>/tests/Makefile`.  It takes
    `TESTS = <app>_test ...` (and optional `TESTLIBS`) and builds each test with `tests/Makefile.one`, so per-app builds
    use the same flags, `testMain.o`, and libraries as the full suite in `tests/tests.list`.  Targets: `all`, `test`
    (build + run), `rebuild` (touch sources, build), `clean`.  The name matches the fragment already referenced by
    `apps/fsmCtrl/tests/Makefile`, which did not exist.
  - `tests/Makefile.one`: if `apps/<app>/tests/stubs/` exists, put it first on the include path so vendor SDK stub
    headers shadow any installed SDK.  This keeps SDK-backed apps testable on machines without the vendor SDK and
    keeps the central tests.list build working with no per-test special cases.
  - `tests/groups.dox` and `AGENTS.md` (new rule 23) document the layout.
- Per-app commit (one per app) containing `apps/<app>/tests/` with:
  - Catch2 test source(s) following AGENTS.md rules 20/21 (`libXWCTest::<app>Test` namespace, `<app>_unit_test`
    defgroup under `application_unit_test`, a Doxygen brief per `TEST_CASE`, `#ifdef <APP>_TEST_DOXYGEN_REF` blocks,
    `\cond`-hidden harness classes).
  - `Makefile` including `../../../tests/magAOX_test.mk`.
  - `README.md` describing coverage, harness approach, and build/run commands.
  - `stubs/` with minimal vendor SDK headers when the app includes an SDK (Andor, ASI, Pylon, PICam, PVCAM, PIXELINK,
    QHY, Andor SDK3, libhsfw, BrainStem2, lgpio, CUDA/cuBLAS/TensorRT, ALPAO, BMC, IrisAO, libftdi, ...).  The stubbed
    functions are defined in the test source with controllable fake state (the ocam2KCtrl/mcp3208Ctrl precedent),
    so each test remains a single translation unit linked by `Makefile.one`.
  - The matching `tests/tests.list` entries.
- Test content priorities, adapted to what each app exposes without hardware:
  1. configuration defaults and overrides through `setupConfig()`/`loadConfig()` with `mx::app::writeConfigFile`;
  2. pure helpers (parsers, formatters, unit conversions, state mapping);
  3. INDI `newCallBack` validation (wrong device/name rejected, right one accepted) via `tests/testMacrosINDI.hpp`;
  4. state and error handling reachable through SDK stubs or fault-injecting subclasses.
- Existing placeholder tests (default construction only) are replaced with functional tests.  Existing substantive
  tests are kept, and gain the Makefile/README (and a tests.list entry if missing).
- Constraints recorded for test authors:
  - only one `MagAOXApp<>` instance may exist at a time (the constructor throws on a second instance);
  - `updateIfChanged()` and related updates are no-ops without an INDI driver, so assert on member state, not on
    published property values, unless the property is set directly;
  - avoid hardware, network, and fixed system paths; write temporary files under `/tmp` with test-unique names.
- Verification: no local build environment is used for this work (per the follow-up).  The user compiles locally and
  reports compile/test errors, which are fixed in follow-up commits on the same branch.

Execution notes:

- 2026-09-30: `apps/timeSeriesSimulator/tests/timeSeriesSimulator.cpp` is renamed to `timeSeriesSimulator_test.cpp`
  to match its existing `tests/tests.list` entry and the `**/tests/*_test` gitignore rule.
- 2026-09-30: `apps/dmCtrl/tests/template_test.cpp` was an uncompilable copy of the template skeleton; it is replaced
  by `dmCtrl_test.cpp`.
- 2026-09-30: pupilAlign has no tests. Its header is a partial copy of pupilFit that cannot compile (duplicate
  m_tgtShmim, many undeclared members and callbacks), so it needs rework before it can be tested.
- 2026-09-30: App-side fixes needed before some of the new tests build (tests assume them; apps not changed here):
  - `c_stdCamera_blacklevel` is required by dev::stdCamera but missing from andorCtrl, asiCtrl, cameraSim,
    cred2Ctrl, ocam2KCtrl, picamCtrl, pixelinkCtrl, pvcamCtrl, qhyCtrl and zylaCtrl.
  - qhyCtrl also lacks `c_stdCamera_synchro` and `setTempControl()`; zylaCtrl still uses the removed
    `m_startup_*` stdCamera members (now `m_default_*`).
  - dmRecon: missing `;` in the CPU modevals assignment and an unguarded `mx::cuda::cublasHandle` member.
  - dmCtrl.hpp does not compile (summerDevice types without `dev::`, undefined PacketCallbacks), so only
    dmCommands.hpp is tested.
  - photonCounter::loadImageIntoStream() casts to `uint16_t*` for a float map.
  - template is named with a C++ keyword, so its example test only compiles after renaming (not in tests.list).
  - aguc8Ctrl uses `sysPath` as a member (test works around it); po4ao uses unqualified `eigenImage` (test works
    around it).
  - fsmCtrl's existing tests reference moved/removed summerDevice types and need rewriting.
- 2026-09-30: `andorCtrl_test` was added to `EDT_TESTS` in tests/Makefile.one, since andorCtrl derives from
  dev::edtCamera.
