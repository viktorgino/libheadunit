#pragma once

#include <functional>
#include <map>
#include <string>
#include <thread>

#include "hu_ssl.h"
#include "hu_uti.h"
#include "defs.h"
#include "HeadunitEventCallbacks.h"
#include "protocol/AndroidAuto.pb.h"
#include "transport/AbstractTransportStream.h"

namespace AndroidAuto {
#define MAX_FRAME_PAYLOAD_SIZE 0x4000
// At 16 bytes for header
#define MAX_FRAME_SIZE 0x4100


class IHUConnectionThreadInterface;

class IHUAnyThreadInterface {
protected:
    ~IHUAnyThreadInterface() {
    }
    IHUAnyThreadInterface() {
    }

public:
    typedef std::function<void(IHUConnectionThreadInterface &)> HUThreadCommand;
    // Can be called from any thread
    virtual int queueCommand(HUThreadCommand &&command) = 0;
};

class IHUConnectionThreadInterface : public IHUAnyThreadInterface {
protected:
    ~IHUConnectionThreadInterface() {
    }
    IHUConnectionThreadInterface() {
    }

public:
    virtual int sendEncodedMessage(int retry, ServiceChannels chan, uint16_t messageCode, const google::protobuf::MessageLite &message,
                                   int overrideTimeout = -1) = 0;
    virtual int sendEncodedMediaPacket(int retry, ServiceChannels chan, uint16_t messageCode, uint64_t timeStamp, const byte *buffer, int bufferLen,
                                       int overrideTimeout = -1) = 0;
    virtual int sendUnencodedBlob(int retry, ServiceChannels chan, uint16_t messageCode, const byte *buffer, int bufferLen,
                                  int overrideTimeout = -1) = 0;
    virtual int sendUnencodedMessage(int retry, ServiceChannels chan, uint16_t messageCode, const google::protobuf::MessageLite &message,
                                     int overrideTimeout = -1) = 0;

    template <typename EnumType>
    inline int sendEncodedMessage(int retry, ServiceChannels chan, EnumType messageCode, const google::protobuf::MessageLite &message,
                                  int overrideTimeout = -1) {
        return sendEncodedMessage(retry, chan, static_cast<uint16_t>(messageCode), message, overrideTimeout);
    }

    template <typename EnumType>
    inline int sendEncodedMediaPacket(int retry, ServiceChannels chan, EnumType messageCode, uint64_t timeStamp, const byte *buffer, int bufferLen,
                                      int overrideTimeout = -1) {
        return sendEncodedMediaPacket(retry, chan, static_cast<uint16_t>(messageCode), timeStamp, buffer, bufferLen, overrideTimeout);
    }

    template <typename EnumType>
    inline int sendUnencodedBlob(int retry, ServiceChannels chan, EnumType messageCode, const byte *buffer, int bufferLen, int overrideTimeout = -1) {
        return sendUnencodedBlob(retry, chan, static_cast<uint16_t>(messageCode), buffer, bufferLen, overrideTimeout);
    }

    template <typename EnumType>
    inline int sendUnencodedMessage(int retry, ServiceChannels chan, EnumType messageCode, const google::protobuf::MessageLite &message,
                                    int overrideTimeout = -1) {
        return sendUnencodedMessage(retry, chan, static_cast<uint16_t>(messageCode), message, overrideTimeout);
    }

    virtual int stop() = 0;
};

// These callbacks are executed in the HU thread


class HUServer : protected IHUConnectionThreadInterface {
public:
    // Must be called from the "main" thread (as defined by the user)
    int start();
    int shutdown();

    HUServer(HeadunitEventCallbacks &callbacks, std::map<std::string, std::string>);
    virtual ~HUServer() {
        shutdown();
    }

    inline IHUAnyThreadInterface &GetAnyThreadInterface() {
        return *this;
    }
    static std::map<std::string, int> getResolutions();
    static std::map<std::string, int> getFPS();

protected:
    HeadunitEventCallbacks &callbacks;
    std::unique_ptr<AbstractTransportStream> transport;
    HU_STATE iaap_state = hu_STATE_INITIAL;
    int iaap_tra_recv_tmo = 150;  // 100;//1;//10;//100;//250;//100;//250;//100;//25;
    // // 10 doesn't work ? 100 does
    int iaap_tra_send_tmo = 500;  // 2;//25;//250;//500;//100;//500;//250;
    std::vector<uint8_t> *temp_assembly_buffer = new std::vector<uint8_t>();
    std::map<int, std::vector<uint8_t> *> channel_assembly_buffers;
    byte enc_buf[MAX_FRAME_SIZE] = {0};
    int32_t channel_session_id[MaximumChannel] = {0};

    std::thread hu_thread;
    int command_read_fd = -1;
    int command_write_fd = -1;
    bool hu_thread_quit_flag = false;

