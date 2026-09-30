###############################################################################
#          makefile fragment for the unit tests of one MagAO-X app            #
#                                                                             #
# Usage, in apps/<app>/tests/Makefile:                                        #
#                                                                             #
#     TESTS = <app>_test [<other>_test ...]                                   #
#     include ../../../tests/magAOX_test.mk                                   #
#                                                                             #
# options (set before the include):                                           #
#   TESTS:    Test names, without extension, relative to the app tests        #
#             directory.  Each <name>.cpp is built into the <name> executable.#
#   TESTLIBS: Extra libraries needed by these tests, e.g. `TESTLIBS=-lgsl`,   #
#             which are += to LDLIBS by Makefile.one.                         #
#   TARGET:   Legacy single-test name, used when TESTS is not set.            #
#                                                                             #
# targets:                                                                    #
#   all:      Build every test in TESTS (the default).                        #
#   test:     Build and then run every test, stopping at the first failure.   #
#   run:      Same as test.                                                   #
#   rebuild:  Touch the test sources and build, so that edits to the app      #
#             headers are picked up.                                          #
#   clean:    Remove the test objects and executables.                        #
#                                                                             #
# notes:                                                                      #
#   -- each test is built by tests/Makefile.one, so the flags and libraries   #
#      match the full test suite driven by tests/tests.list                   #
#   -- vendor SDK stub headers placed in a `stubs/` subdirectory of the app   #
#      tests directory are put first on the include path by Makefile.one      #
#   -- libMagAOX must already be built (`make libs_all` at the top level)     #
#                                                                             #
###############################################################################

# The shared tests directory, which holds Makefile.one and testMain.cpp.
MAGAOX_TESTS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# The app tests directory, which holds the Makefile including this fragment.
APP_TESTS_DIR := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))

TESTS ?= $(TARGET)

# Absolute test paths, without extension, as expected by Makefile.one.
TEST_PATHS = $(addprefix $(APP_TESTS_DIR)/,$(basename $(TESTS)))

TEST_MAKE = $(MAKE) --no-print-directory -C $(MAGAOX_TESTS_DIR) -f Makefile.one TESTLIBS="$(TESTLIBS)"

.PHONY: all
all:
	@for test in $(TEST_PATHS); do \
	  $(TEST_MAKE) t=$$test || exit 1; \
	done

.PHONY: test
test: all
	@for test in $(TEST_PATHS); do \
	  echo "running $$test"; \
	  $$test || exit 1; \
	done

.PHONY: run
run: test

.PHONY: rebuild
rebuild:
	@for test in $(TEST_PATHS); do \
	  touch $$test.cpp; \
	done
	@$(MAKE) --no-print-directory -f $(firstword $(MAKEFILE_LIST)) all

.PHONY: clean
clean:
	@for test in $(TEST_PATHS); do \
	  $(TEST_MAKE) t=$$test clean; \
	done
