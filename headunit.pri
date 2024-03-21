CONFIG += plugin link_pkgconfig


PKGCONFIG += libssl libcrypto libusb-1.0 glib-2.0 gobject-2.0
PKGCONFIG += protobuf

SOURCES += \
    $$PWD/src/BluetoothServer.cpp \
    $$PWD/src/BluetoothService.cpp \
    $$PWD/src/hu_aap.cpp \
    $$PWD/src/hu_ssl.cpp \
    $$PWD/src/hu_uti.cpp \
    $$PWD/src/protocol/AndroidAuto.pb.cc \
    $$PWD/src/protocol/Bluetooth.pb.cc \
    $$PWD/src/transport/USBTransportStream.cpp \
    $$PWD/src/transport/TCPTransportStream.cpp

INCLUDEPATH += $${PWD}/includes
INCLUDEPATH += $${PWD}/includes/protocol
INCLUDEPATH += $${PWD}/includes/transport

HEADERS += \
    $$PWD/includes/AndroidAuto.h \
    $$PWD/includes/BluetoothServer.h \
    $$PWD/includes/BluetoothService.h \
    $$PWD/includes/HeadunitEventCallbacks.h \
    $$PWD/includes/defs.h \
    $$PWD/includes/hu_ssl.h \
    $$PWD/includes/hu_uti.h \
    $$PWD/includes/protocol/AndroidAuto.pb.h \
    $$PWD/includes/protocol/Bluetooth.pb.h \
    $$PWD/includes/transport/AbstractTransportStream.h \
    $$PWD/includes/transport/USBTransportStream.h \
    $$PWD/includes/transport/TCPTransportStream.h