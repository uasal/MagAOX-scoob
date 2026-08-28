allall: all

# Shared dark_metadata.txt schema (also used by darkCtrl and psfRefCtrl).
COMMON_INC := ../common/include
CXXFLAGS += -I$(COMMON_INC)

# Vendored SHM-only IEFC core (no external iefc_camsci_once binary).
IEFC_DIR := iefc
CXXFLAGS += -I$(IEFC_DIR)/include
CXXFLAGS += -DLINA_USE_IMAGESTREAMIO -DLINA_USE_CFITSIO -DLINA_USE_EIGEN_SVD -DLINA_USE_OPENBLAS

OTHER_HEADERS = $(COMMON_INC)/lina/dark_library.h \
                $(IEFC_DIR)/include/lina/array.h \
                $(IEFC_DIR)/include/lina/stream.h \
                $(IEFC_DIR)/include/lina/coro_utils.h \
                $(IEFC_DIR)/include/lina/dm.h \
                $(IEFC_DIR)/include/lina/utils.h \
                $(IEFC_DIR)/include/lina/linalg.h \
                $(IEFC_DIR)/include/lina/iefc.h \
                $(IEFC_DIR)/include/lina/shmim_utils.h \
                $(IEFC_DIR)/include/lina/iefc_package.h

OTHER_OBJS = $(IEFC_DIR)/src/coro_utils.o \
             $(IEFC_DIR)/src/dm.o \
             $(IEFC_DIR)/src/utils.o \
             $(IEFC_DIR)/src/linalg.o \
             $(IEFC_DIR)/src/iefc.o \
             $(IEFC_DIR)/src/shmim_utils.o \
             $(IEFC_DIR)/src/fits_io.o \
             $(IEFC_DIR)/src/iefc_package.o

LDLIBS += -lcfitsio -lopenblas

TARGET=iefcCtrl
include ../../Make/magAOXApp.mk

# Object files live under iefc/src/; ensure they rebuild when headers change.
$(IEFC_DIR)/src/%.o: $(IEFC_DIR)/src/%.cpp $(OTHER_HEADERS)
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

.PHONY: iefc_clean
clean: iefc_clean
iefc_clean:
	rm -f $(OTHER_OBJS)
