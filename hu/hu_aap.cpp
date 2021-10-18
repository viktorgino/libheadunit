// Android Auto Protocol Handler
#include <pthread.h>
#define LOGTAG "hu_aap"
#include <endian.h>
#include <google/protobuf/descriptor.h>

#include <fstream>
#include <iostream>
#include <memory>

#include "hu_aad.h"
#include "hu_aap.h"
#include "hu_ssl.h"
#include "hu_uti.h"

#include "hu_tcp.h"
#include "hu_usb.h"

using namespace AndroidAuto;

HUServer::HUServer(IHUConnectionThreadEventCallbacks& callbacks,
                   std::map<std::string, std::string> _settings)
    : callbacks(callbacks) {
    settings = _settings;

    // Defaults
    std::map<std::string, std::string> default_settings;

    default_settings["head_unit_name"] = "Computer";
    default_settings["car_model"] = "libheadunit";
    default_settings["car_year"] = "2017";
    default_settings["car_serial"] = "007";
    default_settings["driver_pos"] = "1";  // bool
    default_settings["headunit_make"] = "libhu";
    default_settings["headunit_model"] = "libheadunit";
    default_settings["sw_build"] = "SWB1";
    default_settings["sw_version"] = "SWV1";
    default_settings["can_play_native_media_during_vr"] = "0";  // bool
    default_settings["hide_clock"] = "0";                       // bool
    default_settings["ts_width"] = "800";
    default_settings["ts_height"] = "480";
    default_settings["resolution"] =
        "1";  // 800x480 = 1, 1280x720 = 2, 1920x1080 = 3
    default_settings["frame_rate"] = "1";  // 30 FPS = 1, 60 FPS = 2
    default_settings["margin_width"] = "0";
    default_settings["margin_height"] = "0";
    default_settings["dpi"] = "140";
    default_settings["available_while_in_call"] = "0";  // bool
    default_settings["transport_type"] = "usb";         // "usb" or "network"
    default_settings["network_address"] = "127.0.0.1";
    default_settings["wifi_direct"] = "0";

    settings.insert(default_settings.begin(), default_settings.end());
}

int HUServer::startTransport() {
    std::map<std::string, std::string> conf;
    if (settings["transport_type"] == "network") {
        conf["network_address"] = settings["network_address"];
        logd("AA over Wifi");
        transport =
            std::unique_ptr<HUTransportStream>(new HUTransportStreamTCP(conf));
        iaap_tra_recv_tmo = 1000;
        iaap_tra_send_tmo = 2000;
    } else if (settings["transport_type"] == "usb") {
        transport =
            std::unique_ptr<HUTransportStream>(new HUTransportStreamUSB(conf));
        logd("AA over USB");
        iaap_tra_recv_tmo = 0;  // 100;
        iaap_tra_send_tmo = 2500;
    } else {
        loge("Unknown transport type");
        return -1;
    }
    return transport->Start();
}

int HUServer::stopTransport() {
    int ret = 0;
    if (transport) {
        ret = transport->Stop();
        transport.reset();
    }
    return ret;
}

