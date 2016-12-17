#include <glib.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/interfaces/xoverlay.h>
#include <gdk/gdk.h>
#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#include <time.h>
#include <glib-unix.h>

//This gets defined by SDL and breaks the protobuf headers
#undef Status

#include "hu_uti.h"
#include "hu_aap.h"

typedef struct {
    GMainLoop *loop;
    GstElement *sink;
    GstElement *decoder;
    GstElement *convert;
} gst_app_t;


static gst_app_t gst_app;

GstElement *mic_pipeline = nullptr;
GstElement *mic_sink = nullptr;

GstElement *aud_pipeline = nullptr;
GstAppSrc *aud_src = nullptr;

GstElement *au1_pipeline = nullptr;
GstAppSrc *au1_src = nullptr;

GstElement *vid_pipeline = nullptr;
GstAppSrc *vid_src = nullptr;

#ifndef ASPECT_RATIO_FIX
#define ASPECT_RATIO_FIX 1
#endif

float g_dpi_scalefactor = 1.0f;

IHUAnyThreadInterface* g_hu = nullptr;


static void on_pad_added(GstElement *element, GstPad *pad)
{
    GstCaps *caps;
    GstStructure *str;
    gchar *name;
    GstPad *convertsink;
    GstPadLinkReturn ret;

    g_debug("pad added");

    g_assert(pad);

    caps = gst_pad_get_caps(pad);
    
    g_assert(caps);
    
    str = gst_caps_get_structure(caps, 0);

    g_assert(str);

    name = (gchar*)gst_structure_get_name(str);

    g_debug("pad name %s", name);

    if(g_strrstr(name, "video")){

        convertsink = gst_element_get_static_pad(gst_app.convert, "sink");
        g_assert(convertsink);
        ret = gst_pad_link(pad, convertsink);
        g_debug("pad_link returned %d\n", ret);
        gst_object_unref(convertsink);
    }
    gst_caps_unref(caps);
}

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
    gst_app_t *app = (gst_app_t*)ptr;

    switch(GST_MESSAGE_TYPE(message)){

        case GST_MESSAGE_ERROR:{
                           gchar *debug;
                           GError *err;

                           gst_message_parse_error(message, &err, &debug);
                           g_print("Error %s\n", err->message);
                           g_error_free(err);
                           g_free(debug);
                           g_main_loop_quit(app->loop);
                       }
                       break;

        case GST_MESSAGE_WARNING:{
                         gchar *debug;
                         GError *err;
                         gchar *name;

                         gst_message_parse_warning(message, &err, &debug);
                         g_print("Warning %s\nDebug %s\n", err->message, debug);

                         name = (gchar *)GST_MESSAGE_SRC_NAME(message);

                         g_print("Name of src %s\n", name ? name : "nil");
                         g_error_free(err);
                         g_free(debug);
                     }
                     break;

        case GST_MESSAGE_EOS:
                     g_print("End of stream\n");
                     g_main_loop_quit(app->loop);
                     break;

        case GST_MESSAGE_STATE_CHANGED:
                     break;

        default:
//                     g_print("got message %s\n", \
                             gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
                     break;
    }

    return TRUE;
}

static void read_mic_data (GstElement * sink);

