#pragma once

#include <glib.h>
#include <stdio.h>
#include <asoundlib.h>
#include <thread>

#include "hu_uti.h"
#include "hu_aap.h"

class AudioOutput
{
    snd_pcm_t* aud_handle = nullptr;
    snd_pcm_t* au1_handle = nullptr;

    void MediaPacket(snd_pcm_t* pcm, const byte * buf, int len);

    bool halfVolume = false;
    std::vector<int16_t> audio_temp;
public:
    AudioOutput(const char* outDev = "default", bool halfVolume = false);
    ~AudioOutput();

    void MediaPacketAUD(uint64_t timestamp, const byte * buf, int len);
    void MediaPacketAU1(uint64_t timestamp, const byte * buf, int len);
};

class MicInput
{
    snd_pcm_t* mic_handle = nullptr;
    std::thread mic_readthread;
    int cancelPipeRead = -1, cancelPipeWrite = -1;

    snd_pcm_sframes_t read_mic_cancelable(void *buffer, snd_pcm_uframes_t size, bool* canceled);
    void MicThreadMain(IHUAnyThreadInterface* threadInterface);
public:
    MicInput();
    ~MicInput();

    void Start(IHUAnyThreadInterface* threadInterface);
    void Stop();
};
