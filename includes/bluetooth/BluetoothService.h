#pragma once

#include <QObject>
#include <QBluetoothServer>

class BluetoothService : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothService(QObject *parent = nullptr);

    bool registerService(const QBluetoothAddress& bluetoothAddress, uint16_t portNumber);
    bool unregisterService();
    QBluetoothServiceInfo getServiceInfo();

private:
    QBluetoothServiceInfo serviceInfo_;
};
