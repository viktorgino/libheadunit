TOP = $(realpath ..)

INCLUDES=$(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-codecparsers-1.0 libcrypto libusb-1.0 openssl glib-2.0 gtk+-3.0 x11 protobuf sdl2 libudev alsa) -I$(TOP)/hu -I$(TOP)/common -I$(TOP)/hu/generated.x64
CFLAGS= -g -rdynamic -pthread -Wno-unused-parameter
LFLAGS= -g -rdynamic -pthread -ldl $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-codecparsers-1.0 libcrypto libusb-1.0 openssl glib-2.0 gtk+-3.0 x11 protobuf sdl2 libudev alsa)
CXXFLAGS= $(CFLAGS) -std=c++11 -Wno-narrowing -Wno-unused-parameter


SRCS = $(TOP)/hu/hu_aap.cpp
SRCS += $(TOP)/hu/hu_aad.cpp
SRCS += $(TOP)/hu/hu_ssl.cpp
SRCS += $(TOP)/hu/hu_usb.cpp
SRCS += $(TOP)/hu/hu_uti.cpp
SRCS += $(TOP)/hu/hu_tcp.cpp
SRCS += $(TOP)/hu/generated.x64/hu.pb.cc
SRCS += $(TOP)/common/audio.cpp
SRCS += $(TOP)/common/glib_utils.cpp
SRCS += $(TOP)/common/command_server.cpp
SRCS += $(TOP)/common/web++/web++.cpp

SRCS += bt/ub_bluetooth.cpp
SRCS += main.cpp
SRCS += outputs.cpp
SRCS += callbacks.cpp

OBJS = $(addsuffix .x64.o, $(basename $(SRCS)))
DEPS = $(addsuffix .x64.d, $(basename $(SRCS)))
APP = headunit

.PHONY: clean

all: proto $(APP)

proto: $(TOP)/hu/generated.x64/hu.pb.cc

$(APP): $(OBJS)
	$(CXX) -o $(APP) $(OBJS) $(LFLAGS)

$(TOP)/hu/generated.x64/hu.pb.cc $(TOP)/hu/generated.x64/hu.pb.h: $(TOP)/hu/hu.proto
	protoc $< --proto_path=$(TOP)/hu/ --cpp_out=$(TOP)/hu/generated.x64/

%.x64.o : %.c
	$(CX) -MD $(CFLAGS) $(INCLUDES) -c $<  -o $@

%.x64.o : %.cpp
	$(CXX) -MD $(CXXFLAGS) $(INCLUDES) -c $<  -o $@

%.x64.o : %.cc
	$(CXX) -MD $(CXXFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f $(TOP)/hu/generated.x64/* $(OBJS) $(DEPS) *~ $(APP)

-include $(DEPS)

# DO NOT DELETE
