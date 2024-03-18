#include "bluetoothserver.h"
#include <QDataStream>
#include <QLoggingCategory>

#include "bt.pb.h"

Q_LOGGING_CATEGORY(HEADUNIT_BT_SERVER, "libheadunit::BluetoothServer")

BluetoothServer::BluetoothServer(QObject* parent)
    : QObject(parent)
    , rfcommServer_(QBluetoothServiceInfo::RfcommProtocol, this)
{
    connect(&rfcommServer_, &QBluetoothServer::newConnection, this, &BluetoothServer::onClientConnected);
}

int BluetoothServer::start(const Config& config)
{
    m_config = config;
    if (rfcommServer_.listen(m_config.btAddress)) {
        return rfcommServer_.serverPort();
    }
    return 0;
}

void BluetoothServer::onClientConnected()
{
    if (socket != nullptr) {
        socket->deleteLater();
    }

    socket = rfcommServer_.nextPendingConnection();

    if (socket != nullptr) {
        qCInfo(HEADUNIT_BT_SERVER) << "[AndroidBluetoothServer] rfcomm client connected, peer name: " << socket->peerName();

        connect(socket, &QBluetoothSocket::readyRead, this, &BluetoothServer::readSocket);
        //                    connect(socket, &QBluetoothSocket::disconnected, this,
        //                            QOverload<>::of(&ChatServer::clientDisconnected));

        HU::WifiInfoRequest request;
        request.set_ip_address(m_config.wlanIP.toStdString());
        request.set_port(5000);

        sendMessage(request, 1);
    } else {
        qCWarning(HEADUNIT_BT_SERVER) << "[AndroidBluetoothServer] received null socket during client connection.";
    }
}

void BluetoothServer::readSocket()
{
    buffer += socket->readAll();

    qCInfo(HEADUNIT_BT_SERVER)  << "Received message";

    if (buffer.length() < 4) {
        qCDebug(HEADUNIT_BT_SERVER)  << "Not enough data, waiting for more";
        return;
    }

    QDataStream stream(buffer);
    uint16_t length;
    stream >> length;

    if (buffer.length() < length + 4) {
        qCInfo(HEADUNIT_BT_SERVER)  << "Not enough data, waiting for more: " << buffer.length();
        return;
    }

    uint16_t messageId;
    stream >> messageId;

    // qCInfo(HEADUNIT_BT_SERVER)  << "[AndroidBluetoothServer] " << length << " " << messageId;

    switch (messageId) {
    case 1:
        handleWifiInfoRequest(buffer, length);
        break;
    case 2:
        handleWifiSecurityRequest(buffer, length);
        break;
    case 7:
        handleWifiInfoRequestResponse(buffer, length);
        break;
    case 6:
        emit deviceConnected();
        break;
    default: {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (auto&& val : buffer) {
            ss << std::setw(2) << static_cast<unsigned>(val);
        }
        qCInfo(HEADUNIT_BT_SERVER)  << "Unknown message: " << messageId;
        qCInfo(HEADUNIT_BT_SERVER) << QString::fromStdString(ss.str());
        break;
    }
    }

    buffer = buffer.mid(length + 4);
}

void BluetoothServer::handleWifiInfoRequest(QByteArray& buffer, uint16_t length)
{
    HU::WifiInfoRequest msg;
    msg.ParseFromArray(buffer.data() + 4, length);
    qCInfo(HEADUNIT_BT_SERVER) << "WifiInfoRequest: " << QString::fromStdString(msg.DebugString());

    HU::WifiInfoResponse response;
    response.set_ip_address(m_config.wlanIP.toStdString());
    response.set_port(5000);
    response.set_status(HU::WifiInfoResponse_Status_STATUS_SUCCESS);

    sendMessage(response, 7);
}

void BluetoothServer::handleWifiSecurityRequest(QByteArray& buffer, uint16_t length)
{
    qCInfo(HEADUNIT_BT_SERVER) << "handleWifiSecurityRequest: " << buffer.toHex();
    HU::WifiSecurityReponse response;

    response.set_ssid(m_config.wlanSSID.toStdString());
    response.set_bssid(m_config.wlanMacAddress.toStdString());
    response.set_key(m_config.wlanPSK.toStdString());
    response.set_security_mode(HU::WifiSecurityReponse_SecurityMode_WPA2_PERSONAL);
    response.set_access_point_type(HU::WifiSecurityReponse_AccessPointType_STATIC);

    sendMessage(response, 3);
}

void BluetoothServer::sendMessage(const google::protobuf::Message& message, uint16_t type)
{
    int byteSize = message.ByteSizeLong();
    QByteArray out(byteSize + 4, 0);
    QDataStream ds(&out, QIODevice::ReadWrite);
    ds << (uint16_t)byteSize;
    ds << type;
    message.SerializeToArray(out.data() + 4, byteSize);

    qCDebug(HEADUNIT_BT_SERVER) << QString::fromStdString(message.GetTypeName()) << " - " + QString::fromStdString(message.DebugString());

    auto written = socket->write(out);
    if (written > -1) {
        qCInfo(HEADUNIT_BT_SERVER) << "Bytes written: " << written;
    } else {
        qCInfo(HEADUNIT_BT_SERVER) << "Could not write data";
    }
}

void BluetoothServer::handleWifiInfoRequestResponse(QByteArray& buffer, uint16_t length)
{
    HU::WifiInfoResponse msg;
    msg.ParseFromArray(buffer.data() + 4, length);
    qCInfo(HEADUNIT_BT_SERVER) << "WifiInfoResponse: " << QString::fromStdString(msg.DebugString());
}
