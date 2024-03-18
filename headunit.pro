TEMPLATE = lib
CONFIG += plugin link_pkgconfig
TARGET = $$qtLibraryTarget(libheadunit)
DEFINES += QT_DEPRECATED_WARNINGS

PKGCONFIG += libssl libcrypto libusb-1.0 glib-2.0 gobject-2.0
PKGCONFIG += protobuf

SOURCES += \
    headunit/hu/bluetoothserver.cpp \
    headunit/hu/bluetoothservice.cpp \
    headunit/hu/hu_aad.cpp \
    headunit/hu/hu_aap.cpp \
    headunit/hu/hu_ssl.cpp \
    headunit/hu/hu_tcp.cpp \
    headunit/hu/hu_usb.cpp \
    headunit/hu/hu_uti.cpp \
    headunit/hu/generated.x64/hu.pb.cc \
    headunit/hu/generated.x64/bt.pb.cc 
    

INCLUDEPATH +=$$PWD/hu
INCLUDEPATH +=$$PWD/hu/generated.x64


HEADERS += \
    headunit/hu/bluetoothserver.h \
    headunit/hu/bluetoothservice.h \
    headunit/hu/hu_aad.h \
    headunit/hu/hu_aap.h \
    headunit/hu/hu_ssl.h \
    headunit/hu/hu_tcp.h \
    headunit/hu/hu_usb.h \
    headunit/hu/hu_uti.h \
    headunit/hu/generated.x64/hu.pb.h \
    headunit/hu/generated.x64/bt.pb.h