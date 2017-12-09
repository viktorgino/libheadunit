#include "callbacks.h"
#include "outputs.h"
#include "glib_utils.h"
#include "audio.h"
#include "main.h"
#include "bt/mzd_bluetooth.h"

#include "json/json.hpp"
using json = nlohmann::json;

MazdaEventCallbacks::MazdaEventCallbacks(DBus::Connection& serviceBus, DBus::Connection& hmiBus)
    : micInput("mic")
    , serviceBus(serviceBus)
    , hmiBus(hmiBus)
    , connected(false)
    , videoFocus(false)
    , audioFocus(AudioManagerClient::FocusType::NONE)
{
    //no need to create/destroy this
    audioOutput.reset(new AudioOutput("entertainmentMl"));
    audioMgrClient.reset(new AudioManagerClient(*this, serviceBus));
    videoMgrClient.reset(new VideoManagerClient(*this, hmiBus));
}

MazdaEventCallbacks::~MazdaEventCallbacks() {

}

int MazdaEventCallbacks::MediaPacket(int chan, uint64_t timestamp, const byte *buf, int len) {

    if (chan == AA_CH_VID && videoOutput) {
        videoOutput->MediaPacket(timestamp, buf, len);
    } else if (chan == AA_CH_AUD && audioOutput) {
        audioOutput->MediaPacketAUD(timestamp, buf, len);
    } else if (chan == AA_CH_AU1 && audioOutput) {
        audioOutput->MediaPacketAU1(timestamp, buf, len);
    }
    return 0;
}

int MazdaEventCallbacks::MediaStart(int chan) {
    if (chan == AA_CH_MIC) {
        printf("SHAI1 : Mic Started\n");
        micInput.Start(g_hu);
    }
    return 0;
}

int MazdaEventCallbacks::MediaStop(int chan) {
    if (chan == AA_CH_MIC) {
        micInput.Stop();
        printf("SHAI1 : Mic Stopped\n");
    }
    return 0;
}

void MazdaEventCallbacks::MediaSetupComplete(int chan) {
    if (chan == AA_CH_VID) {
        run_on_main_thread([this](){
            //Ask for video focus on connection
            videoMgrClient->requestVideoFocus(VIDEO_FOCUS_REQUESTOR::HEADUNIT);
            return false;
        });
    }
}

void MazdaEventCallbacks::DisconnectionOrError() {
    printf("DisconnectionOrError\n");
    g_main_loop_quit(gst_app.loop);
}

void MazdaEventCallbacks::CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel &streamChannel) {
#if ASPECT_RATIO_FIX
    if (chan == AA_CH_VID) {
        auto videoConfig = streamChannel.mutable_video_configs(0);
        videoConfig->set_margin_height(30);
    }
#endif
}

void MazdaEventCallbacks::releaseAudioFocus()  {
    run_on_main_thread([this](){
        audioMgrClient->audioMgrReleaseAudioFocus();
        return false;
    });
}
void MazdaEventCallbacks::AudioFocusRequest(int chan, const HU::AudioFocusRequest &request)  {

    run_on_main_thread([this, request](){
        //The chan passed here is always AA_CH_CTR but internally we pass the channel AA means
        if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_RELEASE) {
            audioMgrClient->audioMgrReleaseAudioFocus();
        } else if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_GAIN_TRANSIENT) {
            audioMgrClient->audioMgrRequestAudioFocus(AudioManagerClient::FocusType::TRANSIENT); //assume navigation
        } else if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_GAIN || request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_UNKNOWN) {
            audioMgrClient->audioMgrRequestAudioFocus(AudioManagerClient::FocusType::PERMANENT); //assume media
        }

        return false;
    });
}

void MazdaEventCallbacks::VideoFocusRequest(int chan, const HU::VideoFocusRequest &request) {
    run_on_main_thread([this, request](){
        if (request.mode() == HU::VIDEO_FOCUS_MODE::VIDEO_FOCUS_MODE_FOCUSED) {
            videoMgrClient->requestVideoFocus(VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO);
        } else {
            videoMgrClient->releaseVideoFocus(VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO);
        }
        return false;
    });
}

std::string MazdaEventCallbacks::GetCarBluetoothAddress()
{
    return get_bluetooth_mac_address();
}

void MazdaEventCallbacks::takeVideoFocus() {
    run_on_main_thread([this](){
        videoMgrClient->requestVideoFocus(VIDEO_FOCUS_REQUESTOR::HEADUNIT);
        return false;
    });
}

