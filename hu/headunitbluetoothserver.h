#ifndef HEADUNITBLUETOOTHSERVER_H
#define HEADUNITBLUETOOTHSERVER_H

#include <QObject>
#include <QBluetoothServer>
#include <QNetworkInterface>
#include <QThread>
#include <QLoggingCategory>
#include "bt.pb.h"


class HeadunitBluetoothServer : public QObject
{
    Q_OBJECT
public:
    explicit HeadunitBluetoothServer(QObject *parent = nullptr);

    typedef struct Config {
        QBluetoothAddress btAddress;
        uint16_t btPort = 0;
        QString wlanMacAddress = "";
        QString wlanSSID = "";
        QString wlanPSK = "";
    } Config;

    bool start(const Config& config);

private slots:
    void onClientConnected();
    void readSocket();

private:
    QBluetoothServer m_rfcommServer;
    QBluetoothSocket* m_socket;

    Config m_config;

    void handleUnknownMessage(int messageType, QByteArray data);
    void handleSocketInfoRequest(QByteArray data);
    void handleSocketInfoRequestResponse(QByteArray data);
    void writeSocketInfoRequest();
    void writeSocketInfoResponse();
    void writeNetworkInfoMessage();
    bool writeProtoMessage(uint16_t messageType, google::protobuf::Message& message);
};

#endif // HEADUNITBLUETOOTHSERVER_H
