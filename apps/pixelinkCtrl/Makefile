
allall: all

OTHER_HEADERS=
OTHER_OBJS=
TARGET=pixelinkCtrl

PIXELINK_PATH=/opt/pixelink/include

PIXELINK_INCLUDES=-I$(PIXELINK_PATH)
PIXELINK_LIBS = -lPxLApi

INCLUDES += $(PIXELINK_INCLUDES)
LDFLAGS += -L/opt/pixelink/lib
LDLIBS += $(PIXELINK_LIBS)

include ../../Make/magAOXApp.mk