static int gst_pipeline_init(gst_app_t *app)
{
    GstBus *bus;
    GstStateChangeReturn state_ret;

    GError *error = NULL;

    gst_init(NULL, NULL);

    const char* vid_launch_str = "appsrc name=mysrc is-live=true block=false max-latency=100000 do-timestamp=true ! video/x-h264, width=800,height=480,framerate=30/1 ! decodebin2 name=mydecoder "
    #if ASPECT_RATIO_FIX
    " ! videocrop top=16 bottom=15 "
    #endif
    " ! videoscale name=myconvert ! xvimagesink name=mysink";
    vid_pipeline = gst_parse_launch(vid_launch_str, &error);

    bus = gst_pipeline_get_bus(GST_PIPELINE(vid_pipeline));
    gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);
    gst_object_unref(bus);

    vid_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysrc"));

	gst_app_src_set_stream_type(vid_src, GST_APP_STREAM_TYPE_STREAM);

    app->decoder = gst_bin_get_by_name (GST_BIN (vid_pipeline), "mydecoder");
    app->convert = gst_bin_get_by_name (GST_BIN (vid_pipeline), "myconvert");
    app->sink = gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysink");

    g_assert(app->decoder);
    g_assert(app->convert);
    g_assert(app->sink);

    g_signal_connect(app->decoder, "pad-added",
            G_CALLBACK(on_pad_added), app->decoder);

    
    aud_pipeline = gst_parse_launch("appsrc name=audsrc is-live=true block=false max-latency=100000 do-timestamp=true ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=48000, channels=2 ! volume volume=0.5 ! alsasink buffer-time=400000",&error);

    if (error != NULL) {
        printf("could not construct pipeline: %s\n", error->message);
        g_clear_error (&error);    
        return -1;
    }    

    aud_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (aud_pipeline), "audsrc"));
    
    gst_app_src_set_stream_type(aud_src, GST_APP_STREAM_TYPE_STREAM);


    au1_pipeline = gst_parse_launch("appsrc name=au1src is-live=true block=false max-latency=100000 do-timestamp=true ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=16000, channels=1 ! volume volume=0.5 ! alsasink buffer-time=400000 ",&error);

    if (error != NULL) {
        printf("could not construct pipeline: %s\n", error->message);
        g_clear_error (&error);    
        return -1;
    }    

    au1_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (au1_pipeline), "au1src"));
    
    gst_app_src_set_stream_type(au1_src, GST_APP_STREAM_TYPE_STREAM);

    mic_pipeline = gst_parse_launch("alsasrc name=micsrc ! audioconvert ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, channels=1, rate=16000 ! queue !appsink name=micsink async=false emit-signals=true blocksize=8192",&error);
    
    if (error != NULL) {
        printf("could not construct pipeline: %s\n", error->message);
        g_clear_error (&error);    
        return -1;
    }
        
    mic_sink = gst_bin_get_by_name (GST_BIN (mic_pipeline), "micsink");
    
    g_object_set(G_OBJECT(mic_sink), "throttle-time", 3000000, NULL);
        
    g_signal_connect(mic_sink, "new-buffer", G_CALLBACK(read_mic_data), NULL);
    
    gst_element_set_state (mic_pipeline, GST_STATE_READY);

    return 0;
}

uint64_t get_cur_timestamp()
{
    struct timespec tp;
    /* Fetch the time stamp */
    clock_gettime(CLOCK_REALTIME, &tp);

    return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;    
}


static void aa_touch_event(HU::TouchInfo::TOUCH_ACTION action, unsigned int x, unsigned int y)
{
    float normx = float(x) / float(SDL_GetVideoInfo()->current_w);
    float normy = float(y) / float(SDL_GetVideoInfo()->current_h);

    x = (unsigned int)(normx * 800);
#if ASPECT_RATIO_FIX
    y = (unsigned int)(normy * 450) + 15;
#else
    y = (unsigned int)(normy * 480);
#endif
    
	g_hu->hu_queue_command([action, x, y](IHUConnectionThreadInterface& s)
	{
 		HU::InputEvent inputEvent;
	    inputEvent.set_timestamp(get_cur_timestamp());
	    HU::TouchInfo* touchEvent = inputEvent.mutable_touch();
	    touchEvent->set_action(action);
	    HU::TouchInfo::Location* touchLocation = touchEvent->add_location();
	    touchLocation->set_x(x);
	    touchLocation->set_y(y);
	    touchLocation->set_pointer_id(0);

	    /* Send touch event */
	    
	    int ret = s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
	    if (ret < 0) {
	        printf("aa_touch_event(): hu_aap_enc_send() failed with (%d)\n", ret);
	    }
	});
}


