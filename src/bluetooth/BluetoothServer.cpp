#include "bluetooth/BluetoothServer.h"
#include <QDataStream>
#include <QLoggingCategory>

#include "protocol/Bluetooth.pb.h"

Q_LOGGING_CATEGORY(HEADUNIT_BT_SERVER, "libheadunit::BluetoothServer")

BluetoothServer::BluetoothServer(QObject* parent)
    : QObject(parent)
    , m_bluetoothService(this)
    , rfcommServer_(QBluetoothServiceInfo::RfcommProtocol, this)
{
    connect(&rfcommServer_, &QBluetoothServer::newConnection, this, &BluetoothServer::onClientConnected);
}

BluetoothServer::~BluetoothServer(){

    m_bluetoothService.unregisterService();
    qDebug() << "Service unregistered";
}

void BluetoothServer::start(const Config& config)
{
    m_config = config;

    QBluetoothLocalDevice localDevice(m_config.btAddress, this);

    if (rfcommServer_.listen(m_config.btAddress)) {
        int port = rfcommServer_.serverPort();
        qCInfo(HEADUNIT_BT_SERVER) << "Listening on " << m_config.btAddress.toString() << "port" << port;
            
        if (!m_bluetoothService.registerService(m_config.btAddress, port)) {
            qWarning() << "[btservice] Service registration failed.";
        }
    }
}

void BluetoothServer::onClientConnected()
{
    if (socket != nullptr) {
        socket->deleteLater();
    }

    socket = rfcommServer_.nextPendingConnection();

    if (socket != nullptr) {
        qCInfo(HEADUNIT_BT_SERVER) << "New device connected: " << socket->peerName();

        connect(socket, &QBluetoothSocket::readyRead, this, &BluetoothServer::readSocket);

        HU::WifiInfoRequest request;
        request.set_ip_address(m_config.wlanIP.toStdString());
        request.set_port(5000);

        sendMessage(request, 1);
    } else {
        qCWarning(HEADUNIT_BT_SERVER) << "Received null socket during client connection.";
    }
}

void BluetoothServer::readSocket()
{
    buffer += socket->readAll();

    if (buffer.length() < 4) {
        qCWarning(HEADUNIT_BT_SERVER)  << "Not enough data, waiting for more";
        return;
    }

    QDataStream stream(buffer);
    uint16_t length;
    stream >> length;

    if (buffer.length() < length + 4) {
        qCWarning(HEADUNIT_BT_SERVER)  << "Not enough data, waiting for more: " << buffer.length();
        return;
    }

    uint16_t messageId;
    stream >> messageId;

    switch (messageId) {
    case 1:
        qCInfo(HEADUNIT_BT_SERVER) << "WiFi Info Request";
        handleWifiInfoRequest(buffer, length);
        break;
    case 2:
        handleWifiSecurityRequest(buffer, length);
        break;
    case 7:
        handleWifiInfoRequestResponse(buffer, length);
        break;
    case 6:
        qCInfo(HEADUNIT_BT_SERVER) << "Connection successful";
        emit deviceConnected();
        break;
    default: {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (auto&& val : buffer) {
            ss << std::setw(2) << static_cast<unsigned>(val);
        }
        qCWarning(HEADUNIT_BT_SERVER)  << "Unknown message: (" << messageId << ") " << QString::fromStdString(ss.str());
        break;
    }
    }

    buffer = buffer.mid(length + 4);
}

void BluetoothServer::handleWifiInfoRequest(QByteArray& buffer, uint16_t length)
{
    HU::WifiInfoRequest msg;
    msg.ParseFromArray(buffer.data() + 4, length);

    HU::WifiInfoResponse response;
    response.set_ip_address(m_config.wlanIP.toStdString());
    response.set_port(5000);
    response.set_status(HU::WifiInfoResponse_Status_STATUS_SUCCESS);

    sendMessage(response, 7);
}

void BluetoothServer::handleWifiSecurityRequest(QByteArray& buffer, uint16_t length)
{
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

    auto written = socket->write(out);
    if (written < 0) {
        qCWarning(HEADUNIT_BT_SERVER) << "Could not write data";
    }
}

void BluetoothServer::handleWifiInfoRequestResponse(QByteArray& buffer, uint16_t length)
{
    HU::WifiInfoResponse msg;
    msg.ParseFromArray(buffer.data() + 4, length);
}