int HUServer::receiveTransportPacket(byte* buf, int len, int tmo) {
    int ret = 0;
    if (iaap_state != hu_STATE_STARTED &&
        iaap_state != hu_STATE_STARTIN) {  // Need to recv when starting
        loge("CHECK: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        return (-1);
    }

    int readfd = transport->GetReadFD();
    int errorfd = transport->GetErrorFD();
    if (tmo > 0 || errorfd >= 0) {
        fd_set sock_set;
        FD_ZERO(&sock_set);
        FD_SET(readfd, &sock_set);
        int maxfd = readfd;
        if (errorfd >= 0) {
            maxfd = std::max(maxfd, errorfd);
            FD_SET(errorfd, &sock_set);
        }

        timeval tv_timeout = {1, 0};

        int ret = select(maxfd + 1, &sock_set, NULL, NULL, &tv_timeout);
        if (ret < 0) {
            loge("error when select : %s (%d)", strerror(errno), errno);
            return ret;
        } else if (errorfd >= 0 && FD_ISSET(errorfd, &sock_set)) {
            // got an error
            loge("errorf was signaled");
            return -1;
        } else if (ret == 0) {
            loge("hu_aap_tra_recv Timeout");
            return -1;
        }
    }

    ret = read(readfd, buf, len);
    if (ret < 0) {
        loge("ihu_tra_recv() error so stop Transport & AAP  ret: %d", ret);
        stop();
    }
    return (ret);
}

int log_packet_info = 1;

int HUServer::sendTransportPacket(
    int retry, byte* buf, int len,
    int tmo) {  // Send Transport data: chan,flags,len,type,...
    // Need to send when starting
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN) {
        loge("CHECK: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        return (-1);
    }

    int ret = transport->Write(buf, len, tmo);
    if (ret < 0 || ret != len) {
        if (retry == 0) {
            loge(
                "Error ihu_tra_send() error so stop Transport & AAP  ret: %d  "
                "len: "
                "%d",
                ret, len);
            stop();
        }
        return (-1);
    }

    if (ena_log_verbo && ena_log_aap_send)
        logd("OK ihu_tra_send() ret: %d  len: %d", ret, len);
    return (ret);
}

int HUServer::sendEncodedMessage(int retry, ServiceChannels chan, uint16_t messageCode,
    const google::protobuf::MessageLite& message, int overrideTimeout) {
    const int messageSize = message.ByteSizeLong();
    const int requiredSize = messageSize + 2;
    if (temp_assembly_buffer->size() <
        static_cast<unsigned int>(requiredSize)) {
        temp_assembly_buffer->resize(static_cast<unsigned int>(requiredSize));
    }

    uint16_t* destMessageCode =
        reinterpret_cast<uint16_t*>(temp_assembly_buffer->data());
    *destMessageCode++ = htobe16(messageCode);

    if (!message.SerializeToArray(destMessageCode, messageSize)) {
        loge("AppendToString failed for %s", message.GetTypeName().c_str());
        return -1;
    }

    logd("Send %s on channel %i %s", message.GetTypeName().c_str(), chan,
         getChannel(chan));
    // hex_dump("PB:", 80, temp_assembly_buffer->data(), requiredSize);
    return sendEncoded(retry, chan, temp_assembly_buffer->data(),
                           requiredSize, overrideTimeout);
}

int HUServer::sendEncodedMediaPacket(int retry, ServiceChannels chan,
                                           uint16_t messageCode,
                                           uint64_t timeStamp,
                                           const byte* buffer, int bufferLen,
                                           int overrideTimeout) {
    const int requiredSize = bufferLen + 2 + 8;
    if (temp_assembly_buffer->size() <
        static_cast<unsigned int>(requiredSize)) {
        temp_assembly_buffer->resize(static_cast<unsigned int>(requiredSize));
    }

    uint16_t* destMessageCode =
        reinterpret_cast<uint16_t*>(temp_assembly_buffer->data());
    *destMessageCode++ = htobe16(messageCode);

    uint64_t* destTimestamp = reinterpret_cast<uint64_t*>(destMessageCode);
    *destTimestamp++ = htobe64(timeStamp);

    memcpy(destTimestamp, buffer, bufferLen);

    // logd ("Send %s on channel %i %s", message.GetTypeName().c_str(), chan,
    // chan_get(chan)); hex_dump("PB:", 80, temp_assembly_buffer->data(),
    // requiredSize);
    return sendEncoded(retry, chan, temp_assembly_buffer->data(),
                           requiredSize, overrideTimeout);
}

int HUServer::sendEncoded(int retry, ServiceChannels chan, byte* buf, int len,
    int overrideTimeout) {  // Encrypt data and send: type,...
    if (iaap_state != hu_STATE_STARTED) {
        logw("CHECK: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        // logw ("chan: %d  len: %d  buf: %p", chan, len, buf);
        // hex_dump (" W/    hu_aap_enc_send: ", 16, buf, len);    // Byebye:
        // hu_aap_enc_send:  00000000 00 0f 08 00
        return (-1);
    }

    byte base_flags = HU_FRAME_ENCRYPTED;
    uint16_t message_type = be16toh(*((uint16_t*)buf));
    if (chan != ControlChannel && message_type >= 2 &&
        message_type <
            0x8000) {  // If not control channel and msg_type = 0 - 255
        // = control type message
        base_flags |=
            HU_FRAME_CONTROL_MESSAGE;  // Set Control Flag (On non-control
                                       // channels, indicates generic/"control
                                       // type" messages logd ("Setting
                                       // control");
    }

    for (int frag_start = 0; frag_start < len;
         frag_start += MAX_FRAME_PAYLOAD_SIZE) {
        byte flags = base_flags;

        if (frag_start == 0) {
            flags |= HU_FRAME_FIRST_FRAME;
        }
        int cur_len = MAX_FRAME_PAYLOAD_SIZE;
        if ((frag_start + MAX_FRAME_PAYLOAD_SIZE) >= len) {
            flags |= HU_FRAME_LAST_FRAME;
            cur_len = len - frag_start;
        }
#ifndef NDEBUG
        //    if (ena_log_verbo && ena_log_aap_send) {
        if (log_packet_info) {  // && ena_log_aap_send)
            char prefix[MAX_FRAME_SIZE] = {0};
            snprintf(prefix, sizeof(prefix), "S %d %s %1.1x", chan,
                     getChannel(chan),
                     flags);  // "S 1 VID B"
            hu_aad_dmp(prefix, "HU", chan, flags, &buf[frag_start], cur_len);
        }
#endif

        int bytes_written = SSL_write(m_ssl, &buf[frag_start],
                                      cur_len);  // Write plaintext to SSL
        if (bytes_written <= 0) {
            loge("SSL_write() bytes_written: %d", bytes_written);
            logSSLReturnCode(bytes_written);
            logSSLInfo();
            stop();
            return (-1);
        }
        if (bytes_written != cur_len)
            loge("SSL_write() cur_len: %d  bytes_written: %d  chan: %d %s",
                 cur_len, bytes_written, chan, getChannel(chan));
        else if (ena_log_verbo && ena_log_aap_send)
            logd("SSL_write() cur_len: %d  bytes_written: %d  chan: %d %s",
                 cur_len, bytes_written, chan, getChannel(chan));

        enc_buf[0] = (byte)chan;  // Encode channel and flags
        enc_buf[1] = flags;

        int header_size = 4;
        if ((flags & HU_FRAME_FIRST_FRAME) & !(flags & HU_FRAME_LAST_FRAME)) {
            // write total len
            *((uint32_t*)&enc_buf[header_size]) = htobe32(len);
            header_size += 4;
        }

        int bytes_read = BIO_read(
            m_sslReadBio, &enc_buf[header_size],
            sizeof(enc_buf) -
                header_size);  // Read encrypted from SSL BIO to enc_buf +

        if (bytes_read <= 0) {
            loge("BIO_read() bytes_read: %d", bytes_read);
            stop();
            return (-1);
        }
        if (ena_log_verbo && ena_log_aap_send)
            logd("BIO_read() bytes_read: %d", bytes_read);

        *((uint16_t*)&enc_buf[2]) = htobe16(bytes_read);

        int ret = 0;
        ret = sendTransportPacket(
            retry, enc_buf, bytes_read + header_size,
            overrideTimeout < 0
                ? iaap_tra_send_tmo
                : overrideTimeout);  // Send encrypted data to AA Server
        if (retry) return (ret);
    }

    return (0);
}

int HUServer::sendUnencoded(int retry, ServiceChannels chan, byte* buf, int len,
    int overrideTimeout) {  // Encrypt data and send: type,...
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN) {
        logw("CHECK: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        // logw ("chan: %d  len: %d  buf: %p", chan, len, buf);
        // hex_dump (" W/    hu_aap_enc_send: ", 16, buf, len);    // Byebye:
        // hu_aap_enc_send:  00000000 00 0f 08 00
        return (-1);
    }

    byte base_flags = 0;
    uint16_t message_type = be16toh(*((uint16_t*)buf));
    if (chan != ControlChannel && message_type >= 2 &&
        message_type <
            0x8000) {  // If not control channel and msg_type = 0 - 255
        // = control type message
        base_flags |=
            HU_FRAME_CONTROL_MESSAGE;  // Set Control Flag (On non-control
                                       // channels, indicates generic/"control
                                       // type" messages logd ("Setting
                                       // control");
    }

    logd("Sending hu_aap_unenc_send %i bytes", len);

    for (int frag_start = 0; frag_start < len;
         frag_start += MAX_FRAME_PAYLOAD_SIZE) {
        byte flags = base_flags;

        if (frag_start == 0) {
            flags |= HU_FRAME_FIRST_FRAME;
        }
        int cur_len = MAX_FRAME_PAYLOAD_SIZE;
        if ((frag_start + MAX_FRAME_PAYLOAD_SIZE) >= len) {
            flags |= HU_FRAME_LAST_FRAME;
            cur_len = len - frag_start;
        }

        logd("Frame %i : %i bytes", (int)flags, cur_len);
#ifndef NDEBUG
        //    if (ena_log_verbo && ena_log_aap_send) {
        if (log_packet_info) {  // && ena_log_aap_send)
            char prefix[MAX_FRAME_SIZE] = {0};
            snprintf(prefix, sizeof(prefix), "S %d %s %1.1x", chan,
                     getChannel(chan),
                     flags);  // "S 1 VID B"
            hu_aad_dmp(prefix, "HU", chan, flags, &buf[frag_start], cur_len);
        }
#endif

        enc_buf[0] = (byte)chan;  // Encode channel and flags
        enc_buf[1] = flags;
        *((uint16_t*)&enc_buf[2]) = htobe16(cur_len);
        int header_size = 4;
        if ((flags & HU_FRAME_FIRST_FRAME) & !(flags & HU_FRAME_LAST_FRAME)) {
            // write total len
            *((uint32_t*)&enc_buf[header_size]) = htobe32(len);
            header_size += 4;
        }

        memcpy(&enc_buf[header_size], &buf[frag_start], cur_len);

        return sendTransportPacket(
            retry, enc_buf, cur_len + header_size,
            overrideTimeout < 0
                ? iaap_tra_send_tmo
                : overrideTimeout);  // Send encrypted data to AA Server
    }

    return (0);
}

int HUServer::sendUnencodedBlob(int retry, ServiceChannels chan, uint16_t messageCode,
                                     const byte* buffer, int bufferLen,
                                     int overrideTimeout) {
    const int requiredSize = bufferLen + 2;
    if (temp_assembly_buffer->size() <
        static_cast<unsigned int>(requiredSize)) {
        temp_assembly_buffer->resize(static_cast<unsigned int>(requiredSize));
    }

    uint16_t* destMessageCode =
        reinterpret_cast<uint16_t*>(temp_assembly_buffer->data());
    *destMessageCode++ = htobe16(messageCode);

    memcpy(destMessageCode, buffer, bufferLen);

    // logd ("Send %s on channel %i %s", message.GetTypeName().c_str(), chan,
    // chan_get(chan)); hex_dump("PB:", 80, temp_assembly_buffer->data(),
    // requiredSize);
    return sendUnencoded(retry, chan, temp_assembly_buffer->data(),
                             requiredSize, overrideTimeout);
}

int HUServer::sendUnencodedMessage(int retry, ServiceChannels chan, uint16_t messageCode,
    const google::protobuf::MessageLite& message, int overrideTimeout) {
    const int messageSize = message.ByteSizeLong();
    const int requiredSize = messageSize + 2;
    if (temp_assembly_buffer->size() <
        static_cast<unsigned int>(requiredSize)) {
        temp_assembly_buffer->resize(static_cast<unsigned int>(requiredSize));
    }

    uint16_t* destMessageCode =
        reinterpret_cast<uint16_t*>(temp_assembly_buffer->data());
    *destMessageCode++ = htobe16(messageCode);

    if (!message.SerializeToArray(destMessageCode, messageSize)) {
        loge("AppendToString failed for %s", message.GetTypeName().c_str());
        return -1;
    }

    logd("Send %s on channel %i %s", message.GetTypeName().c_str(), chan,
         getChannel(chan));
    // hex_dump("PB:", 80, temp_assembly_buffer->data(), requiredSize);
    return sendUnencoded(retry, chan, temp_assembly_buffer->data(),
                             requiredSize, overrideTimeout);
}

int HUServer::handle_VersionResponse(ServiceChannels chan, byte* buf, int len) {
    logd("Version response recv len: %d", len);
    hex_dump("version", 40, buf, len);

    int ret =
        beginSSLHandshake();  // Do SSL Client Handshake with AA SSL server
    if (ret) {
        stop();
    }
    return (ret);
}

//  extern int wifi_direct;// = 0;//1;//0;
int HUServer::handle_ServiceDiscoveryRequest(ServiceChannels chan, byte* buf,
    int len) {  // Service Discovery Request

    HU::ServiceDiscoveryRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("Service Discovery Request: %x", buf[2]);
    else
        logd("Service Discovery Request: %s",
             request.phone_name()
                 .c_str());  // S 0 CTR b src: HU  lft:   113  msg_type:     6
                             // Service Discovery Response    S 0 CTR b 00000000
                             // 0a 08 08 01 12 04 0a 02 08 0b 0a 13 08 02 1a 0f

    HU::ServiceDiscoveryResponse carInfo;
    carInfo.set_head_unit_name(settings["head_unit_name"]);
    carInfo.set_car_model(settings["car_model"]);
    carInfo.set_car_year(settings["car_year"]);
    carInfo.set_car_serial(settings["car_serial"]);
    carInfo.set_driver_pos(std::stoi(settings["driver_pos"]));
    carInfo.set_headunit_make(settings["headunit_make"]);
    carInfo.set_headunit_model(settings["headunit_model"]);
    carInfo.set_sw_build(settings["sw_build"]);
    carInfo.set_sw_version(settings["sw_version"]);
    carInfo.set_can_play_native_media_during_vr(
        settings["can_play_native_media_during_vr"] == "true");
    carInfo.set_hide_clock(settings["hide_clock"] == "true");

    carInfo.mutable_channels()->Reserve(MaximumChannel);

    HU::ChannelDescriptor* inputChannel = carInfo.add_channels();
    inputChannel->set_channel_id(TouchChannel);
    {
        auto inner = inputChannel->mutable_input_event_channel();
        auto tsConfig = inner->mutable_touch_screen_config();
        tsConfig->set_width(stoul(settings["ts_width"]));
        tsConfig->set_height(stoul(settings["ts_height"]));

        // No idea what these mean since they aren't the same as HU_INPUT_BUTTON
        inner->add_keycodes_supported(HUIB_MENU);       // 0x01 Soft Left (Menu)
        inner->add_keycodes_supported(HUIB_MIC1);       // 0x02 Soft Right (Mic)
        inner->add_keycodes_supported(HUIB_HOME);       // 0x03 Home
        inner->add_keycodes_supported(HUIB_BACK);       // 0x04 Back
        inner->add_keycodes_supported(HUIB_PHONE);      // 0x05 Call
        inner->add_keycodes_supported(HUIB_CALLEND);    // 0x06 End Call
        inner->add_keycodes_supported(HUIB_UP);         // 0x13 Up
        inner->add_keycodes_supported(HUIB_DOWN);       // 0x14 Down
        inner->add_keycodes_supported(HUIB_LEFT);       // 0x15 Left (Menu)
        inner->add_keycodes_supported(HUIB_RIGHT);      // 0x16 Right (Mic)
        inner->add_keycodes_supported(HUIB_ENTER);      // 0x17 Select
        inner->add_keycodes_supported(HUIB_MIC);        // 0x54 Search (Mic)
        inner->add_keycodes_supported(HUIB_PLAYPAUSE);  // 0x55 Play/Pause
        inner->add_keycodes_supported(HUIB_NEXT);       // 0x57 Next Track
        inner->add_keycodes_supported(HUIB_PREV);       // 0x58 Prev Track
        inner->add_keycodes_supported(HUIB_MUSIC);      // 0xD1 Music Screen
        inner->add_keycodes_supported(
            HUIB_SCROLLWHEEL);                    // 65536 Comand Knob Rotate
        inner->add_keycodes_supported(HUIB_TEL);  // 65537 Phone
        inner->add_keycodes_supported(HUIB_NAVIGATION);  // 65538 Navigation
        inner->add_keycodes_supported(HUIB_MEDIA);       // 65539 Media
        // Might as well include these even if we dont use them
        inner->add_keycodes_supported(
            HUIB_RADIO);  // 65540 Radio (Doesn't Do Anything)
        inner->add_keycodes_supported(
            HUIB_PRIMARY_BUTTON);  // 65541 Primary (Doesn't Do Anything)
        inner->add_keycodes_supported(
            HUIB_SECONDARY_BUTTON);  // 65542 Secondary (Doesn't Do Anything)
        inner->add_keycodes_supported(
            HUIB_TERTIARY_BUTTON);  // 65543 Tertiary (Doesn't Do Anything)
        inner->add_keycodes_supported(HUIB_START);  // 0x7E (126) Start Media
        inner->add_keycodes_supported(HUIB_STOP);   // 0x7F (127) Stop Media

        callbacks.CustomizeInputConfig(*inner);
    }

    HU::ChannelDescriptor* sensorChannel = carInfo.add_channels();
    sensorChannel->set_channel_id(SensorChannel);
    {
        auto inner = sensorChannel->mutable_sensor_channel();
        inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_DRIVING_STATUS);
        inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_NIGHT_DATA);
        inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_LOCATION);

        callbacks.CustomizeSensorConfig(*inner);
    }

    HU::ChannelDescriptor* videoChannel = carInfo.add_channels();
    videoChannel->set_channel_id(VideoChannel);
    {
        auto inner = videoChannel->mutable_output_stream_channel();
        inner->set_type(HU::STREAM_TYPE_VIDEO);
        auto videoConfig = inner->add_video_configs();
        videoConfig->set_resolution(
            static_cast<HU::ChannelDescriptor::OutputStreamChannel::
                            VideoConfig::VIDEO_RESOLUTION>(
                std::stoi(settings["resolution"])));
        videoConfig->set_frame_rate(
            static_cast<HU::ChannelDescriptor::OutputStreamChannel::
                            VideoConfig::VIDEO_FPS>(
                std::stoi(settings["frame_rate"])));
        videoConfig->set_margin_width(std::stoi(settings["margin_width"]));
        videoConfig->set_margin_height(std::stoi(settings["margin_height"]));
        videoConfig->set_dpi(std::stoi(settings["dpi"]));
        inner->set_available_while_in_call(
            settings["available_while_in_call"] == "true");

        callbacks.CustomizeOutputChannel(VideoChannel, *inner);
    }

    HU::ChannelDescriptor* audioChannel0 = carInfo.add_channels();
    audioChannel0->set_channel_id(MediaAudioChannel);
    {
        auto inner = audioChannel0->mutable_output_stream_channel();
        inner->set_type(HU::STREAM_TYPE_AUDIO);
        inner->set_audio_type(HU::AUDIO_TYPE_MEDIA);
        auto audioConfig = inner->add_audio_configs();
        audioConfig->set_sample_rate(48000);
        audioConfig->set_bit_depth(16);
        audioConfig->set_channel_count(2);
        inner->set_available_while_in_call(
            settings["available_while_in_call"] == "true");

        callbacks.CustomizeOutputChannel(MediaAudioChannel, *inner);
    }

    HU::ChannelDescriptor* audioChannel1 = carInfo.add_channels();
    audioChannel1->set_channel_id(Audio1Channel);
    {
        auto inner = audioChannel1->mutable_output_stream_channel();
        inner->set_type(HU::STREAM_TYPE_AUDIO);
        inner->set_audio_type(HU::AUDIO_TYPE_SPEECH);
        auto audioConfig = inner->add_audio_configs();
        audioConfig->set_sample_rate(16000);
        audioConfig->set_bit_depth(16);
        audioConfig->set_channel_count(1);

        callbacks.CustomizeOutputChannel(Audio1Channel, *inner);
    }

    HU::ChannelDescriptor* micChannel = carInfo.add_channels();
    micChannel->set_channel_id(MicrophoneChannel);
    {
        auto inner = micChannel->mutable_input_stream_channel();
        inner->set_type(HU::STREAM_TYPE_AUDIO);
        auto audioConfig = inner->mutable_audio_config();
        audioConfig->set_sample_rate(16000);
        audioConfig->set_bit_depth(16);
        audioConfig->set_channel_count(1);
        callbacks.CustomizeInputChannel(MicrophoneChannel, *inner);
    }


    HU::ChannelDescriptor* notificationChannel = carInfo.add_channels();
    notificationChannel->set_channel_id(NotificationChannel);

    HU::ChannelDescriptor* navigationChannel = carInfo.add_channels();
    navigationChannel->set_channel_id(NavigationChannel);
    {
        auto inner = navigationChannel->mutable_navigation_status_service();

        inner->set_minimum_interval_ms(1000);
        // auto ImageOptions = inner->mutable_image_options();
        // ImageOptions->set_width(100);
        // ImageOptions->set_height(100);
        // ImageOptions->set_colour_depth_bits(8);
        inner->set_type(
            HU::ChannelDescriptor::NavigationStatusService::IMAGE_CODES_ONLY);
    }

    std::string carBTAddress = callbacks.GetCarBluetoothAddress();
    if (carBTAddress.size() > 0) {
        logd("Found BT address %s. Exposing Bluetooth service",
             carBTAddress.c_str());
        HU::ChannelDescriptor* btChannel = carInfo.add_channels();
        btChannel->set_channel_id(BluetoothChannel);
        {
            auto inner = btChannel->mutable_bluetooth_service();
            inner->set_car_address(carBTAddress);
            inner->add_supported_pairing_methods(
                HU::BLUETOOTH_PARING_METHOD_A2DP);
            inner->add_supported_pairing_methods(
                HU::BLUETOOTH_PARING_METHOD_HFP);
            callbacks.CustomizeBluetoothService(BluetoothChannel, *inner);
        }

        HU::ChannelDescriptor* phoneStatusChannel = carInfo.add_channels();
        phoneStatusChannel->set_channel_id(PhoneStatusChannel);

    } else {
        logw(
            "No Bluetooth or finding BT address failed. Not exposing Bluetooth "
            "service");
    }

    callbacks.CustomizeCarInfo(carInfo);

    return sendEncodedMessage(
        0, chan, HU_PROTOCOL_MESSAGE::ServiceDiscoveryResponse, carInfo);
}

