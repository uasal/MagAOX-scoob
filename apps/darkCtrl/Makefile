allall: all

# Shared dark_metadata.txt schema (also used by psfRefCtrl and iefcCtrl).
COMMON_INC := ../common/include
CXXFLAGS += -I$(COMMON_INC)
OTHER_HEADERS=$(COMMON_INC)/lina/dark_library.h
TARGET=darkCtrl
include ../../Make/magAOXApp.mk
