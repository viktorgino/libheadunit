#include "headunitbluetoothserver.h"


Q_LOGGING_CATEGORY(HEADUNIT_BT_SERVER, "libheadunit::HeadunitBluetoothServer")

HeadunitBluetoothServer::HeadunitBluetoothServer(QObject *parent) : QObject (parent)
    , m_rfcommServer(QBluetoothServiceInfo::RfcommProtocol, this)
    , m_socket(nullptr)
{
    connect(&m_rfcommServer, &QBluetoothServer::newConnection, this, &HeadunitBluetoothServer::onClientConnected);
}

bool HeadunitBluetoothServer::start(const Config& config)
{
    m_config = config;
    qCInfo(HEADUNIT_BT_SERVER) << "listening.";
    return m_rfcommServer.listen(m_config.btAddress, m_config.btPort);
}

void HeadunitBluetoothServer::onClientConnected()
{
    m_socket = m_rfcommServer.nextPendingConnection();
    if(m_socket != nullptr)
    {
        qCInfo(HEADUNIT_BT_SERVER) << "Device Connected: " << m_socket->peerName();
        connect(m_socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
        writeSocketInfoRequest();
    }
    else
    {
        qCCritical(HEADUNIT_BT_SERVER) << "received null socket during client connection.";
    }
}

bool HeadunitBluetoothServer::writeProtoMessage(uint16_t messageType, google::protobuf::Message& message)
{
    QByteArray byteArray(message.SerializeAsString().c_str(), message.ByteSizeLong());
    uint16_t messageLength = message.ByteSizeLong();
    byteArray.prepend(messageType & 0x000000ff);
    byteArray.prepend((messageType & 0x0000ff00) >> 8);
    byteArray.prepend(messageLength & 0x000000ff);
    byteArray.prepend((messageLength & 0x0000ff00) >> 8);

    if(m_socket->write(byteArray) != byteArray.length())
    {
        return false;
    }

    return true;
}

void HeadunitBluetoothServer::writeSocketInfoRequest()
{
    qCInfo(HEADUNIT_BT_SERVER) << "Sending SocketInfoRequest.";

    QString ipAddr;
    foreach(QHostAddress addr, QNetworkInterface::allAddresses())
    {
        if(!addr.isLoopback() && (addr.protocol() == QAbstractSocket::IPv4Protocol))
        {
            ipAddr = addr.toString();
        }
    }

    HU::SocketInfoRequest socketInfoRequest;
    socketInfoRequest.set_ip_address(ipAddr.toStdString());
    qCInfo(HEADUNIT_BT_SERVER) << "ipAddress: " << ipAddr;

    socketInfoRequest.set_port(5000);
    qCInfo(HEADUNIT_BT_SERVER) << "port: "<< 5000;

    if(this->writeProtoMessage(1, socketInfoRequest))
    {
        qCInfo(HEADUNIT_BT_SERVER) << "Sent SocketInfoRequest.";
    }
    else
    {
        qCCritical(HEADUNIT_BT_SERVER) << "Error sending SocketInfoRequest.";
    }
}
void HeadunitBluetoothServer::writeSocketInfoResponse()
{
    qCInfo(HEADUNIT_BT_SERVER) << "Sending SocketInfoResponse.";
    QString ipAddr;
    foreach(QHostAddress addr, QNetworkInterface::allAddresses())
    {
        if(!addr.isLoopback() && (addr.protocol() == QAbstractSocket::IPv4Protocol))
        {
            ipAddr = addr.toString();
        }
    }

    HU::SocketInfoResponse socketInfoResponse;
    socketInfoResponse.set_ip_address(ipAddr.toStdString());
    qCInfo(HEADUNIT_BT_SERVER) << "ipAddress: "<< ipAddr;

    socketInfoResponse.set_port(5000);
    qCInfo(HEADUNIT_BT_SERVER) << "port: "<< 5000;

    socketInfoResponse.set_status(HU::Status::STATUS_SUCCESS);
    qCInfo(HEADUNIT_BT_SERVER) << "status: "<< HU::Status::STATUS_SUCCESS;


    if(this->writeProtoMessage(7, socketInfoResponse))
    {
        qCInfo(HEADUNIT_BT_SERVER) << "Sent SocketInfoResponse.";
    }
    else
    {
        qCCritical(HEADUNIT_BT_SERVER) << "Error sending SocketInfoResponse.";
    }
}

void HeadunitBluetoothServer::handleSocketInfoRequestResponse(QByteArray data)
{
    HU::SocketInfoResponse socketInfoResponse;
    socketInfoResponse.ParseFromArray(data, data.size());
    qCInfo(HEADUNIT_BT_SERVER) <<"Received SocketInfoRequestResponse, status: "<<socketInfoResponse.status();
    if(socketInfoResponse.status() == 0)
    {
        // A status of 0 should be successful handshake (unless phone later reports an error, aw well)
        // save this phone so we can autoconnect to it next time
//        config_->setLastBluetoothPair(m_socket->peerAddress().toString().toStdString());
//        config_->save();
    }
}


void HeadunitBluetoothServer::handleSocketInfoRequest(QByteArray data)
{
    qCInfo(HEADUNIT_BT_SERVER) << "Reading SocketInfoRequest.";

    HU::SocketInfoRequest socketInfoRequest;

    writeSocketInfoResponse();
}

void HeadunitBluetoothServer::writeNetworkInfoMessage()
{
    qCInfo(HEADUNIT_BT_SERVER) << "Sending NetworkInfoMessage.";

    HU::NetworkInfo networkMessage;
    networkMessage.set_ssid(m_config.wlanSSID.toStdString());
    qCInfo(HEADUNIT_BT_SERVER) << "SSID: " << m_config.wlanSSID;

    networkMessage.set_psk(m_config.wlanPSK.toStdString());
    qCInfo(HEADUNIT_BT_SERVER) << "PSKEY: " << m_config.wlanPSK;

    networkMessage.set_mac_addr(m_config.wlanMacAddress.toStdString());
    qCInfo(HEADUNIT_BT_SERVER) << "MAC: "<< m_config.wlanMacAddress;

    networkMessage.set_security_mode(HU::SecurityMode::WPA2_PERSONAL);
    qCInfo(HEADUNIT_BT_SERVER) << "Security: "<< HU::SecurityMode::WPA2_PERSONAL;

    networkMessage.set_ap_type(HU::AccessPointType::STATIC);
    qCInfo(HEADUNIT_BT_SERVER) << "AP Type: "<< HU::AccessPointType::STATIC;


    if(this->writeProtoMessage(3, networkMessage))
    {
        qCInfo(HEADUNIT_BT_SERVER) << "Sent NetworkInfoMessage";
    }
    else
    {
        qCInfo(HEADUNIT_BT_SERVER) << "Error sending NetworkInfoMessage.";
    }
}

void HeadunitBluetoothServer::handleUnknownMessage(int messageType, QByteArray data)
{
        qCInfo(HEADUNIT_BT_SERVER) << "Received unknown MessageType of "<<messageType;
        qCInfo(HEADUNIT_BT_SERVER) << "Unknown Message Data: " << data.toHex(' ');
}

void HeadunitBluetoothServer::readSocket()
{
    if(!m_socket)
    {
        return;
    }

    auto data = m_socket->readAll();
    if(data.length() == 0)
    {
        return;
    }

    uint16_t messageType = (data[2] << 8) | data[3];
    switch(messageType)
    {
        case 1:
            handleSocketInfoRequest(data);
            break;
        case 2:
            writeNetworkInfoMessage();
            break;
        case 7:
            data.remove(0, 4);
            handleSocketInfoRequestResponse(data);
            break;
        default:
            data.remove(0, 4);
            handleUnknownMessage(messageType, data);
            break;
    }
}

