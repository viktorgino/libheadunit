INCLUDES=$(shell pkg-config --cflags gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 gstreamer-interfaces-0.10 libcrypto libusb-1.0 openssl gtk+-3.0 x11) -I$(TOP)/hu 
CFLAGS= -g 
LFLAGS= -g $(shell pkg-config --libs gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 gstreamer-interfaces-0.10 libcrypto libusb-1.0 openssl gtk+-3.0 x11) -lSDL
CXXFLAGS= -g -std=c++11

TOP = ..
SRCS = $(TOP)/hu/hu_aad.cpp
SRCS += $(TOP)/hu/hu_aap.cpp
SRCS += $(TOP)/hu/hu_ssl.cpp
SRCS += $(TOP)/hu/hu_usb.cpp
SRCS += $(TOP)/hu/hu_uti.cpp
SRCS += $(TOP)/hu/hu_tcp.cpp
SRCS += main.cpp

OBJS = $(addsuffix .x64.o, $(basename $(SRCS)))
DEPS = $(addsuffix .x64.d, $(basename $(SRCS)))
APP = headunit

.PHONY: clean

all: $(APP)

$(APP): $(OBJS)
	$(CXX) -o $(APP) $(OBJS) $(LFLAGS)

%.x64.o : %.cpp
	$(CXX) -MD $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f $(OBJS) $(DEPS) *~ $(APP)

-include $(DEPS)

# DO NOT DELETE