void MazdaEventCallbacks::releaseVideoFocus() {
    run_on_main_thread([this](){
        videoMgrClient->releaseVideoFocus(VIDEO_FOCUS_REQUESTOR::HEADUNIT);
        return false;
    });
}

void MazdaEventCallbacks::VideoFocusHappened(bool hasFocus, bool unrequested) {
    videoFocus = hasFocus;
    if ((bool)videoOutput != hasFocus) {
        videoOutput.reset(hasFocus ? new VideoOutput(this) : nullptr);
    }
    g_hu->hu_queue_command([hasFocus, unrequested] (IHUConnectionThreadInterface & s) {
        HU::VideoFocus videoFocusGained;
        videoFocusGained.set_mode(hasFocus ? HU::VIDEO_FOCUS_MODE_FOCUSED : HU::VIDEO_FOCUS_MODE_UNFOCUSED);
        videoFocusGained.set_unrequested(unrequested);
        s.hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
    });
}

void MazdaEventCallbacks::AudioFocusHappend(AudioManagerClient::FocusType type) {
    printf("AudioFocusHappend(%i)\n", int(type));
    audioFocus = type;
    HU::AudioFocusResponse response;
    switch(type) {
        case AudioManagerClient::FocusType::NONE:
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_LOSS);
            break;
        case AudioManagerClient::FocusType::PERMANENT:
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN);
            break;
        case AudioManagerClient::FocusType::TRANSIENT:
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN_TRANSIENT);
            break;
    }
    g_hu->hu_queue_command([response](IHUConnectionThreadInterface & s) {
        s.hu_aap_enc_send_message(0, AA_CH_CTR, HU_PROTOCOL_MESSAGE::AudioFocusResponse, response);
    });
    logd("Sent channel %i HU_PROTOCOL_MESSAGE::AudioFocusResponse %s\n", AA_CH_CTR,  HU::AudioFocusResponse::AUDIO_FOCUS_STATE_Name(response.focus_type()).c_str());
}

VideoManagerClient::VideoManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection& hmiBus)
        : DBus::ObjectProxy(hmiBus, "/com/jci/bucpsa", "com.jci.bucpsa"), guiClient(hmiBus), callbacks(callbacks)
{
    uint32_t currentDisplayMode;
    int32_t returnValue;
    // check if backup camera is not visible at the moment and get output only when not
    GetDisplayMode(currentDisplayMode, returnValue);
    allowedToGetFocus = !(bool)currentDisplayMode;
}

VideoManagerClient::~VideoManagerClient() {
    //We can't call release video focus since the callbacks object is being destroyed, but make sure we got to opera if no in backup cam
    if (allowedToGetFocus) {
        logd("Requesting video surface: JCI_OPERA_PRIMARY");
        guiClient.SetRequiredSurfacesByEnum({NativeGUICtrlClient::JCI_OPERA_PRIMARY}, true);
    }
}

void VideoManagerClient::requestVideoFocus(VIDEO_FOCUS_REQUESTOR requestor)
{
    if (!allowedToGetFocus) {
        // we can safely exit - backup camera will notice us when finish and we request focus back
        waitsForFocus = true;
        return;
    }
    waitsForFocus = false;
    bool unrequested = requestor != VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO;
    logd("Requestor %i requested video focus\n", requestor);

    auto handleRequest = [this, unrequested](){
        callbacks.VideoFocusHappened(true, unrequested);
        logd("Requesting video surface: TV_TOUCH_SURFACE");
        guiClient.SetRequiredSurfacesByEnum({NativeGUICtrlClient::TV_TOUCH_SURFACE}, true);
        return false;
    };
    if (requestor == VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA)
    {
        // need to wait for a second (maybe less but 100ms is too early) to make sure
        // the CMU has already changed the surface from backup camera to opera
        run_on_main_thread_delay(1000, handleRequest);
    }
    else
    {
        //otherwise don't pause
        handleRequest();
    }
}

void VideoManagerClient::releaseVideoFocus(VIDEO_FOCUS_REQUESTOR requestor)
{
    if (!callbacks.videoFocus) {
        return;
    }
    bool unrequested = requestor != VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO;
    logd("Requestor %i released video focus\n", requestor);
    callbacks.VideoFocusHappened(false, unrequested);
    if (requestor != VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA) {
        logd("Requesting video surface: JCI_OPERA_PRIMARY");
        guiClient.SetRequiredSurfacesByEnum({NativeGUICtrlClient::JCI_OPERA_PRIMARY}, true);
    }
}