static void read_mic_data (GstElement * sink)
{
        
    GstBuffer *gstbuf;
    int ret;
    
    g_signal_emit_by_name (sink, "pull-buffer", &gstbuf,NULL);
    
    if (gstbuf) {

        /* if mic is stopped, don't bother sending */    
        GstState mic_state = GST_STATE_NULL;
        if (gst_element_get_state (mic_pipeline, &mic_state, NULL, GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_SUCCESS
            || mic_state != GST_STATE_PLAYING) {
            printf("Mic stopped.. dropping buffers \n");
            gst_buffer_unref(gstbuf);
            return;
        }
        
        
        gint mic_buf_sz = GST_BUFFER_SIZE (gstbuf);
        
        int idx;
        
        if (mic_buf_sz <= 64) {
            printf("Mic data < 64 \n");
            gst_buffer_unref(gstbuf);
            return;
        }
        
        uint64_t bufTimestamp = GST_BUFFER_TIMESTAMP(gstbuf);
        uint64_t timestamp = GST_CLOCK_TIME_IS_VALID(bufTimestamp) ? (bufTimestamp / 1000) : get_cur_timestamp();
        g_hu->hu_queue_command([timestamp, gstbuf, mic_buf_sz](IHUConnectionThreadInterface& s)
		{
	        int ret = s.hu_aap_enc_send_media_packet(1, AA_CH_MIC, HU_PROTOCOL_MESSAGE::MediaDataWithTimestamp, timestamp, GST_BUFFER_DATA(gstbuf), mic_buf_sz);
	       
	        if (ret < 0) {
	            printf("read_mic_data(): hu_aap_enc_send() failed with (%d)\n", ret);
	        }
	        
	        gst_buffer_unref(gstbuf);
	    });
    }
}

bool g_nightmode = false;

/* Print all information about a key event */
void PrintKeyInfo( SDL_KeyboardEvent *key ){
    /* Is it a release or a press? */
    if( key->type == SDL_KEYUP )
        printf( "Release:- " );
    else
        printf( "Press:- " );

    /* Print the hardware scancode first */
    printf( "Scancode: 0x%02X", key->keysym.scancode );
    /* Print the name of the key */
    printf( ", Name: %s", SDL_GetKeyName( key->keysym.sym ) );
    /* We want to print the unicode info, but we need to make */
    /* sure its a press event first (remember, release events */
    /* don't have unicode info                                */
    if( key->type == SDL_KEYDOWN ){
        /* If the Unicode value is less than 0x80 then the    */
        /* unicode value can be used to get a printable       */
        /* representation of the key, using (char)unicode.    */
        printf(", Unicode: " );
        if( key->keysym.unicode < 0x80 && key->keysym.unicode > 0 ){
            printf( "%c (0x%04X)", (char)key->keysym.unicode,
                    key->keysym.unicode );
        }
        else{
            printf( "? (0x%04X)", key->keysym.unicode );
        }
    }
    printf( "\n" );
}

gboolean sdl_poll_event(gpointer data)
{
    gst_app_t *app = (gst_app_t *)data;

    SDL_Event event;
    SDL_MouseButtonEvent *mbevent;
    SDL_MouseMotionEvent *mmevent;
    SDL_KeyboardEvent *key;

    bool nightmodenow = g_nightmode;

    int ret;
    while (SDL_PollEvent(&event) > 0) {
        switch (event.type) {
        case SDL_MOUSEMOTION:
            mmevent = &event.motion;
            if (mmevent->state & SDL_BUTTON_LMASK)
            {
                aa_touch_event(HU::TouchInfo::TOUCH_ACTION_DRAG, (unsigned int)mmevent->x, (unsigned int)mmevent->y);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            mbevent = &event.button;
            if (mbevent->button == SDL_BUTTON_LEFT) {
                aa_touch_event(HU::TouchInfo::TOUCH_ACTION_PRESS, (unsigned int)mbevent->x, (unsigned int)mbevent->y);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            mbevent = &event.button;
            if (mbevent->button == SDL_BUTTON_LEFT) {
                aa_touch_event(HU::TouchInfo::TOUCH_ACTION_RELEASE, (unsigned int)mbevent->x, (unsigned int)mbevent->y);
            } else if (mbevent->button == SDL_BUTTON_RIGHT) {
                printf("Quitting...\n");
                g_main_loop_quit(app->loop);
//                SDL_Quit();
                return FALSE;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym != 0)
            {
                key = &event.key;
                PrintKeyInfo(key);

                HU::InputEvent inputEvent;
                inputEvent.set_timestamp(get_cur_timestamp());
                HU::ButtonInfo* buttonInfo = inputEvent.mutable_button()->add_button();
                buttonInfo->set_is_pressed(event.type == SDL_KEYDOWN);
                buttonInfo->set_meta(0);
                buttonInfo->set_long_press(false);
                if (key->keysym.sym == SDLK_UP) {
                    buttonInfo->set_scan_code(HUIB_UP);
                }            
                else if (key->keysym.sym == SDLK_DOWN) {
                    buttonInfo->set_scan_code(HUIB_DOWN);
                }
                else if (key->keysym.sym == SDLK_TAB) { //Left is the menu, so kinda tab?
                     buttonInfo->set_scan_code(HUIB_LEFT);
                }
                //This is just mic again
                // else if (key->keysym.sym == SDLK_RIGHT) {
                //      buttonInfo->set_scan_code(HUIB_RIGHT);
                // }
                else if (key->keysym.sym == SDLK_LEFT || key->keysym.sym == SDLK_RIGHT)
                {
                    if (event.type == SDL_KEYDOWN)
                    {
                        HU::InputEvent inputEvent2;
                        inputEvent2.set_timestamp(get_cur_timestamp());
                        HU::RelativeInputEvent* rel = inputEvent2.mutable_rel_event()->mutable_event();
                        rel->set_delta(key->keysym.sym == SDLK_LEFT ? -1 : 1);
                        rel->set_scan_code(HUIB_SCROLLWHEEL);

				 		g_hu->hu_queue_command([inputEvent2](IHUConnectionThreadInterface& s)
						{
                        	s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent2);
                    	});
                    }
                }
                else if (key->keysym.sym == SDLK_m) {
                    buttonInfo->set_scan_code(HUIB_MIC);
                }
                else if (key->keysym.sym == SDLK_p) {
                    buttonInfo->set_scan_code(HUIB_PREV);
                }
                else if (key->keysym.sym == SDLK_n) {
                    buttonInfo->set_scan_code(HUIB_NEXT);
                }
                else if (key->keysym.sym == SDLK_SPACE) {
                    buttonInfo->set_scan_code(HUIB_PLAYPAUSE);
                }
                else if (key->keysym.sym == SDLK_RETURN) {
                    buttonInfo->set_scan_code(HUIB_ENTER);
                }
                else if (key->keysym.sym == SDLK_BACKSPACE) {
                    buttonInfo->set_scan_code(HUIB_BACK);
                }
                else if (key->keysym.sym == SDLK_F1) {
                    if (event.type == SDL_KEYUP)
                    {
                        nightmodenow = !nightmodenow;
                    }
                }

                if (buttonInfo->has_scan_code()) {
			 		ret = g_hu->hu_queue_command([inputEvent](IHUConnectionThreadInterface& s)
					{
                		s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
                	});
                    if (ret == -1) {
                        g_main_loop_quit(app->loop);
                        SDL_Quit();
                        return FALSE;
                    }
                }
            }                                                
            break;

        case SDL_QUIT:
            printf("Quitting...\n");
            g_main_loop_quit(app->loop);
//            SDL_Quit();
            return FALSE;
            break;
        }
    }
    
   
   
    if (g_nightmode != nightmodenow) {
        g_nightmode = nightmodenow;

		g_hu->hu_queue_command([nightmodenow](IHUConnectionThreadInterface& s)
		{
	        HU::SensorEvent sensorEvent;
	        sensorEvent.add_night_mode()->set_is_night(nightmodenow);

	        s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
    	});

        printf("Nightmode: %s\n", g_nightmode ? "On" : "Off");
    }
    
    return TRUE;
}


static gboolean on_sig_received (gpointer data)
{
    gst_app_t *app = (gst_app_t *)data;
    g_main_loop_quit(app->loop);
    return FALSE;
}

static int gst_loop(gst_app_t *app)
{
    int ret;
    GstStateChangeReturn state_ret;

    state_ret = gst_element_set_state(vid_pipeline, GST_STATE_PLAYING);
    state_ret = gst_element_set_state(aud_pipeline, GST_STATE_PLAYING);
    state_ret = gst_element_set_state(au1_pipeline, GST_STATE_PLAYING);
//    g_warning("set state returned %d\n", state_ret);

    app->loop = g_main_loop_new (NULL, FALSE);
    g_unix_signal_add (SIGTERM, on_sig_received, app);
    g_timeout_add_full(G_PRIORITY_HIGH, 100, sdl_poll_event, (gpointer)app, NULL);
    printf("Starting Android Auto...\n");
      g_main_loop_run (app->loop);


    state_ret = gst_element_set_state(vid_pipeline, GST_STATE_NULL);
    state_ret = gst_element_set_state(mic_pipeline, GST_STATE_NULL);
    state_ret = gst_element_set_state(aud_pipeline, GST_STATE_NULL);
    state_ret = gst_element_set_state(au1_pipeline, GST_STATE_NULL);
//    g_warning("set state null returned %d\n", state_ret);

    gst_object_unref(vid_pipeline);
    gst_object_unref(mic_pipeline);
    gst_object_unref(aud_pipeline);
    gst_object_unref(au1_pipeline);

    gst_object_unref(vid_src);
    gst_object_unref(mic_sink);
    gst_object_unref(aud_src);
    gst_object_unref(au1_src);

    gst_object_unref(app->decoder);
    gst_object_unref(app->convert);
    gst_object_unref(app->sink);
    
    ms_sleep(100);
    
    printf("here we are \n");
    /* Should not reach this? */
    SDL_Quit();

    return ret;
}


class DesktopEventCallbacks : public IHUConnectionThreadEventCallbacks
{
public:
  virtual int MediaPacket(int chan, uint64_t timestamp, const byte * buf, int len) override
  {
    GstAppSrc* gst_src = nullptr;
    GstElement* gst_pipe = nullptr;
    if (chan == AA_CH_VID)
    {
        gst_src = vid_src;
        gst_pipe = vid_pipeline;
    }
    else if (chan == AA_CH_AUD)
    {
        gst_src  = aud_src;
        gst_pipe = aud_pipeline;
    }
    else if (chan == AA_CH_AU1)
    {
        gst_src = au1_src;
        gst_pipe = au1_pipeline;
    }

    if (gst_src)
    {
        GstBuffer * buffer = gst_buffer_new_and_alloc(len);
        memcpy(GST_BUFFER_DATA(buffer), buf, len);
        int ret = gst_app_src_push_buffer(gst_src, buffer);
        if(ret !=  GST_FLOW_OK){
            printf("push buffer returned %d for %d bytes \n", ret, len);
        }
    }
    return 0;
  }

  virtual int MediaStart(int chan) override
  {
    if (chan == AA_CH_MIC)
    {
        printf("SHAI1 : Mic Started\n");
        gst_element_set_state (mic_pipeline, GST_STATE_PLAYING);
    }   
    return 0;
  }

  virtual int MediaStop(int chan) override
  {
    if (chan == AA_CH_MIC)
    {
        printf("SHAI1 : Mic Stopped\n");
        gst_element_set_state (mic_pipeline, GST_STATE_READY);
    }
    return 0;
  }

  virtual void DisconnectionOrError() override
  {
    printf("DisconnectionOrError\n");
    g_main_loop_quit(gst_app.loop);
  }

  virtual void CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel& streamChannel) override
  {
    #if ASPECT_RATIO_FIX
        if (chan == AA_CH_VID)
        {
            auto videoConfig = streamChannel.mutable_video_configs(0);
            videoConfig->set_margin_height(30);
        }
    #endif
  }
};

int main (int argc, char *argv[])
{    

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    hu_log_library_versions();
    hu_install_crash_handler();

    //Assuming we are on Gnome, what's the DPI scale factor?
    gdk_init(&argc, &argv);

    GdkScreen * primaryDisplay = gdk_screen_get_default();
    if (primaryDisplay)
    {
        g_dpi_scalefactor = (float)gdk_screen_get_monitor_scale_factor(primaryDisplay, 0);
        printf("Got gdk_screen_get_monitor_scale_factor() == %f\n", g_dpi_scalefactor);
    }

    gst_app_t *app = &gst_app;
    int ret = 0;
    errno = 0;
    byte ep_in_addr  = -2;
    byte ep_out_addr = -2;
    SDL_Cursor *cursor;

    /* Init gstreamer pipelien */
    ret = gst_pipeline_init(app);
    if (ret < 0) {
        printf("STATUS:gst_pipeline_init() ret: %d\n", ret);
        return (ret);
    }

    DesktopEventCallbacks callbacks;
    HUServer headunit(callbacks);

    /* Overlay gst sink on the Qt window */
//    WId xwinid = window->winId();
    
//#endif


    /* Start AA processing */
    ret = headunit.hu_aap_start (ep_in_addr, ep_out_addr);
    if (ret < 0) {
        printf("Phone is not connected. Connect a supported phone and restart.\n");
        return 0;
    }

    
    g_hu = &headunit.GetAnyThreadInterface();

    SDL_SysWMinfo info;

    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_WM_SetCaption("Android Auto", NULL);
    #if ASPECT_RATIO_FIX
    //emulate the CMU stretching
    SDL_Surface *screen = SDL_SetVideoMode((int)(853*g_dpi_scalefactor), (int)(480*g_dpi_scalefactor), 32, SDL_HWSURFACE);
    #else
    SDL_Surface *screen = SDL_SetVideoMode((int)(800*g_dpi_scalefactor), (int)(480*g_dpi_scalefactor), 32, SDL_HWSURFACE);
    #endif

    struct SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if(-1 == SDL_GetWMInfo(&wmInfo))
        printf("STATUS:errorxxxxx \n");
    
    
    SDL_EnableUNICODE(1);
    
    gst_x_overlay_set_window_handle(GST_X_OVERLAY(app->sink), wmInfo.info.x11.window);
    //Let SDL do the key events, etc
    gst_x_overlay_handle_events(GST_X_OVERLAY(app->sink), FALSE);

    //Don't use SDL's weird cursor, too small on HiDPI
    XUndefineCursor(wmInfo.info.x11.display, wmInfo.info.x11.window);

    /* Start gstreamer pipeline and main loop */
    ret = gst_loop(app);
    if (ret < 0) {
        printf("STATUS:gst_loop() ret: %d\n", ret);
    }

    /* Stop AA processing */
    ret = headunit.hu_aap_shutdown ();
    if (ret < 0) {
        printf("STATUS:hu_aap_stop() ret: %d\n", ret);
        SDL_Quit();
        return (ret);
    }

    g_hu = nullptr;

    SDL_Quit();
    
    if (ret == 0) {
        printf("STATUS:Press Back or Home button to close\n");
    }

    return (ret);
}
