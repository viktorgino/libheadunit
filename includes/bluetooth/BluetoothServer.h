#pragma once

#include <QBluetoothServer>
#include <QBluetoothLocalDevice>
#include <QBluetoothSocket>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QObject>
#include <QThread>

#include "bluetooth/BluetoothService.h"
#include "protocol/Bluetooth.pb.h"

class BluetoothServer : public QObject
{
    Q_OBJECT
public:
    typedef struct Config {
        QBluetoothAddress btAddress;
        QString wlanMacAddress = "";
        QString wlanSSID = "";
        QString wlanPSK = "";
        QString wlanIP = "";
    } Config;

    explicit BluetoothServer(QObject* parent = nullptr);
    ~BluetoothServer();

    void start(const Config& config);

signals:
    void deviceConnected();

private slots:
    void onClientConnected();

private:
    BluetoothService m_bluetoothService;
    QBluetoothServer rfcommServer_;
    QBluetoothSocket* socket = nullptr;
    Config m_config;

    void readSocket();

    QByteArray buffer;

    void handleWifiInfoRequest(QByteArray &buffer, uint16_t length);

    void sendMessage(const google::protobuf::Message &message, uint16_t type);

    void handleWifiSecurityRequest(QByteArray &buffer, uint16_t length);

    void handleWifiInfoRequestResponse(QByteArray &buffer, uint16_t length);

    const ::std::string getIP4_(const QString intf);
};
