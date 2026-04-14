# kim101Ctrl Makefile
#
# Build requirements: libftdi1
#
# To build libftdi1 from scratch:
#
# sudo yum install libconfuse-devel
# wget https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.5.tar.bz2 .
# tar -xvjf libftdi1-1.5.tar.bz2
# cd libftdi1-1.5
# mkdir _build
# cd _build
# cmake ..
# make
# sudo make install
# echo /usr/local/lib64/ | sudo tee -a /etc/ld.so.conf.d/lib64.conf
# sudo ldconfig


allall: all 

OTHER_HEADERS=

TARGET=kim101Ctrl

# tmcController.hpp lives in kcubeCtrl (this repo)
INCLUDES+=-I../kcubeCtrl/
# libftdi: prefer pkg-config (libftdi1 or libftdi), else distro defaults
FTDI_CFLAGS:=$(shell pkg-config --cflags libftdi1 2>/dev/null || pkg-config --cflags libftdi 2>/dev/null)
FTDI_LIBS:=$(shell pkg-config --libs libftdi1 2>/dev/null || pkg-config --libs libftdi 2>/dev/null)

ifneq ($(strip $(FTDI_LIBS)),)
INCLUDES+=$(FTDI_CFLAGS)
LDLIBS+=$(FTDI_LIBS)
else
INCLUDES+=-I/usr/include/libftdi1 -I/usr/include/libusb-1.0
LDLIBS+=-L/usr/lib/x86_64-linux-gnu -lftdi1 -lusb-1.0
endif

include ../../Make/magAOXApp.mk


