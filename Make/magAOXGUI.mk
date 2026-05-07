####################################################
# Makefile for building MagAOX Qt GUIs
#
# The only thing that needs to be customized here is TARGET. The
# remaing app-specific details are handled by the Qt .pro file.
#
# Usage: In the folder of a GUI app, create a Makefile with a minimum of:
#       TARGET=<name>
#       include ../../../Make/magAOXGUI.mk
#
####################################################



# Verify qmake is available:

QMAKE ?= qmake
QMAKE_PATH := $(shell which qmake 2>/dev/null)

ifeq "$(QMAKE_PATH)" ""
  $(error No qmake on PATH)
endif


##############################


all: $(TARGET)

.PHONY: $(TARGET)
$(TARGET):
	$(QMAKE) -makefile $(TARGET).pro
	$(MAKE) -f makefile.$(TARGET)

install: $(TARGET)
	sudo install bin/$(TARGET) /usr/local/bin

clean:
ifneq (,$(wildcard ./makefile.$(TARGET)))  #Test if the generated makefile exists to avoid errors on 2nd make clean
	$(MAKE) -f makefile.$(TARGET) distclean
endif
	rm -f *~
	rm -f bin/$(TARGET)
	rm -rf bin moc obj res
