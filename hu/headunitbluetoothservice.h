#ifndef HEADUNITBLUETOOTHSERVICE_H
#define HEADUNITBLUETOOTHSERVICE_H

#include <QObject>
#include <QBluetoothServer>

class HeadunitBluetoothService : public QObject
{
    Q_OBJECT
public:
    explicit HeadunitBluetoothService(QObject *parent = nullptr);

    bool registerService(const QBluetoothAddress& bluetoothAddress, uint16_t portNumber);
    bool unregisterService();

private:
    QBluetoothServiceInfo serviceInfo_;
};

#endif // HEADUNITBLUETOOTHSERVICE_H
