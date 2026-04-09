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
# libftdi1: prefer pkg-config (e.g. Ubuntu: libftdi1-dev), else manual /usr/local install
ifeq ($(shell pkg-config --exists libftdi1 2>/dev/null && echo yes),yes)
INCLUDES+=$(shell pkg-config --cflags libftdi1)
LDLIBS+=$(shell pkg-config --libs libftdi1)
else
INCLUDES+=-I/usr/local/include/libftdi1/
LDLIBS+=-L/usr/local/lib64/ -lftdi1
endif

include ../../Make/magAOXApp.mk