int HUServer::handle_PingRequest(ServiceChannels chan, byte* buf,
                                    int len) {  // Ping Request
    HU::PingRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("Ping Request");
    else
        logd("Ping Request: %d", buf[3]);

    HU::PingResponse response;
    response.set_timestamp(request.timestamp());
    return sendEncodedMessage(0, chan, HU_PROTOCOL_MESSAGE::PingResponse,
                                   response);
}

int HUServer::handle_NavigationFocusRequest(
    ServiceChannels chan, byte* buf,
    int len) {  // Navigation Focus Request
    HU::NavigationFocusRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("Navigation Focus Request");
    else
        logd("Navigation Focus Request: %d", request.focus_type());

    HU::NavigationFocusResponse response;
    response.set_focus_type(2);  // Gained / Gained Transient ?
    return sendEncodedMessage(
        0, chan, HU_PROTOCOL_MESSAGE::NavigationFocusResponse, response);
}

int HUServer::handle_ShutdownRequest(ServiceChannels chan, byte* buf,
                                        int len) {  // Byebye Request

    HU::ShutdownRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("Byebye Request");
    else if (request.reason() == 1)
        logd("Byebye Request reason: 1 AA Exit Car Mode");
    else
        loge("Byebye Request reason: %d", request.reason());

    HU::ShutdownResponse response;
    sendEncodedMessage(0, chan, HU_PROTOCOL_MESSAGE::ShutdownResponse,
                            response);

    ms_sleep(100);  // Wait a bit for response

    stop();

    return (-1);
}

