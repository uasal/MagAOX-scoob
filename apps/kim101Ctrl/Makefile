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
# Force system FTDI/libusb (avoid /usr/local pkg-config ambiguity on this host)
INCLUDES+=-I/usr/include/libftdi1 -I/usr/include/libusb-1.0
FTDI_SO:=$(firstword $(wildcard /usr/lib/x86_64-linux-gnu/libftdi1.so /usr/lib/x86_64-linux-gnu/libftdi1.so.*))
LIBUSB_SO:=$(firstword $(wildcard /usr/lib/x86_64-linux-gnu/libusb-1.0.so /usr/lib/x86_64-linux-gnu/libusb-1.0.so.*))
LDLIBS+=$(FTDI_SO) $(LIBUSB_SO)

include ../../Make/magAOXApp.mk


