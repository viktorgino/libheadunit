#pragma once
#include <functional>
#include <map>
#include <string>
#include <thread>
#include "hu.pb.h"
#include "hu_ssl.h"
#include "hu_uti.h"

    namespace AndroidAuto {

        enum HU_STATE {
            hu_STATE_INITIAL = 0,
            hu_STATE_STARTIN = 1,
            hu_STATE_STARTED = 2,
            hu_STATE_STOPPIN = 3,
            hu_STATE_STOPPED = 4,
            };
    enum ServiceChannels {
        ControlChannel = 0,
        TouchChannel,
        SensorChannel,
        VideoChannel,
        MediaAudioChannel,
        Audio1Channel,
        Audio2Channel,
        MicrophoneChannel,
        BluetoothChannel,
        PhoneStatusChannel,
        NotificationChannel,
        NavigationChannel,
        MaximumChannel = 256
    };

    const char* state_get(int s);
    const char* getChannel(ServiceChannels chan);

    inline const char* state_get(int state) {
        switch (state) {
        case hu_STATE_INITIAL:  // 0
            return ("hu_STATE_INITIAL");
        case hu_STATE_STARTIN:  // 1
            return ("hu_STATE_STARTIN");
        case hu_STATE_STARTED:  // 2
            return ("hu_STATE_STARTED");
        case hu_STATE_STOPPIN:  // 3
            return ("hu_STATE_STOPPIN");
        case hu_STATE_STOPPED:  // 4
            return ("hu_STATE_STOPPED");
        }
        return ("hu_STATE Unknown error");
    }

    inline const char* getChannel(ServiceChannels chan) {
        switch (chan) {
        case ControlChannel:
            return ("Control Channel");
        case TouchChannel:
            return ("Touch Channel");
        case SensorChannel:
            return ("Sensor Channel");
        case VideoChannel:
            return ("Video Channel");
        case MediaAudioChannel:
            return ("Media Audio Channel");
        case Audio1Channel:
            return ("Voice Audio Channel");
        case Audio2Channel:
            return ("Voice Audio Channel 2");
        case MicrophoneChannel:
            return ("Microphone Channel");
        case BluetoothChannel:
            return ("Bluetooth Channel");
        case PhoneStatusChannel:
            return ("Phone Status Channel");
        case NotificationChannel:
            return ("Notification Channel");
        case NavigationChannel:
            return ("Navigation Channel");
        default:
            return ("<Invalid>");
        }
    }

        enum HU_FRAME_FLAGS {
            HU_FRAME_FIRST_FRAME = 1 << 0,
            HU_FRAME_LAST_FRAME = 1 << 1,
            HU_FRAME_CONTROL_MESSAGE = 1 << 2,
            HU_FRAME_ENCRYPTED = 1 << 3,
            };

#define MAX_FRAME_PAYLOAD_SIZE 0x4000
// At 16 bytes for header
#define MAX_FRAME_SIZE 0x4100

    class HUTransportStream {
    protected:
        int readfd = -1;
        // optional if required for pipe, etc
        int errorfd = -1;

    public:
        virtual ~HUTransportStream() {}
        inline HUTransportStream(std::map<std::string, std::string>) {}
        virtual int Start() = 0;
        virtual int Stop() = 0;
        virtual int Write(const byte* buf, int len, int tmo) = 0;

        inline int GetReadFD() { return readfd; }
        inline int GetErrorFD() { return errorfd; }
    };

    class IHUConnectionThreadInterface;

    class IHUAnyThreadInterface {
    protected:
        ~IHUAnyThreadInterface() {}
        IHUAnyThreadInterface() {}

    public:
        typedef std::function<void(IHUConnectionThreadInterface&)> HUThreadCommand;
        // Can be called from any thread
        virtual int queueCommand(HUThreadCommand&& command) = 0;
    };

    class IHUConnectionThreadInterface : public IHUAnyThreadInterface {
    protected:
        ~IHUConnectionThreadInterface() {}
        IHUConnectionThreadInterface() {}

    public:
        virtual int sendEncodedMessage(int retry, ServiceChannels chan, uint16_t messageCode,
                                       const google::protobuf::MessageLite& message,
                                       int overrideTimeout = -1) = 0;
        virtual int sendEncodedMediaPacket(int retry, ServiceChannels chan,
                                           uint16_t messageCode, uint64_t timeStamp,
                                           const byte* buffer, int bufferLen,
                                           int overrideTimeout = -1) = 0;
        virtual int sendUnencodedBlob(int retry, ServiceChannels chan, uint16_t messageCode,
                                      const byte* buffer, int bufferLen,
                                      int overrideTimeout = -1) = 0;
        virtual int sendUnencodedMessage(
            int retry, ServiceChannels chan, uint16_t messageCode,
            const google::protobuf::MessageLite& message,
            int overrideTimeout = -1) = 0;

        template <typename EnumType>
        inline int sendEncodedMessage(int retry, ServiceChannels chan, EnumType messageCode,
                                      const google::protobuf::MessageLite& message,
                                      int overrideTimeout = -1) {
            return sendEncodedMessage(retry, chan,
                                      static_cast<uint16_t>(messageCode), message,
                                      overrideTimeout);
        }

        template <typename EnumType>
        inline int sendEncodedMediaPacket(int retry, ServiceChannels chan, EnumType messageCode,
                                          uint64_t timeStamp, const byte* buffer,
                                          int bufferLen, int overrideTimeout = -1) {
            return sendEncodedMediaPacket(
                retry, chan, static_cast<uint16_t>(messageCode), timeStamp, buffer,
                bufferLen, overrideTimeout);
        }

        template <typename EnumType>
        inline int sendUnencodedBlob(int retry, ServiceChannels chan, EnumType messageCode,
                                     const byte* buffer, int bufferLen,
                                     int overrideTimeout = -1) {
            return sendUnencodedBlob(retry, chan,
                                     static_cast<uint16_t>(messageCode), buffer,
                                     bufferLen, overrideTimeout);
        }

        template <typename EnumType>
        inline int sendUnencodedMessage(
            int retry, ServiceChannels chan, EnumType messageCode,
            const google::protobuf::MessageLite& message,
            int overrideTimeout = -1) {
            return sendUnencodedMessage(retry, chan,
                                        static_cast<uint16_t>(messageCode), message,
                                        overrideTimeout);
        }

        virtual int stop() = 0;
    };

    // These callbacks are executed in the HU thread
    class IHUConnectionThreadEventCallbacks {
    protected:
        ~IHUConnectionThreadEventCallbacks() {}
        IHUConnectionThreadEventCallbacks() {}

    public:
        // return > 0 if handled < 0 for error
        virtual int MessageFilter(IHUConnectionThreadInterface& stream,
                                  HU_STATE state, ServiceChannels chan, uint16_t msg_type,
                                  const byte* buf, int len) {
            return 0;
        }

        // return -1 for error
        virtual int MediaPacket(ServiceChannels chan, uint64_t timestamp, const byte* buf,
                                int len) = 0;
        virtual int MediaStart(ServiceChannels chan) = 0;
        virtual int MediaStop(ServiceChannels chan) = 0;
        virtual void MediaSetupComplete(ServiceChannels chan) = 0;

        virtual void DisconnectionOrError() = 0;

        virtual void CustomizeCarInfo(HU::ServiceDiscoveryResponse& carInfo) {}
        virtual void CustomizeInputConfig(
            HU::ChannelDescriptor::InputEventChannel& inputChannel) {}
        virtual void CustomizeSensorConfig(
            HU::ChannelDescriptor::SensorChannel& sensorChannel) {}
        virtual void CustomizeOutputChannel(
            ServiceChannels chan, HU::ChannelDescriptor::OutputStreamChannel& streamChannel) {}
        virtual void CustomizeInputChannel(
            ServiceChannels chan, HU::ChannelDescriptor::InputStreamChannel& streamChannel) {}
        virtual void CustomizeBluetoothService(
            ServiceChannels chan, HU::ChannelDescriptor::BluetoothService& bluetoothService) {}

        // returning a empty string means no bluetooth
        virtual std::string GetCarBluetoothAddress() { return std::string(); }
        virtual void PhoneBluetoothReceived(std::string address) {}

        virtual void AudioFocusRequest(ServiceChannels chan,
                                       const HU::AudioFocusRequest& request) = 0;
        virtual void VideoFocusRequest(ServiceChannels chan,
                                       const HU::VideoFocusRequest& request) = 0;

        virtual void HandlePhoneStatus(IHUConnectionThreadInterface& stream,
                                       const HU::PhoneStatus& phoneStatus) {}

        virtual void HandleGenericNotificationResponse(IHUConnectionThreadInterface& stream,
                                                       const HU::GenericNotificationResponse& response) {}

        virtual void ShowingGenericNotifications(IHUConnectionThreadInterface&
                                                     stream, bool bIsShowing) {}
        virtual void HandleNaviStatus(IHUConnectionThreadInterface& stream,
                                      const HU::NAVMessagesStatus& request) {}
        virtual void HandleNaviTurn(IHUConnectionThreadInterface& stream,
                                    const HU::NAVTurnMessage& request) {}
        virtual void HandleNaviTurnDistance(IHUConnectionThreadInterface& stream,
                                            const HU::NAVDistanceMessage& request) {
        }
    };

    class HUServer : protected IHUConnectionThreadInterface {
    public:
        // Must be called from the "main" thread (as defined by the user)
        int start();
        int shutdown();

        HUServer(IHUConnectionThreadEventCallbacks& callbacks,
                 std::map<std::string, std::string>);
        virtual ~HUServer() { shutdown(); }

        inline IHUAnyThreadInterface& GetAnyThreadInterface() { return *this; }
        static std::map<std::string, int> getResolutions();
        static std::map<std::string, int> getFPS();

    protected:
        IHUConnectionThreadEventCallbacks& callbacks;
        std::unique_ptr<HUTransportStream> transport;
        HU_STATE iaap_state = hu_STATE_INITIAL;
        int iaap_tra_recv_tmo =
            150;  // 100;//1;//10;//100;//250;//100;//250;//100;//25;
            // // 10 doesn't work ? 100 does
        int iaap_tra_send_tmo = 500;  // 2;//25;//250;//500;//100;//500;//250;
        std::vector<uint8_t>* temp_assembly_buffer = new std::vector<uint8_t>();
        std::map<int, std::vector<uint8_t>*> channel_assembly_buffers;
        byte enc_buf[MAX_FRAME_SIZE] = {0};
        int32_t channel_session_id[MaximumChannel] = {0};

        std::thread hu_thread;
        int command_read_fd = -1;
        int command_write_fd = -1;
        bool hu_thread_quit_flag = false;

        // Can be called from any thread
        HUThreadCommand* popCommand();
        virtual int queueCommand(
            IHUAnyThreadInterface::HUThreadCommand&& command) override;

        void mainThread();

        SSL* m_ssl = nullptr;
        SSL_CTX* m_sslContext = nullptr;
        SSL_METHOD* m_sslMethod = nullptr;
        BIO* m_sslWriteBio = nullptr;
        BIO* m_sslReadBio = nullptr;

        void logSSLReturnCode(int ret);
        void logSSLInfo();

        int beginSSLHandshake();
        int sendSSLHandshakePacket();
        int handleSSLHandshake(byte* buf, int len);

        int startTransport();
        int stopTransport();
        int receiveTransportPacket(byte* buf, int len,
                                   int tmo);  // Used by intern, hu_ssl
        int sendTransportPacket(int retry, byte* buf, int len,
                                int tmo);  // Used by intern, hu_ssl

        int processMessage(ServiceChannels chan, uint16_t msg_type, byte* buf, int len);
        int sendEncoded(int retry, ServiceChannels chan, byte* buf, int len,
                        int overrideTimeout = -1);  // Used by intern, hu_jni     //
            // Encrypted Send
        int sendUnencoded(int retry, ServiceChannels chan, byte* buf, int len,
                          int overrideTimeout = -1);

        int processReceived(int tmo);  // Used by          hu_mai,  hu_jni     //
            // Process 1 encrypted receive message set:
            // Respond to decrypted message
        virtual int sendEncodedMessage(int retry, ServiceChannels chan, uint16_t messageCode,
                                       const google::protobuf::MessageLite& message,
                                       int overrideTimeout = -1) override;
        virtual int sendEncodedMediaPacket(int retry, ServiceChannels chan,
                                           uint16_t messageCode, uint64_t timeStamp,
                                           const byte* buffer, int bufferLen,
                                           int overrideTimeout = -1) override;
        virtual int sendUnencodedBlob(int retry, ServiceChannels chan, uint16_t messageCode,
                                      const byte* buffer, int bufferLen,
                                      int overrideTimeout = -1) override;
        virtual int sendUnencodedMessage(
            int retry, ServiceChannels chan, uint16_t messageCode,
            const google::protobuf::MessageLite& message,
            int overrideTimeout = -1) override;
        virtual int stop() override;

        using IHUConnectionThreadInterface::sendEncodedMediaPacket;
        using IHUConnectionThreadInterface::sendEncodedMessage;
        using IHUConnectionThreadInterface::sendUnencodedBlob;
        using IHUConnectionThreadInterface::sendUnencodedMessage;

        int handle_VersionResponse(ServiceChannels chan, byte* buf, int len);
        int handle_ServiceDiscoveryRequest(ServiceChannels chan, byte* buf, int len);
        int handle_PingRequest(ServiceChannels chan, byte* buf, int len);
        int handle_NavigationFocusRequest(ServiceChannels chan, byte* buf, int len);
        int handle_ShutdownRequest(ServiceChannels chan, byte* buf, int len);
        int handle_VoiceSessionRequest(ServiceChannels chan, byte* buf, int len);
        int handle_AudioFocusRequest(ServiceChannels chan, byte* buf, int len);
        int handle_ChannelOpenRequest(ServiceChannels chan, byte* buf, int len);
        int handle_MediaSetupRequest(ServiceChannels chan, byte* buf, int len);
        int handle_VideoFocusRequest(ServiceChannels chan, byte* buf, int len);
        int handle_MediaStartRequest(ServiceChannels chan, byte* buf, int len);
        int handle_MediaStopRequest(ServiceChannels chan, byte* buf, int len);
        int handle_SensorStartRequest(ServiceChannels chan, byte* buf, int len);
        int handle_BindingRequest(ServiceChannels chan, byte* buf, int len);
        int handle_MediaAck(ServiceChannels chan, byte* buf, int len);
        int handle_MicRequest(ServiceChannels chan, byte* buf, int len);
        int handle_MediaDataWithTimestamp(ServiceChannels chan, byte* buf, int len);
        int handle_MediaData(ServiceChannels chan, byte* buf, int len);
        int handle_PhoneStatus(ServiceChannels chan, byte* buf, int len);
        int handle_GenericNotificationResponse(ServiceChannels chan, byte* buf, int len);
        int handle_StartGenericNotifications(ServiceChannels chan, byte* buf, int len);
        int handle_StopGenericNotifications(ServiceChannels chan, byte* buf, int len);
        int handle_BluetoothPairingRequest(ServiceChannels chan, byte* buf, int len);
        int handle_BluetoothAuthData(ServiceChannels chan, byte* buf, int len);
        int handle_NaviStatus(ServiceChannels chan, byte* buf, int len);
        int handle_NaviTurn(ServiceChannels chan, byte* buf, int len);
        int handle_NaviTurnDistance(ServiceChannels chan, byte* buf, int len);

    private:
        std::map<std::string, std::string> settings;
    };

        enum class HU_INIT_MESSAGE : uint16_t {
            VersionRequest = 0x0001,
            VersionResponse = 0x0002,
            SSLHandshake = 0x0003,
            AuthComplete = 0x0004,
            };

        enum class HU_PROTOCOL_MESSAGE : uint16_t {
            MediaDataWithTimestamp = 0x0000,
            MediaData = 0x0001,
            ServiceDiscoveryRequest = 0x0005,
            ServiceDiscoveryResponse = 0x0006,
            ChannelOpenRequest = 0x0007,
            ChannelOpenResponse = 0x0008,
            PingRequest = 0x000b,
            PingResponse = 0x000c,
            NavigationFocusRequest = 0x000d,
            NavigationFocusResponse = 0x000e,
            ShutdownRequest = 0x000f,
            ShutdownResponse = 0x0010,
            VoiceSessionRequest = 0x0011,
            AudioFocusRequest = 0x0012,
            AudioFocusResponse = 0x0013,
            };  // If video data, put on queue

        enum class HU_MEDIA_CHANNEL_MESSAGE : uint16_t {
            MediaSetupRequest = 0x8000,   // Setup
            MediaStartRequest = 0x8001,   // Start
            MediaStopRequest = 0x8002,    // Stop
            MediaSetupResponse = 0x8003,  // Config
            MediaAck = 0x8004,
            MicRequest = 0x8005,
            MicReponse = 0x8006,
            VideoFocusRequest = 0x8007,
            VideoFocus = 0x8008,
            };

        enum class HU_SENSOR_CHANNEL_MESSAGE : uint16_t {
            SensorStartRequest = 0x8001,
            SensorStartResponse = 0x8002,
            SensorEvent = 0x8003,
            };

        enum class HU_INPUT_CHANNEL_MESSAGE : uint16_t {
            InputEvent = 0x8001,
            BindingRequest = 0x8002,
            BindingResponse = 0x8003,
            };

        enum class HU_PHONE_STATUS_CHANNEL_MESSAGE : uint16_t {
            PhoneStatus = 0x8001,
            PhoneStatusInput = 0x8002,
            };

        enum class HU_BLUETOOTH_CHANNEL_MESSAGE : uint16_t {
            BluetoothPairingRequest = 0x8001,
            BluetoothPairingResponse = 0x8002,
            BluetoothAuthData = 0x8003,
            };

        // Not sure if these are right
        enum class HU_GENERIC_NOTIFICATIONS_CHANNEL_MESSAGE : uint16_t {
            StartGenericNotifications = 0x8001,
            StopGenericNotifications = 0x8002,
            GenericNotificationRequest = 0x8003,
            GenericNotificationResponse = 0x8004,
            };

        enum HU_INPUT_BUTTON {
            HUIB_MIC1 = 0x01,
            HUIB_MENU = 0x02,
            HUIB_HOME = 0x03,
            HUIB_BACK = 0x04,
            HUIB_PHONE = 0x05,
            HUIB_CALLEND = 0x06,
            // HUIB_NAV = 0x07,
            HUIB_UP = 0x13,
            HUIB_DOWN = 0x14,
            HUIB_LEFT = 0x15,
            HUIB_RIGHT = 0x16,
            HUIB_ENTER = 0x17,
            HUIB_MIC = 0x54,
            HUIB_PLAYPAUSE = 0x55,
            HUIB_NEXT = 0x57,
            HUIB_PREV = 0x58,
            HUIB_START = 0x7E,
            HUIB_STOP = 0x7F,
            HUIB_MUSIC = 0xD1,
            HUIB_SCROLLWHEEL = 65536,
            HUIB_MEDIA = 65537,
            HUIB_NAVIGATION = 65538,
            HUIB_RADIO = 65539,
            HUIB_TEL = 65540,
            HUIB_PRIMARY_BUTTON = 65541,
            HUIB_SECONDARY_BUTTON = 65542,
            HUIB_TERTIARY_BUTTON = 65543,
            };

        enum class HU_NAVI_CHANNEL_MESSAGE : uint16_t {
            Status = 0x8003,
            Turn = 0x8004,
            TurnDistance = 0x8005,
            };

}  // namespace AndroidAuto
