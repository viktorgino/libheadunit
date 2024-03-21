#pragma once
#include <cstdint>

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

const char *state_get(int s);
const char *getChannel(ServiceChannels chan);

inline const char *state_get(int state) {
    switch (state) {
        case HU_STATE::hu_STATE_INITIAL:  // 0
            return ("hu_STATE_INITIAL");
        case HU_STATE::hu_STATE_STARTIN:  // 1
            return ("hu_STATE_STARTIN");
        case HU_STATE::hu_STATE_STARTED:  // 2
            return ("hu_STATE_STARTED");
        case HU_STATE::hu_STATE_STOPPIN:  // 3
            return ("hu_STATE_STOPPIN");
        case HU_STATE::hu_STATE_STOPPED:  // 4
            return ("hu_STATE_STOPPED");
    }
    return ("hu_STATE Unknown error");
}

inline const char *getChannel(ServiceChannels chan) {
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

enum class INIT_MESSAGE : uint16_t {
    VersionRequest = 0x0001,
    VersionResponse = 0x0002,
    SSLHandshake = 0x0003,
    AuthComplete = 0x0004,
};

enum class PROTOCOL_MESSAGE : uint16_t {
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

enum class MEDIA_CHANNEL_MESSAGE : uint16_t {
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

enum class SENSOR_CHANNEL_MESSAGE : uint16_t {
    SensorStartRequest = 0x8001,
    SensorStartResponse = 0x8002,
    SensorEvent = 0x8003,
};

enum class INPUT_CHANNEL_MESSAGE : uint16_t {
    InputEvent = 0x8001,
    BindingRequest = 0x8002,
    BindingResponse = 0x8003,
};

enum class PHONE_STATUS_CHANNEL_MESSAGE : uint16_t {
    PhoneStatus = 0x8001,
    PhoneStatusInput = 0x8002,
};

enum class BLUETOOTH_CHANNEL_MESSAGE : uint16_t {
    BluetoothPairingRequest = 0x8001,
    BluetoothPairingResponse = 0x8002,
    BluetoothAuthData = 0x8003,
};

// Not sure if these are right
enum class GENERIC_NOTIFICATIONS_CHANNEL_MESSAGE : uint16_t {
    StartGenericNotifications = 0x8001,
    StopGenericNotifications = 0x8002,
    GenericNotificationRequest = 0x8003,
    GenericNotificationResponse = 0x8004,
};

enum INPUT_BUTTON {
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
}