TOP = ..

M3TOOLCHAIN=./m3-toolchain
PROTOTOOLCHAIN=./protobuf-2.6.1-arm
CC=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-gcc
CXX=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-g++
LD=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-ld
SYSROOT=$(M3TOOLCHAIN)/arm-cortexa9_neon-linux-gnueabi/sysroot

INCLUDES=$(shell ./pkg-config-wrapper $(SYSROOT) --cflags gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 dbus-1 libusb-1.0  libcrypto openssl) -I$(TOP)/hu -I$(TOP)/hu/generated.arm -Imazda-connector -I$(PROTOTOOLCHAIN)/include
CFLAGS=-O3 --sysroot=$(SYSROOT) 
CXXFLAGS=$(CFLAGS) -std=c++11 -static-libstdc++

LFLAGS= --sysroot=$(SYSROOT) -static-libstdc++ $(shell ./pkg-config-wrapper $(SYSROOT) --libs gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 dbus-1 libusb-1.0 libcrypto openssl) -L$(PROTOTOOLCHAIN)/lib -lprotobuf
LFLAGS_IF= --sysroot=$(SYSROOT) -static-libstdc++ $(shell ./pkg-config-wrapper $(SYSROOT) --libs dbus-1)

#headunit binary
SRCS = $(TOP)/hu/hu_aad.cpp
SRCS += $(TOP)/hu/hu_aap.cpp
SRCS += $(TOP)/hu/hu_ssl.cpp
SRCS += $(TOP)/hu/hu_usb.cpp
SRCS += $(TOP)/hu/hu_uti.cpp
SRCS += $(TOP)/hu/hu_tcp.cpp
SRCS += $(TOP)/hu/generated.arm/hu.pb.cc
SRCS += nm/mzd_nightmode.cpp
SRCS += main.cpp

OBJS = $(addsuffix .arm.o, $(basename $(SRCS)))
DEPS = $(addsuffix .arm.d, $(basename $(SRCS)))

APP = headunit

SRCS_IF = mazda-connector/src/input_filter/input_filter.cpp

OBJS_IF = $(addsuffix .arm.o, $(basename $(SRCS_IF)))
DEPS_IF = $(addsuffix .arm.d, $(basename $(SRCS_IF)))

#input_filter binary
APP_IF = input_filter

all: proto $(APP) $(APP_IF)

proto: $(TOP)/hu/generated.arm/hu.pb.cc

$(APP): $(OBJS)
	$(CXX) -MD -g -o $(APP) $(OBJS) $(LFLAGS)

$(APP_IF): $(OBJS_IF)
	$(CXX) -MD -g -o $(APP_IF) $(OBJS_IF) $(LFLAGS_IF)

$(TOP)/hu/generated.arm/hu.pb.cc $(TOP)/hu/generated.arm/hu.pb.h: $(TOP)/hu/hu.proto
	$(PROTOTOOLCHAIN)/bin/protoc $< --proto_path=$(TOP)/hu/ --cpp_out=$(TOP)/hu/generated.arm/

%.arm.o : %.c
	$(CC) -MD -g $(CFLAGS) $(INCLUDES) -c $<  -o $@

%.arm.o : %.cpp
	$(CXX) -MD -g $(CXXFLAGS) $(INCLUDES) -c $<  -o $@

%.arm.o : %.cc
	$(CXX) -MD -g $(CXXFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f *~ $(TOP)/hu/generated.arm/* $(OBJS) $(OBJS_IF) $(APP) $(APP_IF) $(DEPS) $(DEPS_IF)

install: all
	cp -a -f input_filter installer/config/androidauto/data_persist/dev/bin/
	cp -a -f headunit installer/config/androidauto/data_persist/dev/bin/

-include $(DEPS)
-include $(DEPS_IF)

.PHONY: clean
