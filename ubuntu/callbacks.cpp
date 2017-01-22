#include "callbacks.h"
#include "outputs.h"
#include "glib_utils.h"

std::atomic<bool> GlobalState::connected(false);
std::atomic<bool> GlobalState::videoFocus(false);
std::atomic<bool> GlobalState::audioFocus(false);

DesktopEventCallbacks::DesktopEventCallbacks()  {

}

DesktopEventCallbacks::~DesktopEventCallbacks() {

}

int DesktopEventCallbacks::MediaPacket(int chan, uint64_t timestamp, const byte *buf, int len) {

    if (chan == AA_CH_VID && videoOutput) {
        videoOutput->MediaPacket(timestamp, buf, len);
    } else if (chan == AA_CH_AUD && audioOutput) {
        audioOutput->MediaPacketAUD(timestamp, buf, len);
    } else if (chan == AA_CH_AU1 && audioOutput) {
        audioOutput->MediaPacketAU1(timestamp, buf, len);
    }
    return 0;
}

int DesktopEventCallbacks::MediaStart(int chan) {
    if (chan == AA_CH_MIC) {
        printf("SHAI1 : Mic Started\n");
        micInput.Start(g_hu);
    }
    return 0;
}

int DesktopEventCallbacks::MediaStop(int chan) {
    if (chan == AA_CH_MIC) {
        micInput.Stop();
        printf("SHAI1 : Mic Stopped\n");
    }
    return 0;
}

void DesktopEventCallbacks::MediaSetupComplete(int chan) {
    if (chan == AA_CH_VID) {
        VideoFocusHappened(true, true);
    }
}

void DesktopEventCallbacks::DisconnectionOrError() {
    printf("DisconnectionOrError\n");
    g_main_loop_quit(gst_app.loop);
}

void DesktopEventCallbacks::CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel &streamChannel) {
#if ASPECT_RATIO_FIX
    if (chan == AA_CH_VID) {
        auto videoConfig = streamChannel.mutable_video_configs(0);
        videoConfig->set_margin_height(30);
    }
#endif
}

void DesktopEventCallbacks::AudioFocusRequest(int chan, const HU::AudioFocusRequest &request)  {
    run_on_main_thread([this, chan, request](){
        HU::AudioFocusResponse response;
        if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_RELEASE) {
            audioOutput.reset();
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_LOSS);
            GlobalState::audioFocus = false;
        } else {
            if (!audioOutput) {
                audioOutput.reset(new AudioOutput());
            }
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN);
            GlobalState::audioFocus = true;
        }

        g_hu->hu_queue_command([chan, response](IHUConnectionThreadInterface & s) {
            s.hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::AudioFocusResponse, response);
        });
        return false;
    });
}

void DesktopEventCallbacks::VideoFocusRequest(int chan, const HU::VideoFocusRequest &request) {
    VideoFocusHappened(request.mode() == HU::VIDEO_FOCUS_MODE_FOCUSED, false);
}

void DesktopEventCallbacks::VideoFocusHappened(bool hasFocus, bool unrequested) {
    run_on_main_thread([this, hasFocus, unrequested](){
        if ((bool)videoOutput != hasFocus) {
            videoOutput.reset(hasFocus ? new VideoOutput(this) : nullptr);
        }
        GlobalState::videoFocus = hasFocus;
        g_hu->hu_queue_command([hasFocus, unrequested](IHUConnectionThreadInterface & s) {
            HU::VideoFocus videoFocusGained;
            videoFocusGained.set_mode(hasFocus ? HU::VIDEO_FOCUS_MODE_FOCUSED : HU::VIDEO_FOCUS_MODE_UNFOCUSED);
            videoFocusGained.set_unrequested(unrequested);
            s.hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
        });
        return false;
    });
}

DesktopCommandServerCallbacks::DesktopCommandServerCallbacks(DesktopEventCallbacks &eventCallbacks)
    : eventCallbacks(eventCallbacks)
{

}

bool DesktopCommandServerCallbacks::IsConnected() const
{
    return GlobalState::connected;
}

bool DesktopCommandServerCallbacks::HasAudioFocus() const
{
    return GlobalState::audioFocus;
}

bool DesktopCommandServerCallbacks::HasVideoFocus() const
{
    return GlobalState::videoFocus;
}

void DesktopCommandServerCallbacks::TakeVideoFocus()
{
    eventCallbacks.VideoFocusHappened(true, true);
}
