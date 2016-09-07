CC=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-gcc
LD=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-ld
SYSROOT=$(M3TOOLCHAIN)/arm-cortexa9_neon-linux-gnueabi/sysroot

INCLUDES=$(shell ./pkg-config-wrapper --cflags gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 dbus-1 libusb-1.0  libcrypto openssl) -I$(TOP)/jni
CFLAGS=-fPIC -O3 --sysroot=$(SYSROOT) 
CPPFLAGS=-fPIC 
LFLAGS= --sysroot=$(SYSROOT) $(shell ./pkg-config-wrapper --libs gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 dbus-1 libusb-1.0 libcrypto openssl)
TOP = ..
SRCS =	$(TOP)/jni/hu_aad.c	\
	$(TOP)/jni/hu_aap.c	\
        $(TOP)/jni/hu_ssl.c	\
	$(TOP)/jni/hu_usb.c	\
	$(TOP)/jni/hu_uti.c	\
	main.c
OBJS = $(SRCS:.c=.arm.o)
APP = headunit

.PHONY: depend clean

all: $(APP)

$(APP): $(OBJS)
	$(CC)  -g -o $(APP) $(OBJS) $(LFLAGS)

%.arm.o : %.c
	$(CC) -g $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f $(OBJS) *~ $(APP)

depend: $(SRCS)
	makedepend $^

# DO NOT DELETE