int HUServer::handle_VoiceSessionRequest(
    ServiceChannels chan, byte* buf,
    int len) {  // sr:  00000000 00 11 08 01      Microphone voice search usage
    // sr:  00000000 00 11 08 02

    HU::VoiceSessionRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("Voice Session Notification");
    else if (request.voice_status() ==
             HU::VoiceSessionRequest::VOICE_STATUS_START)
        logd("Voice Session Notification: 1 START");
    else if (request.voice_status() ==
             HU::VoiceSessionRequest::VOICE_STATUS_STOP)
        logd("Voice Session Notification: 2 STOP");
    else
        loge("Voice Session Notification: %d", request.voice_status());
    return (0);
}

int HUServer::handle_AudioFocusRequest(
    ServiceChannels chan, byte* buf,
    int len) {  // Navigation Focus Request
    HU::AudioFocusRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("AudioFocusRequest Focus Request");
    else
        logd("AudioFocusRequest Focus Request %s: %d", getChannel(chan),
             request.focus_type());

    callbacks.AudioFocusRequest(chan, request);
    return 0;
}

int HUServer::handle_ChannelOpenRequest(ServiceChannels chan, byte* buf,
                                           int len) {  // Channel Open Request

    HU::ChannelOpenRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("Channel Open Request");
    else
        logd("Channel Open Request: %d  priority: %d", request.id(),
             request.priority());

    HU::ChannelOpenResponse response;
    response.set_status(HU::STATUS_OK);

    int ret = sendEncodedMessage(
        0, chan, HU_PROTOCOL_MESSAGE::ChannelOpenResponse, response);
    if (ret)  // If error, done with error
        return (ret);

    if (chan == SensorChannel) {  // If Sensor channel...
        ms_sleep(2);          // 20);

        HU::SensorEvent sensorEvent;
        sensorEvent.add_driving_status()->set_status(
            HU::SensorEvent::DrivingStatus::DRIVE_STATUS_UNRESTRICTED);
        return sendEncodedMessage(
            0, SensorChannel, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
    }
    return (ret);
}

int HUServer::handle_MediaSetupRequest(ServiceChannels chan, byte* buf, int len) {
    HU::MediaSetupRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("MediaSetupRequest");
    else
        logd("MediaSetupRequest: %d", request.type());

    HU::MediaSetupResponse response;
    response.set_media_status(HU::MediaSetupResponse::MEDIA_STATUS_2);
    response.set_max_unacked(1);
    response.add_configs(0);

    int ret = sendEncodedMessage(
        0, chan, HU_MEDIA_CHANNEL_MESSAGE::MediaSetupResponse, response);

    if (!ret) {
        callbacks.MediaSetupComplete(chan);
    }

    return (ret);
}

int HUServer::handle_VideoFocusRequest(ServiceChannels chan, byte* buf, int len) {
    HU::VideoFocusRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("VideoFocusRequest");
    else
        logd("VideoFocusRequest: %d", request.disp_index());

    callbacks.VideoFocusRequest(chan, request);

    return 0;
}

int HUServer::handle_MediaStartRequest(
    ServiceChannels chan, byte* buf,
    int len) {  // sr:  00000000 00 11 08 01      Microphone voice search usage
    // sr:  00000000 00 11 08 02

    HU::MediaStartRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("MediaStartRequest");
    else
        logd("MediaStartRequest: %d", request.session());

    channel_session_id[chan] = request.session();
    return callbacks.MediaStart(chan);
}

int HUServer::handle_MediaStopRequest(
    ServiceChannels chan, byte* buf,
    int len) {  // sr:  00000000 00 11 08 01      Microphone voice search usage
    // sr:  00000000 00 11 08 02

    HU::MediaStopRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("MediaStopRequest");
    else
        logd("MediaStopRequest");

    channel_session_id[chan] = 0;
    return callbacks.MediaStop(chan);
}

int HUServer::handle_SensorStartRequest(
    ServiceChannels chan, byte* buf,
    int len) {  // Navigation Focus Request
    HU::SensorStartRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("SensorStartRequest Focus Request");
    else
        logd("SensorStartRequest Focus Request: %d", request.type());

    HU::SensorStartResponse response;
    response.set_status(HU::STATUS_OK);

    return sendEncodedMessage(
        0, chan, HU_SENSOR_CHANNEL_MESSAGE::SensorStartResponse, response);
}

int HUServer::handle_BindingRequest(ServiceChannels chan, byte* buf,
                                       int len) {  // Navigation Focus Request
    HU::BindingRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("BindingRequest Focus Request");
    else
        logd("BindingRequest Focus Request: %d", request.scan_codes_size());

    HU::BindingResponse response;
    response.set_status(HU::STATUS_OK);

    return sendEncodedMessage(
        0, chan, HU_INPUT_CHANNEL_MESSAGE::BindingResponse, response);
}

int HUServer::handle_MediaAck(ServiceChannels chan, byte* buf, int len) {
    HU::MediaAck request;
    if (!request.ParseFromArray(buf, len))
        loge("MediaAck");
    else
        logd("MediaAck");
    return (0);
}

int HUServer::handle_MicRequest(ServiceChannels chan, byte* buf, int len) {
    HU::MicRequest request;
    if (!request.ParseFromArray(buf, len))
        loge("MicRequest");
    else
        logd("MicRequest");

    if (!request.open()) {
        logd("Mic Start/Stop Request: 0 STOP");
        return callbacks.MediaStop(chan);
    } else {
        logd("Mic Start/Stop Request: 1 START");
        return callbacks.MediaStart(chan);
    }
    return (0);
}

int HUServer::handle_MediaDataWithTimestamp(ServiceChannels chan, byte* buf, int len) {
    uint64_t timestamp = be64toh(*((uint64_t*)buf));
    logd("Media timestamp %s %llu", getChannel(chan), timestamp);

    int ret = callbacks.MediaPacket(chan, timestamp, &buf[8], len - 8);
    if (ret < 0) {
        return ret;
    }

    HU::MediaAck mediaAck;
    mediaAck.set_session(channel_session_id[chan]);
    mediaAck.set_value(1);

    return sendEncodedMessage(0, chan, HU_MEDIA_CHANNEL_MESSAGE::MediaAck,
                                   mediaAck);
}

int HUServer::handle_MediaData(ServiceChannels chan, byte* buf, int len) {
    int ret = callbacks.MediaPacket(chan, 0, buf, len);
    if (ret < 0) {
        return ret;
    }

    HU::MediaAck mediaAck;
    mediaAck.set_session(channel_session_id[chan]);
    mediaAck.set_value(1);

    return sendEncodedMessage(0, chan, HU_MEDIA_CHANNEL_MESSAGE::MediaAck,
                                   mediaAck);
}

int HUServer::handle_PhoneStatus(ServiceChannels chan, byte* buf, int len) {
    HU::PhoneStatus request;
    if (!request.ParseFromArray(buf, len)) {
        loge("PhoneStatus Focus Request");
        return -1;
    } else {
        logd("PhoneStatus Focus Request");
    }
    callbacks.HandlePhoneStatus(*this, request);
    return 0;
}

int HUServer::handle_GenericNotificationResponse(ServiceChannels chan, byte* buf,
                                                    int len) {
    HU::GenericNotificationResponse request;
    if (!request.ParseFromArray(buf, len)) {
        loge("GenericNotificationResponse Focus Request");
        return -1;
    } else {
        logd("GenericNotificationResponse Focus Request");
    }
    callbacks.HandleGenericNotificationResponse(*this, request);
    return 0;
}

int HUServer::handle_StartGenericNotifications(ServiceChannels chan, byte* buf,
                                                  int len) {
    HU::StartGenericNotifications request;
    if (!request.ParseFromArray(buf, len)) {
        loge("StartGenericNotifications Focus Request");
        return -1;
    } else {
        logd("StartGenericNotifications Focus Request");
    }
    callbacks.ShowingGenericNotifications(*this, true);
    return 0;
}

int HUServer::handle_StopGenericNotifications(ServiceChannels chan, byte* buf, int len) {
    HU::StopGenericNotifications request;
    if (!request.ParseFromArray(buf, len)) {
        loge("StopGenericNotifications Focus Request");
        return -1;
    } else {
        logd("StopGenericNotifications Focus Request");
    }
    callbacks.ShowingGenericNotifications(*this, false);
    return 0;
}

int HUServer::handle_BluetoothPairingRequest(ServiceChannels chan, byte* buf, int len) {
    HU::BluetoothPairingRequest request;
    if (!request.ParseFromArray(buf, len)) {
        loge("BluetoothPairingRequest Focus Request");
        return -1;
    } else {
        logd("BluetoothPairingRequest Focus Request");
    }
    printf("BluetoothPairingRequest: %s\n", request.DebugString().c_str());

    HU::BluetoothPairingResponse response;
    response.set_already_paired(true);
    response.set_status(HU::BluetoothPairingResponse::PAIRING_STATUS_1);

    return sendEncodedMessage(
        0, chan, HU_BLUETOOTH_CHANNEL_MESSAGE::BluetoothPairingResponse,
        response);
}

int HUServer::handle_BluetoothAuthData(ServiceChannels chan, byte* buf, int len) {
    HU::BluetoothAuthData request;
    if (!request.ParseFromArray(buf, len)) {
        loge("BluetoothAuthData Focus Request");
        return -1;
    } else {
        logd("BluetoothAuthData Focus Request");
    }
    printf("BluetoothAuthData: %s\n", request.DebugString().c_str());
    return 0;
}

int HUServer::handle_NaviStatus(ServiceChannels chan, byte* buf, int len) {
    HU::NAVMessagesStatus request;
    if (!request.ParseFromArray(buf, len)) {
        logv("NaviStatus Request");
        logv(request.DebugString().c_str());
        return -1;
    } else {
        logv("NaviStatus Request");
    }
    callbacks.HandleNaviStatus(*this, request);
    return 0;
}

int HUServer::handle_NaviTurn(ServiceChannels chan, byte* buf, int len) {
    HU::NAVTurnMessage request;
    if (!request.ParseFromArray(buf, len)) {
        logv("NaviTurn Request");
        logv(request.DebugString().c_str());
        return -1;
    } else {
        logv("NaviTurn Request");
    }
    callbacks.HandleNaviTurn(*this, request);
    return 0;
}

int HUServer::handle_NaviTurnDistance(ServiceChannels chan, byte* buf, int len) {
    HU::NAVDistanceMessage request;
    if (!request.ParseFromArray(buf, len)) {
        logv("NaviTurnDistance Request");
        logv(request.DebugString().c_str());
        return -1;
    } else {
        logv("NaviTurnDistance Request");
    }
    callbacks.HandleNaviTurnDistance(*this, request);
    return 0;
}

int HUServer::processMessage(ServiceChannels chan, uint16_t msg_type, byte* buf,
                               int len) {
    if (ena_log_verbo)
        logd("iaap_msg_process msg_type: %d  len: %d  buf: %p", msg_type, len,
             buf);

    int filter_ret =
        callbacks.MessageFilter(*this, iaap_state, chan, msg_type, buf, len);
    if (filter_ret < 0) {
        return filter_ret;
    } else if (filter_ret > 0) {
        return 0;  // handled
    }

    if (iaap_state == hu_STATE_STARTIN) {
        switch ((HU_INIT_MESSAGE)msg_type) {
            case HU_INIT_MESSAGE::VersionResponse:
                return handle_VersionResponse(chan, buf, len);
            case HU_INIT_MESSAGE::SSLHandshake:
                return handleSSLHandshake(buf, len);
            default:
                logw("Unknown msg_type: %d", msg_type);
                return (0);
        }
    } else {
        const bool isControlMessage = msg_type < 0x8000;

        if (isControlMessage) {
            switch ((HU_PROTOCOL_MESSAGE)msg_type) {
                case HU_PROTOCOL_MESSAGE::MediaDataWithTimestamp:
                    return handle_MediaDataWithTimestamp(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::MediaData:
                    return handle_MediaData(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::ServiceDiscoveryRequest:
                    return handle_ServiceDiscoveryRequest(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::ChannelOpenRequest:
                    return handle_ChannelOpenRequest(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::PingRequest:
                    return handle_PingRequest(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::NavigationFocusRequest:
                    return handle_NavigationFocusRequest(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::ShutdownRequest:
                    return handle_ShutdownRequest(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::VoiceSessionRequest:
                    return handle_VoiceSessionRequest(chan, buf, len);
                case HU_PROTOCOL_MESSAGE::AudioFocusRequest:
                    return handle_AudioFocusRequest(chan, buf, len);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        } else if (chan == SensorChannel) {
            switch ((HU_SENSOR_CHANNEL_MESSAGE)msg_type) {
                case HU_SENSOR_CHANNEL_MESSAGE::SensorStartRequest:
                    return handle_SensorStartRequest(chan, buf, len);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        } else if (chan == TouchChannel) {
            switch ((HU_INPUT_CHANNEL_MESSAGE)msg_type) {
                case HU_INPUT_CHANNEL_MESSAGE::BindingRequest:
                    return handle_BindingRequest(chan, buf, len);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        } else if (chan == BluetoothChannel) {
            switch ((HU_BLUETOOTH_CHANNEL_MESSAGE)msg_type) {
                case HU_BLUETOOTH_CHANNEL_MESSAGE::BluetoothPairingRequest:
                    return handle_BluetoothPairingRequest(chan, buf, len);
                case HU_BLUETOOTH_CHANNEL_MESSAGE::BluetoothAuthData:
                    return handle_BluetoothAuthData(chan, buf, len);
                default:
                    logw("BLUETOOTH CHANNEL MESSAGE = chan %d - msg_type: %d",
                         chan, msg_type);
                    return (0);
            }
        } else if (chan == PhoneStatusChannel) {
            switch ((HU_PHONE_STATUS_CHANNEL_MESSAGE)msg_type) {
                case HU_PHONE_STATUS_CHANNEL_MESSAGE::PhoneStatus:
                    return handle_PhoneStatus(chan, buf, len);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        } else if (chan == NotificationChannel) {
            switch ((HU_GENERIC_NOTIFICATIONS_CHANNEL_MESSAGE)msg_type) {
                case HU_GENERIC_NOTIFICATIONS_CHANNEL_MESSAGE::
                    StartGenericNotifications:
                    return handle_StartGenericNotifications(chan, buf, len);
                case HU_GENERIC_NOTIFICATIONS_CHANNEL_MESSAGE::
                    StopGenericNotifications:
                    return handle_StopGenericNotifications(chan, buf, len);
                case HU_GENERIC_NOTIFICATIONS_CHANNEL_MESSAGE::
                    GenericNotificationResponse:
                    return handle_GenericNotificationResponse(chan, buf,
                                                                 len);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        } else if (chan == MediaAudioChannel || chan == Audio1Channel ||
                   chan == Audio2Channel || chan == VideoChannel ||
                   chan == MicrophoneChannel) {
            switch ((HU_MEDIA_CHANNEL_MESSAGE)msg_type) {
                case HU_MEDIA_CHANNEL_MESSAGE::MediaSetupRequest:
                    return handle_MediaSetupRequest(chan, buf, len);
                case HU_MEDIA_CHANNEL_MESSAGE::MediaStartRequest:
                    return handle_MediaStartRequest(chan, buf, len);
                case HU_MEDIA_CHANNEL_MESSAGE::MediaStopRequest:
                    return handle_MediaStopRequest(chan, buf, len);
                case HU_MEDIA_CHANNEL_MESSAGE::MediaAck:
                    return handle_MediaAck(chan, buf, len);
                case HU_MEDIA_CHANNEL_MESSAGE::MicRequest:
                    return handle_MicRequest(chan, buf, len);
                case HU_MEDIA_CHANNEL_MESSAGE::VideoFocusRequest:
                    return handle_VideoFocusRequest(chan, buf, len);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        } else if (chan == NavigationChannel) {
            logv("AA_CH_NAVI");
            logv("AA_CH_NAVI msg_type: %04x  len: %d  buf: %p", msg_type, len,
                 buf);
            hex_dump("AA_CH_NAVI", 80, buf, len);
            switch ((HU_NAVI_CHANNEL_MESSAGE)msg_type) {
                case HU_NAVI_CHANNEL_MESSAGE::Status:
                    logv("AA_CH_NAVI: HU_NAVI_CHANNEL_MESSAGE::Status");
                    handle_NaviStatus(chan, buf, len);
                    return (0);
                case HU_NAVI_CHANNEL_MESSAGE::Turn:
                    logv("AA_CH_NAVI: HU_NAVI_CHANNEL_MESSAGE::Turn");
                    handle_NaviTurn(chan, buf, len);
                    return (0);
                case HU_NAVI_CHANNEL_MESSAGE::TurnDistance:
                    logv("AA_CH_NAVI: HU_NAVI_CHANNEL_MESSAGE::TurnDistance");
                    handle_NaviTurnDistance(chan, buf, len);
                    return (0);
                default:
                    logw("Unknown msg_type: %d", msg_type);
                    return (0);
            }
        }
    }

    logw("Unknown chan: %d", chan);
    return (0);
}

int HUServer::queueCommand(
    IHUAnyThreadInterface::HUThreadCommand&& command) {
    IHUAnyThreadInterface::HUThreadCommand* ptr =
        new IHUAnyThreadInterface::HUThreadCommand(command);
    int ret = write(command_write_fd, &ptr, sizeof(ptr));
    if (ret < 0) {
        delete ptr;
        loge("hu_queue_command error %d", ret);
    }
    return ret;
}

int HUServer::shutdown() {
    if (hu_thread.joinable()) {
        int ret = queueCommand([this](IHUConnectionThreadInterface& s) {
            if (iaap_state == hu_STATE_STARTED) {
                logd("Sending ShutdownRequest");
                HU::ShutdownRequest byebye;
                byebye.set_reason(HU::ShutdownRequest::REASON_QUIT);
                s.sendEncodedMessage(
                    0, ControlChannel, HU_PROTOCOL_MESSAGE::ShutdownRequest, byebye);
                ms_sleep(500);
            }
            s.stop();
        });

        if (ret < 0) {
            loge("write end command error %d", ret);
        }
        hu_thread.join();
    }

    if (command_write_fd >= 0) close(command_write_fd);
    command_write_fd = -1;

    if (command_read_fd >= 0) close(command_read_fd);
    command_read_fd = -1;

    // Send Byebye
    iaap_state = hu_STATE_STOPPIN;
    logd("  SET: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));

    int ret = stopTransport();  // Stop Transport/USBACC/OAP
    iaap_state = hu_STATE_STOPPED;
    logd("  SET: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));

    return (ret);
}

int HUServer::stop() {  // Sends Byebye, then stops Transport/USBACC/OAP
    // assumes HU thread
    if (iaap_state == hu_STATE_STARTIN) {
        // hu_thread not ready yet
        return shutdown();
    }
    // Continue only if started or starting...
    if (iaap_state != hu_STATE_STARTED) return (0);

    HU::ShutdownRequest shutdownReq;
    shutdownReq.set_reason(HU::ShutdownRequest::REASON_QUIT);
    sendEncodedMessage(0, ControlChannel, HU_PROTOCOL_MESSAGE::ShutdownRequest,
                            shutdownReq);

    hu_thread_quit_flag = true;
    callbacks.DisconnectionOrError();

    return (0);
}

IHUAnyThreadInterface::HUThreadCommand* HUServer::popCommand() {
    IHUAnyThreadInterface::HUThreadCommand* ptr = nullptr;
    int ret = read(command_read_fd, &ptr, sizeof(ptr));
    if (ret < 0) {
        loge("hu_pop_command error %d", ret);
    } else if (ret == sizeof(ptr)) {
        return ptr;
    }
    return nullptr;
}

void HUServer::mainThread() {
    pthread_setname_np(pthread_self(), "hu_thread_main");

    int transportFD = transport->GetReadFD();
    int errorfd = transport->GetErrorFD();
    while (!hu_thread_quit_flag) {
        fd_set sock_set;
        FD_ZERO(&sock_set);
        FD_SET(command_read_fd, &sock_set);
        FD_SET(transportFD, &sock_set);
        int maxfd = std::max(command_read_fd, transportFD);
        if (errorfd >= 0) {
            maxfd = std::max(maxfd, errorfd);
            FD_SET(errorfd, &sock_set);
        }

        int ret = select(maxfd + 1, &sock_set, NULL, NULL, NULL);
        if (ret <= 0) {
            loge("Select failed %d", ret);
            return;
        }
        if (errorfd >= 0 && FD_ISSET(errorfd, &sock_set)) {
            logd("Got errorfd");
            hu_thread_quit_flag = true;
            callbacks.DisconnectionOrError();
        } else {
            if (FD_ISSET(command_read_fd, &sock_set)) {
                logd("Got command_read_fd");
                IHUAnyThreadInterface::HUThreadCommand* ptr = nullptr;
                if (ptr = popCommand()) {
                    logd("Running %p", ptr);
                    (*ptr)(*this);
                    delete ptr;
                }
            }
            if (FD_ISSET(transportFD, &sock_set)) {
                // data ready
                logd("Got transportFD");
                ret = processReceived(iaap_tra_recv_tmo);
                if (ret < 0) {
                    loge("hu_aap_recv_process failed %d", ret);
                    stop();
                }
            }
        }
    }
    logd("hu_thread_main exit");
    iaap_state = hu_STATE_STOPPED;
}

static_assert(PIPE_BUF >= sizeof(IHUAnyThreadInterface::HUThreadCommand*),
              "PIPE_BUF is tool small for a pointer?");

int HUServer::start() {  // Starts Transport/USBACC/OAP, then AA
    // protocol w/ VersReq(1), SSL handshake,
    // Auth Complete

    if (iaap_state == hu_STATE_STARTED || iaap_state == hu_STATE_STARTIN) {
        loge("CHECK: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        return (0);
    }

    pthread_setname_np(pthread_self(), "aa_main_thread");

    iaap_state = hu_STATE_STARTIN;
    logd("  SET: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));

    int ret =
        startTransport();  // Start Transport/USBACC/OAP
    if (ret) {
        iaap_state = hu_STATE_STOPPED;
        logd("  SET: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        return (ret);  // Done if error
    }

    byte vr_buf[] = {0, 1, 0, 1};  // Version Request
    ret = sendUnencodedBlob(0, ControlChannel, HU_INIT_MESSAGE::VersionRequest,
                                 vr_buf, sizeof(vr_buf), 2000);
    if (ret < 0) {
        loge("Version request send ret: %d", ret);
        return (-1);
    }

    while (iaap_state == hu_STATE_STARTIN) {
        ret = processReceived(2000);
        if (ret < 0) {
            shutdown();
            return (ret);
        }
    }

    int pipefd[2];
    ret = pipe2(pipefd, O_DIRECT);
    if (ret < 0) {
        loge("pipe2 failed ret: %d %i", ret, errno);
        shutdown();
        return (-1);
    }

    logd("Starting HU thread");
    command_read_fd = pipefd[0];
    command_write_fd = pipefd[1];
    hu_thread_quit_flag = false;
    hu_thread = std::thread([this] { this->mainThread(); });

    return (0);
}

int HUServer::processReceived(int tmo) {  //
    // Terminate unless started or starting (we need to process when
    // starting)
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN) {
        loge("CHECK: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));
        return (-1);
    }

    int ret = 0;
    errno = 0;
    int min_size_hdr = 4;
    int have_len = 0;  // Length remaining to process for all sub-packets plus
                       // 4/8 byte headers

    bool has_last = false;
    bool has_first = false;
    int chan = -1;
    while (!has_last) {  // While length remaining to process,... Process Rx
                         // packet:
        have_len = receiveTransportPacket(enc_buf, min_size_hdr, tmo);
        if (have_len == 0 && !has_first) {
            return 0;
        }

        if (have_len < min_size_hdr) {  // If we don't have a full 6 byte header
                                        // at least...
            loge("Recv have_len: %d", have_len);
            return (-1);
        }

        if (ena_log_verbo) {
            logd("Recv while (have_len > 0): %d", have_len);
            hex_dump("LR: ", 16, enc_buf, have_len);
        }
        int cur_chan = (int)enc_buf[0];  // Channel
        if (cur_chan != chan && chan >= 0) {
            logd(
                "Interleaved channels, preserving incomplete packet for chan "
                "%s",
                getChannel((ServiceChannels)chan));
            channel_assembly_buffers[chan] = temp_assembly_buffer;
            temp_assembly_buffer = NULL;
            has_first = has_last = false;
            // return (-1);
        }
        chan = cur_chan;

        int flags = enc_buf[1];  // Flags
        int frame_len = be16toh(*((uint16_t*)&enc_buf[2]));

        logd("Frame flags %i len %i", flags, frame_len);

        if (frame_len > MAX_FRAME_PAYLOAD_SIZE) {
            loge("Too big");
            return (-1);
        }

        int header_size = 4;
        bool has_total_size_header = false;
        if ((flags & HU_FRAME_FIRST_FRAME) & !(flags & HU_FRAME_LAST_FRAME)) {
            // if first but not last, next 4 is total size
            has_total_size_header = true;
            header_size += 4;
        }

        int remaining_bytes_in_frame = (frame_len + header_size) - have_len;
        while (remaining_bytes_in_frame > 0) {
            logd("Getting more %i", remaining_bytes_in_frame);
            int got_bytes =
                receiveTransportPacket(&enc_buf[have_len], remaining_bytes_in_frame,
                                tmo);  // Get Rx packet from Transport
            if (got_bytes <
                0) {  // If we don't have a full 6 byte header at least...
                loge("Recv got_bytes: %d", got_bytes);
                return (-1);
            }
            have_len += got_bytes;
            remaining_bytes_in_frame -= got_bytes;
        }

        auto buffer = channel_assembly_buffers.find(chan);
        if (buffer !=
            channel_assembly_buffers
                .end())  // Have old buffer with incomplete data for channel
        {
            logd("Found existing buffer for chan %s", getChannel((ServiceChannels)chan));
            temp_assembly_buffer = buffer->second;
        } else if (temp_assembly_buffer ==
                   NULL)  // Old buffer had incomplete data and was preserved,
                          // need
        // to create new one
        {
            logd("Created new buffer for chan %s", getChannel((ServiceChannels)chan));
            temp_assembly_buffer = new std::vector<uint8_t>();
        }

        if (flags & HU_FRAME_FIRST_FRAME) {
            temp_assembly_buffer->clear();  // It's the first frame and old data
                                            // may still be there, so clear
        } else if (!has_first &&
                   temp_assembly_buffer->size() ==
                       0)  // No first frame yet and buffer is empty
        {
            loge(
                "No HU_FRAME_FIRST_FRAME, and no incomplete buffer for chan %s",
                getChannel((ServiceChannels)chan));
            return (-1);
        }

        has_first = true;
        has_last = (flags & HU_FRAME_LAST_FRAME) != 0;

        if (has_total_size_header) {
            uint32_t total_size = be32toh(*((uint32_t*)&enc_buf[4]));
            logd("First only, total len %u", total_size);
            temp_assembly_buffer->reserve(total_size);
        } else {
            temp_assembly_buffer->reserve(frame_len);
        }

        if (flags & HU_FRAME_ENCRYPTED) {
            size_t cur_vec = temp_assembly_buffer->size();
            temp_assembly_buffer->resize(cur_vec + frame_len);  // just incase

            int bytes_written =
                BIO_write(m_sslWriteBio, &enc_buf[header_size],
                          frame_len);  // Write encrypted to SSL input BIO
            if (bytes_written <= 0) {
                loge("BIO_write() bytes_written: %d", bytes_written);
                return (-1);
            }
            if (bytes_written != frame_len)
                loge("BIO_write() len: %d  bytes_written: %d  chan: %d %s",
                     frame_len, bytes_written, chan, getChannel((ServiceChannels)chan));
            else if (ena_log_verbo)
                logd("BIO_write() len: %d  bytes_written: %d  chan: %d %s",
                     frame_len, bytes_written, chan, getChannel((ServiceChannels)chan));

            int bytes_read =
                SSL_read(m_ssl, &(*temp_assembly_buffer)[cur_vec],
                         frame_len);  // Read decrypted to decrypted rx buf
            if (bytes_read <= 0 || bytes_read > frame_len) {
                loge("SSL_read() bytes_read: %d  errno: %d", bytes_read, errno);
                logSSLReturnCode(bytes_read);
                return (-1);  // Fatal so return error and de-initialize; Should
                              // we be able to recover, if Transport data got
                              // corrupted ??
            }
            if (ena_log_verbo) logd("SSL_read() bytes_read: %d", bytes_read);

            temp_assembly_buffer->resize(cur_vec + bytes_read);
        } else {
            temp_assembly_buffer->insert(temp_assembly_buffer->end(),
                                         &enc_buf[header_size],
                                         &enc_buf[frame_len + header_size]);
        }
    }

    const int buf_len = temp_assembly_buffer->size();
    if (buf_len >= 2) {
        uint16_t msg_type =
            be16toh(*reinterpret_cast<uint16_t*>(temp_assembly_buffer->data()));

        ret = processMessage(
            (ServiceChannels)chan, msg_type, &(*temp_assembly_buffer)[2],
            buf_len - 2);  // Decrypt & Process 1 received encrypted message
        if (ret < 0 && iaap_state != hu_STATE_STOPPED) {  // If error...
            loge("Error iaap_msg_process() ret: %d  ", ret);
            return (ret);
        }
    }

    return (ret);  // Return value from the last iaap_recv_dec_process() call;
                   // should be 0
}

std::map<std::string, int> HUServer::getResolutions() {
    std::map<std::string, int> ret;

    const google::protobuf::EnumDescriptor* resolutions = HU::
        ChannelDescriptor_OutputStreamChannel_VideoConfig_VIDEO_RESOLUTION_descriptor();
    for (int i = 0; i < resolutions->value_count(); i++) {
        ret[resolutions->value(i)->name()] = resolutions->value(i)->index();
    }

    return ret;
}

std::map<std::string, int> HUServer::getFPS() {
    std::map<std::string, int> ret;

    const google::protobuf::EnumDescriptor* fpss = HU::
        ChannelDescriptor_OutputStreamChannel_VideoConfig_VIDEO_FPS_descriptor();
    for (int i = 0; i < fpss->value_count(); i++) {
        ret[fpss->value(i)->name()] = fpss->value(i)->index();
    }

    return ret;
}