    // Can be called from any thread
    HUThreadCommand *popCommand();
    virtual int queueCommand(IHUAnyThreadInterface::HUThreadCommand &&command) override;

    void mainThread();

    SSL *m_ssl = nullptr;
    SSL_CTX *m_sslContext = nullptr;
    SSL_METHOD *m_sslMethod = nullptr;
    BIO *m_sslWriteBio = nullptr;
    BIO *m_sslReadBio = nullptr;

    void logSSLReturnCode(int ret);
    void logSSLInfo();

    int beginSSLHandshake();
    int sendSSLHandshakePacket();
    int handleSSLHandshake(byte *buf, int len);

    int startTransport();
    int stopTransport();
    int receiveTransportPacket(byte *buf, int len,
                               int tmo);  // Used by intern, hu_ssl
    int sendTransportPacket(int retry, byte *buf, int len,
                            int tmo);  // Used by intern, hu_ssl

    int processMessage(ServiceChannels chan, uint16_t msg_type, byte *buf, int len);
    int sendEncoded(int retry, ServiceChannels chan, byte *buf, int len,
                    int overrideTimeout = -1);  // Used by intern, hu_jni //
    // Encrypted Send
    int sendUnencoded(int retry, ServiceChannels chan, byte *buf, int len, int overrideTimeout = -1);

    int processReceived(int tmo);  // Used by          hu_mai,  hu_jni //
                                   // Process 1 encrypted receive message
                                   // set: Respond to decrypted message
    virtual int sendEncodedMessage(int retry, ServiceChannels chan, uint16_t messageCode, const google::protobuf::MessageLite &message,
                                   int overrideTimeout = -1) override;
    virtual int sendEncodedMediaPacket(int retry, ServiceChannels chan, uint16_t messageCode, uint64_t timeStamp, const byte *buffer, int bufferLen,
                                       int overrideTimeout = -1) override;
    virtual int sendUnencodedBlob(int retry, ServiceChannels chan, uint16_t messageCode, const byte *buffer, int bufferLen,
                                  int overrideTimeout = -1) override;
    virtual int sendUnencodedMessage(int retry, ServiceChannels chan, uint16_t messageCode, const google::protobuf::MessageLite &message,
                                     int overrideTimeout = -1) override;
    virtual int stop() override;

    using IHUConnectionThreadInterface::sendEncodedMediaPacket;
    using IHUConnectionThreadInterface::sendEncodedMessage;
    using IHUConnectionThreadInterface::sendUnencodedBlob;
    using IHUConnectionThreadInterface::sendUnencodedMessage;

    int handle_VersionResponse(ServiceChannels chan, byte *buf, int len);
    int handle_ServiceDiscoveryRequest(ServiceChannels chan, byte *buf, int len);
    int handle_PingRequest(ServiceChannels chan, byte *buf, int len);
    int handle_NavigationFocusRequest(ServiceChannels chan, byte *buf, int len);
    int handle_ShutdownRequest(ServiceChannels chan, byte *buf, int len);
    int handle_VoiceSessionRequest(ServiceChannels chan, byte *buf, int len);
    int handle_AudioFocusRequest(ServiceChannels chan, byte *buf, int len);
    int handle_ChannelOpenRequest(ServiceChannels chan, byte *buf, int len);
    int handle_MediaSetupRequest(ServiceChannels chan, byte *buf, int len);
    int handle_VideoFocusRequest(ServiceChannels chan, byte *buf, int len);
    int handle_MediaStartRequest(ServiceChannels chan, byte *buf, int len);
    int handle_MediaStopRequest(ServiceChannels chan, byte *buf, int len);
    int handle_SensorStartRequest(ServiceChannels chan, byte *buf, int len);
    int handle_BindingRequest(ServiceChannels chan, byte *buf, int len);
    int handle_MediaAck(ServiceChannels chan, byte *buf, int len);
    int handle_MicRequest(ServiceChannels chan, byte *buf, int len);
    int handle_MediaDataWithTimestamp(ServiceChannels chan, byte *buf, int len);
    int handle_MediaData(ServiceChannels chan, byte *buf, int len);
    int handle_PhoneStatus(ServiceChannels chan, byte *buf, int len);
    int handle_GenericNotificationResponse(ServiceChannels chan, byte *buf, int len);
    int handle_StartGenericNotifications(ServiceChannels chan, byte *buf, int len);
    int handle_StopGenericNotifications(ServiceChannels chan, byte *buf, int len);
    int handle_BluetoothPairingRequest(ServiceChannels chan, byte *buf, int len);
    int handle_BluetoothAuthData(ServiceChannels chan, byte *buf, int len);
    int handle_NaviStatus(ServiceChannels chan, byte *buf, int len);
    int handle_NaviTurn(ServiceChannels chan, byte *buf, int len);
    int handle_NaviTurnDistance(ServiceChannels chan, byte *buf, int len);

private:
    std::map<std::string, std::string> settings;
};
}  // namespace AndroidAuto
