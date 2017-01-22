#pragma once

#include <glib.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <dbus-c++/dbus.h>
#include <time.h>
#include <glib-unix.h>

#include "dbus/generated_cmu.h"

//This gets defined by SDL and breaks the protobuf headers
#undef Status

#include "hu_uti.h"
#include "hu_aap.h"

#ifndef ASPECT_RATIO_FIX
#define ASPECT_RATIO_FIX 1
#endif

struct gst_app_t;
class MazdaEventCallbacks;

class BUCPSAClient : public com::jci::bucpsa_proxy,
                     public DBus::ObjectProxy
{
    MazdaEventCallbacks* callbacks;
public:
    BUCPSAClient(DBus::Connection &connection, MazdaEventCallbacks* callbacks)
        : DBus::ObjectProxy(connection, "/com/jci/bucpsa", "com.jci.bucpsa"),
          callbacks(callbacks)
    {
    }

    virtual void CommandResponse(const uint32_t& cmdResponse) override {}
    virtual void DisplayMode(const uint32_t& currentDisplayMode) override;
    virtual void ReverseStatusChanged(const int32_t& reverseStatus) override {}
    virtual void PSMInstallStatusChanged(const uint8_t& psmInstalled) override {}
};


class NativeGUICtrlClient : public com::jci::nativeguictrl_proxy,
                     public DBus::ObjectProxy
{
public:
    NativeGUICtrlClient(DBus::Connection &connection)
        : DBus::ObjectProxy(connection, "/com/jci/nativeguictrl", "com.jci.nativeguictrl")
    {
    }

    enum SURFACES
    {
        NNG_NAVI_ID = 0,
        TV_TOUCH_SURFACE,
        NATGUI_SURFACE,
        LOOPLOGO_SURFACE,
        TRANLOGOEND_SURFACE,
        TRANLOGO_SURFACE,
        QUICKTRANLOGO_SURFACE,
        EXITLOGO_SURFACE,
        JCI_OPERA_PRIMARY,
        JCI_OPERA_SECONDARY,
        lvdsSurface,
        SCREENREP_IVI_NAME,
        NNG_NAVI_MAP1,
        NNG_NAVI_MAP2,
        NNG_NAVI_HMI,
        NNG_NAVI_TWN,
    };

    void SetRequiredSurfacesByEnum(const std::vector<SURFACES>& surfaces, bool fadeOpera)
    {
        std::ostringstream idString;
        for (size_t i = 0; i < surfaces.size(); i++)
        {
            if (i > 0)
                idString << ",";
            idString << surfaces[i];
        }
        SetRequiredSurfaces(idString.str(), fadeOpera ? 1 : 0);
    }
};

class MazdaEventCallbacks;

class VideoOutput {
    GstElement *vid_pipeline = nullptr;
    GstAppSrc *vid_src = nullptr;
    GstElement *vid_sink = nullptr;
    MazdaEventCallbacks* callbacks;

    std::thread input_thread;
    int input_thread_quit_pipe_read = -1;
    int input_thread_quit_pipe_write = -1;
    int touch_fd = -1, kbd_fd = -1;
    void input_thread_func();

    DBus::Connection& hmiBus;
    BUCPSAClient displayClient;
public:
    VideoOutput(MazdaEventCallbacks* callbacks, DBus::Connection& hmiBus);
    ~VideoOutput();

    void MediaPacket(uint64_t timestamp, const byte * buf, int len);
};


