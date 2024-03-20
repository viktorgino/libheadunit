#pragma once
#include "defs.h"

namespace AndroidAuto {

class IHUConnectionThreadInterface;
class HeadunitEventCallbacks {
protected:
    ~HeadunitEventCallbacks() {
    }
    HeadunitEventCallbacks() {
    }

public:
    // return > 0 if handled < 0 for error
    virtual int MessageFilter(IHUConnectionThreadInterface &stream, HU_STATE state, ServiceChannels chan, uint16_t msg_type, const byte *buf,
                              int len) {
        return 0;
    }

    // return -1 for error
    virtual int MediaPacket(ServiceChannels chan, uint64_t timestamp, const byte *buf, int len) = 0;
    virtual int MediaStart(ServiceChannels chan) = 0;
    virtual int MediaStop(ServiceChannels chan) = 0;
    virtual void MediaSetupComplete(ServiceChannels chan) = 0;

    virtual void DisconnectionOrError() = 0;

    virtual void CustomizeCarInfo(HU::ServiceDiscoveryResponse &carInfo) {
    }
    virtual void CustomizeInputConfig(HU::ChannelDescriptor::InputEventChannel &inputChannel) {
    }
    virtual void CustomizeSensorConfig(HU::ChannelDescriptor::SensorChannel &sensorChannel) {
    }
    virtual void CustomizeOutputChannel(ServiceChannels chan, HU::ChannelDescriptor::OutputStreamChannel &streamChannel) {
    }
    virtual void CustomizeInputChannel(ServiceChannels chan, HU::ChannelDescriptor::InputStreamChannel &streamChannel) {
    }
    virtual void CustomizeBluetoothService(ServiceChannels chan, HU::ChannelDescriptor::BluetoothService &bluetoothService) {
    }

    // returning a empty string means no bluetooth
    virtual std::string GetCarBluetoothAddress() {
        return std::string();
    }
    virtual void PhoneBluetoothReceived(std::string address) {
    }

    virtual void AudioFocusRequest(ServiceChannels chan, const HU::AudioFocusRequest &request) = 0;
    virtual void VideoFocusRequest(ServiceChannels chan, const HU::VideoFocusRequest &request) = 0;

    virtual void HandlePhoneStatus(IHUConnectionThreadInterface &stream, const HU::PhoneStatus &phoneStatus) {
    }

    virtual void HandleGenericNotificationResponse(IHUConnectionThreadInterface &stream, const HU::GenericNotificationResponse &response) {
    }

    virtual void ShowingGenericNotifications(IHUConnectionThreadInterface &stream, bool bIsShowing) {
    }
    virtual void HandleNaviStatus(IHUConnectionThreadInterface &stream, const HU::NAVMessagesStatus &request) {
    }
    virtual void HandleNaviTurn(IHUConnectionThreadInterface &stream, const HU::NAVTurnMessage &request) {
    }
    virtual void HandleNaviTurnDistance(IHUConnectionThreadInterface &stream, const HU::NAVDistanceMessage &request) {
    }
};
}