void VideoManagerClient::DisplayMode(const uint32_t &currentDisplayMode)
{
    // currentDisplayMode != 0 means backup camera wants the screen
    allowedToGetFocus = !(bool)currentDisplayMode;
    if ((bool)currentDisplayMode) {
        waitsForFocus = callbacks.videoFocus;
        releaseVideoFocus(VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA);
    } else if (waitsForFocus) {
        requestVideoFocus(VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA);
    }
}

MazdaCommandServerCallbacks::MazdaCommandServerCallbacks()
{

}

bool MazdaCommandServerCallbacks::IsConnected() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->connected;
    }
    return false;
}

bool MazdaCommandServerCallbacks::HasAudioFocus() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->audioFocus != AudioManagerClient::FocusType::NONE;
    }
    return false;
}

bool MazdaCommandServerCallbacks::HasVideoFocus() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->videoFocus;
    }
    return false;
}

void MazdaCommandServerCallbacks::TakeVideoFocus()
{
    if (eventCallbacks && eventCallbacks->connected)
    {
        eventCallbacks->takeVideoFocus();
    }
}

std::string MazdaCommandServerCallbacks::GetLogPath() const
{
    return "/tmp/mnt/data/headunit.log";
}

void AudioManagerClient::aaRegisterStream()
{
    if(!aaStreamRegistered)
    {
        // so we dont accedentally try to register this twice
        aaStreamRegistered = true;
        try
        {
            // First open a new Stream
            json sessArgs = {
                { "busName", "com.jci.usbm_am_client" },
                { "objectPath", "/com/jci/usbm_am_client" },
                { "destination", "Cabin" }
            };
            std::string sessString = Request("openSession", sessArgs.dump());
            printf("openSession(%s)\n%s\n", sessArgs.dump().c_str(), sessString.c_str());
            aaSessionID = json::parse(sessString)["sessionId"];

            // Register the stream
            json regArgs = {
                { "sessionId", aaSessionID },
                { "streamName", aaStreamName },
                { "streamModeName", aaStreamName },
                { "focusType", "permanent" },
                { "streamType", "Media" }
            };
            std::string regString = Request("registerAudioStream", regArgs.dump());
            printf("registerAudioStream(%s)\n%s\n", regArgs.dump().c_str(), regString.c_str());
        }
        catch (const std::domain_error& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
        catch (const std::invalid_argument& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }

        // Stream is registered add it to the array
        streamToSessionIds[aaStreamName] = aaSessionID;
    }
}
void AudioManagerClient::populateStreamTable()
{
    streamToSessionIds.clear();
    json requestArgs = {
        { "svc", "SRCS" },
        { "pretty", false }
    };
    std::string resultString = Request("dumpState", requestArgs.dump());
    printf("dumpState(%s)\n%s\n", requestArgs.dump().c_str(), resultString.c_str());
    /*
         * An example resonse:
         *
        {
          "HMI": {

          },
          "APP": [
            "1.Media.Pandora.granted.NotPlaying",
            "2.Media.AM..NotPlaying"
          ]
        }
        */
    //Row format:
    //"%d.%s.%s.%s.%s", obj.sessionId, obj.stream.streamType, obj.stream.streamName, obj.focus, obj.stream.playing and "playing" or "NotPlaying")

    try
    {
        auto result = json::parse(resultString);
        for (auto& sessionRecord : result["APP"].get_ref<json::array_t&>())
        {
            std::string sessionStr = sessionRecord.get<std::string>();
            //Stream names have no spaces so it's safe to do this
            std::replace(sessionStr.begin(), sessionStr.end(), '.', ' ');
            std::istringstream sessionIStr(sessionStr);

            int sessionId;
            std::string streamName, streamType;

            if (!(sessionIStr >> sessionId >> streamType >> streamName))
            {
                logw("Can't parse line \"%s\"", sessionRecord.get<std::string>().c_str());
                continue;
            }

            printf("Found stream %s session id %i\n", streamName.c_str(), sessionId);
            streamToSessionIds[streamName] = sessionId;

            if(streamName == aaStreamName)
            {
                aaSessionID = sessionId;
            }
            else if(streamName == "USB")
            {
                usbSessionID = sessionId;
            }
            else if(streamName == "FM")
            {
                fmSessionID = sessionId;
            }
        }
        // Create and register stream (only if we need to)
        if (aaSessionID < 0)
        {
            logw("When using the USB stream we should never have to register since it should be already there");
            aaRegisterStream();
        }
    }
    catch (const std::domain_error& ex)
    {
        loge("Failed to parse state json: %s", ex.what());
        printf("%s\n", resultString.c_str());
    }
    catch (const std::invalid_argument& ex)
    {
        loge("Failed to parse state json: %s", ex.what());
        printf("%s\n", resultString.c_str());
    }
}

AudioManagerClient::AudioManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection &connection)
    : DBus::ObjectProxy(connection, "/com/xse/service/AudioManagement/AudioApplication", "com.xsembedded.service.AudioManagement")
    , callbacks(callbacks)
{
    populateStreamTable();
    if (aaSessionID < 0)
    {
        loge("Can't find USB stream. Audio will not work");
    }
}

