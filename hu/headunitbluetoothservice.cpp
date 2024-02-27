#include "headunitbluetoothservice.h"

HeadunitBluetoothService::HeadunitBluetoothService(QObject *parent)
    : QObject{parent}
{
}

bool HeadunitBluetoothService::registerService(const QBluetoothAddress& bluetoothAddress, uint16_t portNumber)
{
    //"4de17a00-52cb-11e6-bdf4-0800200c9a66";
    //"669a0c20-0008-f4bd-e611-cb52007ae14d";
    const QBluetoothUuid serviceUuid(QLatin1String("4de17a00-52cb-11e6-bdf4-0800200c9a66"));

    QBluetoothServiceInfo::Sequence classId;
    classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList, classId);
    classId.prepend(QVariant::fromValue(serviceUuid));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceName, "OpenAuto Bluetooth Service");
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceDescription, "AndroidAuto WiFi projection automatic setup");
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceProvider, "f1xstudio.com");
    serviceInfo_.setServiceUuid(serviceUuid);

    QBluetoothServiceInfo::Sequence publicBrowse;
    publicBrowse << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::BrowseGroupList, publicBrowse);

    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
             << QVariant::fromValue(quint16(portNumber));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocolDescriptorList);
    return serviceInfo_.registerService(bluetoothAddress);
}

bool HeadunitBluetoothService::unregisterService()
{
    return serviceInfo_.unregisterService();
}
