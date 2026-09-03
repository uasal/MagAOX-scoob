allall: all

# Same interpreter as MagAO-X Python apps: base conda (/opt/conda), not a named env.
# Instrument Make/python.mk is already PYTHON ?= /opt/conda/bin/python.
PYTHON ?= /opt/conda/bin/python
include ../../Make/python.mk

PYINC := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_path('include'))")
NPYINC := $(shell $(PYTHON) -c "import numpy; print(numpy.get_include())")
PYLIBDIR := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
PYLDVER := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('LDVERSION'))")
PYPREFIX := $(shell $(PYTHON) -c "import sys; print(sys.prefix)")

CXXFLAGS += -I$(PYINC) -I$(NPYINC)
CXXFLAGS += -DPYTHON_PREFIX=\"$(PYPREFIX)\"
CXXFLAGS += -DLLOWFSCSIM_BRIDGE_DIR=\"$(abspath .)/\"
LDLIBS += -L$(PYLIBDIR) -lpython$(PYLDVER) -Wl,-rpath,$(PYLIBDIR)

OTHER_HEADERS = optical_model_embed.hpp
TARGET = llowfscSimCpp
include ../../Make/magAOXApp.mk

.PHONY: install-bridge
install: install-bridge
install-bridge:
	sudo install -d /opt/MagAOX/python/llowfscSimCpp
	sudo install -m 644 optical_bridge.py /opt/MagAOX/python/llowfscSimCpp/optical_bridge.py