AudioManagerClient::~AudioManagerClient()
{
    if (currentFocus != FocusType::NONE && previousSessionID >= 0)
    {
        json args = { { "sessionId", previousSessionID } };
        std::string result = Request("requestAudioFocus", args.dump());
        printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
    }
}

bool AudioManagerClient::canSwitchAudio() { return aaSessionID >= 0; }

void AudioManagerClient::audioMgrRequestAudioFocus(FocusType type)
{
    if (type == FocusType::NONE)
    {
        audioMgrReleaseAudioFocus();
        return;
    }

    printf("audioMgrRequestAudioFocus(%i)\n", int(type));
    if (currentFocus != FocusType::NONE)
    {
        //no need to do anything
        if (type != currentFocus)
        {
            currentFocus = type;
            callbacks.AudioFocusHappend(currentFocus);
        }
        return;
    }

    pendingFocus = type;
    if (requestPending && !releasePending)
    {
        //already asked
        return;
    }

    requestPending = true;
    waitingForFocusLostEvent = true;
    previousSessionID = -1;
    json args = { { "sessionId", aaSessionID } };
    std::string result = Request("requestAudioFocus", args.dump());
    printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
}

void AudioManagerClient::audioMgrReleaseAudioFocus()
{
    printf("audioMgrReleaseAudioFocus()\n");
    if (currentFocus == FocusType::NONE && !requestPending)
    {
        //nothing to do
        pendingFocus = FocusType::NONE;
        return;
    }
    bool hadFocus = currentFocus != FocusType::NONE;
    if ((hadFocus || requestPending) && !releasePending)
    {
        if (previousSessionID >= 0)
        {
            //We released the last one, give up audio focus for real
            json args = { { "sessionId", previousSessionID } };
            std::string result = Request("requestAudioFocus", args.dump());
            printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
            previousSessionID = -1;
            releasePending = true;
        }
        else
        {
            currentFocus = FocusType::NONE;
            callbacks.AudioFocusHappend(currentFocus);
        }
    }
}

void AudioManagerClient::Notify(const std::string &signalName, const std::string &payload)
{
    printf("AudioManagerClient::Notify signalName=%s payload=%s\n", signalName.c_str(), payload.c_str());
    if (signalName == "audioFocusChangeEvent")
    {
        try
        {
            auto result = json::parse(payload);
            std::string streamName = result["streamName"].get<std::string>();
            std::string newFocus = result["newFocus"].get<std::string>();

            auto findIt = streamToSessionIds.find(streamName);
            int eventSessionID = -1;
            if (findIt != streamToSessionIds.end())
            {
                eventSessionID = findIt->second;
                printf("Found audio sessionId %i for stream %s\n", eventSessionID, streamName.c_str());
            }
            else
            {
                loge("Can't find audio sessionId for stream %s\n", streamName.c_str());
            }

            if (eventSessionID >= 0)
            {
                if (waitingForFocusLostEvent && newFocus == "lost")
                {
                    previousSessionID = eventSessionID;
                    waitingForFocusLostEvent = false;
                }

                if (eventSessionID == aaSessionID)
                {
                    bool hasFocus = newFocus == "gained";
                    if (hasFocus)
                    {
                        requestPending = false;
                    }
                    else
                    {
                        releasePending = false;
                        pendingFocus = FocusType::NONE;
                    }

                    if (pendingFocus != currentFocus)
                    {
                        currentFocus = pendingFocus;
                        callbacks.AudioFocusHappend(currentFocus);
                    }
                    pendingFocus = FocusType::NONE;
                }
            }
        }
        catch (const std::domain_error& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
        catch (const std::invalid_argument& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
    }
}
