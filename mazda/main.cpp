#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <linux/input.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <poll.h>
#include <inttypes.h>
#include <cmath>
#include <functional>
#include <condition_variable>
#include <sstream>
#include <fstream>

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "dbus/generated_cmu.h"
#include "dbus/generated_hu.h"

#include "hu_uti.h"
#include "hu_aap.h"

#include "nm/mzd_nightmode.h"
#include "gps/mzd_gps.h"

#define EVENT_DEVICE_TS	"/dev/input/filtered-touchscreen0"
#define EVENT_DEVICE_CMD   "/dev/input/event1"
#define EVENT_TYPE	  EV_ABS
#define EVENT_CODE_X	ABS_X
#define EVENT_CODE_Y	ABS_Y

#define HMI_BUS_ADDRESS "unix:path=/tmp/dbus_hmi_socket"
#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

__asm__(".symver realpath1,realpath1@GLIBC_2.11.1");


typedef struct {
	GMainLoop *loop;
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
GstElement *vid_sink = nullptr;

#define ASPECT_RATIO_FIX 1

IHUAnyThreadInterface* g_hu = nullptr;


bool display_status = true;
static void set_display_status(bool st)
{
	printf("set_display_status %s\n", st ? "true" : "false");
	if (st != display_status)
	{
        if (vid_sink)
        {
            g_object_set(G_OBJECT(vid_sink), "should-display", st ? TRUE : FALSE, NULL);
            #if ASPECT_RATIO_FIX
            if (st)
            {
            	//This gets forgotten for some reason
            	g_object_set(G_OBJECT(vid_sink), 
            		"axis-left", 0,
            		"axis-top", -20,
            		"disp-width", 800,
            		"disp-height", 520,
            		NULL);
    		}
			#endif
        }
		display_status = st;

 		g_hu->hu_queue_command([st](IHUConnectionThreadInterface& s)
		{
		    HU::VideoFocus videoFocusGained;
		    videoFocusGained.set_mode(st ? HU::VIDEO_FOCUS_MODE_FOCUSED : HU::VIDEO_FOCUS_MODE_UNFOCUSED);
		    videoFocusGained.set_unrequested(true);
		    s.hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
		});

	}
}


static void read_mic_data (GstElement * sink);

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
//					 g_print("got message %s\n", \
							 gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
					 break;
	}

	return TRUE;
}

static int gst_pipeline_init(gst_app_t *app)
{
	GstBus *bus;
	GstStateChangeReturn state_ret;
	
	GError *error = NULL;
	gst_init(NULL, NULL);

	//if we have ASPECT_RATIO_FIX, cut off the bottom black bar
	const char* vid_pipeline_launch = "appsrc name=mysrc is-live=true block=false max-latency=1000000 do-timestamp=true ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 ! mfw_isink name=mysink "
	#if ASPECT_RATIO_FIX
    "axis-left=0 axis-top=-20 disp-width=800 disp-height=520"
	#else
	"axis-left=0 axis-top=0 disp-width=800 disp-height=480"
	#endif
	" max-lateness=1000000000 sync=false async=false";

	vid_pipeline = gst_parse_launch(vid_pipeline_launch, &error);
		
	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	    
	bus = gst_pipeline_get_bus(GST_PIPELINE(vid_pipeline));
	gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);
	gst_object_unref(bus);

	vid_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysrc"));
    vid_sink = GST_ELEMENT(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysink"));
	
	gst_app_src_set_stream_type(vid_src, GST_APP_STREAM_TYPE_STREAM);

	aud_pipeline = gst_parse_launch("appsrc name=audsrc is-live=true block=false max-latency=1000000 do-timestamp=true ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=48000, channels=2 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	aud_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (aud_pipeline), "audsrc"));
	
	gst_app_src_set_stream_type((GstAppSrc *)aud_src, GST_APP_STREAM_TYPE_STREAM);


	au1_pipeline = gst_parse_launch("appsrc name=au1src is-live=true block=false max-latency=1000000 do-timestamp=true ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=16000, channels=1 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	au1_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (au1_pipeline), "au1src"));
	
	gst_app_src_set_stream_type((GstAppSrc *)au1_src, GST_APP_STREAM_TYPE_STREAM);



	mic_pipeline = gst_parse_launch("alsasrc name=micsrc ! audioconvert ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, channels=1, rate=16000 ! queue !appsink name=micsink async=false emit-signals=true blocksize=8192",&error);
	
	if (error != NULL) {
		printf("could not construct mic pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	
	mic_sink = gst_bin_get_by_name (GST_BIN (mic_pipeline), "micsink");

	g_object_set(G_OBJECT(mic_sink), "throttle-time", 3000000, NULL);
		
	g_signal_connect(mic_sink, "new-buffer", G_CALLBACK(read_mic_data), NULL);
	
	state_ret = gst_element_set_state (mic_pipeline, GST_STATE_READY);

	return 0;

}


uint64_t get_cur_timestamp()
{
	struct timespec tp;
	/* Fetch the time stamp */
	clock_gettime(CLOCK_REALTIME, &tp);

	return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;	
}


static void aa_touch_event(HU::TouchInfo::TOUCH_ACTION action, unsigned int x, unsigned int y) {

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
        uint64_t timestamp = GST_CLOCK_TIME_IS_VALID(bufTimestamp) ? bufTimestamp : get_cur_timestamp();
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


struct TouchScreenState {
	int x;
	int y;
	HU::TouchInfo::TOUCH_ACTION action;
	int action_recvd;
};

bool touch_poll_event(TouchScreenState& mTouch, int touchfd, int quitfd)
{

	struct input_event event[64];
	const size_t ev_size = sizeof(struct input_event);
	const size_t buffer_size = ev_size * 64;
	ssize_t size;
	gst_app_t *app = &gst_app;
	
	fd_set set;
	int unblocked;

	FD_ZERO(&set);
	FD_SET(touchfd, &set);
	FD_SET(quitfd, &set);
	
	unblocked = select(std::max(touchfd, quitfd) + 1, &set, NULL, NULL, NULL);

	if (unblocked == -1) {
		printf("Error in read...\n");
		g_main_loop_quit(app->loop);
		return false;
	}
	else if (unblocked > 0 && FD_ISSET(quitfd, &set)) {
		return false;
	}
	
	size = read(touchfd, &event, buffer_size);
	
	if (size == 0 || size == -1)
		return false;
	
	if (size < ev_size) {
		printf("Error size when reading\n");
		g_main_loop_quit(app->loop);
		return false;
	}
	
	int num_chars = size / ev_size;
	
	int i;
	for (i=0;i < num_chars;i++) {
		switch (event[i].type) {
			case EV_ABS:
				switch (event[i].code) {
					case ABS_MT_POSITION_X:
						mTouch.x = event[i].value * 800 /4095;
						break;
					case ABS_MT_POSITION_Y:
						#if ASPECT_RATIO_FIX
                        mTouch.y = event[i].value * 450/4095 + 15;
						#else
						mTouch.y = event[i].value * 480/4095;
						#endif
						break;
				}
				break;
			case EV_KEY:
				if (event[i].code == BTN_TOUCH) {
					mTouch.action_recvd = 1;
					if (event[i].value == 1) {
						mTouch.action = HU::TouchInfo::TOUCH_ACTION_PRESS;
					}
					else {
						mTouch.action = HU::TouchInfo::TOUCH_ACTION_RELEASE;
					}
				}
				break;
			case EV_SYN:
				if (mTouch.action_recvd == 0) {
					mTouch.action = HU::TouchInfo::TOUCH_ACTION_DRAG;
					aa_touch_event(mTouch.action, mTouch.x, mTouch.y);
				} else {
					aa_touch_event(mTouch.action, mTouch.x, mTouch.y);
					mTouch.action_recvd = 0;
				}
				break;
		}
	} 

	
	return true;
}

static void input_thread_func(int touchfd, int quitfd) 
{

	TouchScreenState mTouch {0,0,(HU::TouchInfo::TOUCH_ACTION)0,0};
	while (touch_poll_event(mTouch, touchfd, quitfd)) {

	}
}

static gboolean delayedShouldDisplayTrue(gpointer data)
{
	set_display_status(true);
	return FALSE;
}

static gboolean delayedShouldDisplayFalse(gpointer data)
{
	set_display_status(false);
	return FALSE;
}

static gboolean delayedToggleShouldDisplay(gpointer data)
{
	set_display_status(!display_status);
	return FALSE;
}

class BUCPSAClient : public com::jci::bucpsa_proxy,
                     public DBus::ObjectProxy
{
public:
    BUCPSAClient(DBus::Connection &connection, const char *path, const char *name)
        : DBus::ObjectProxy(connection, path, name)
    {
    }

    virtual void CommandResponse(const uint32_t& cmdResponse) override {}
    virtual void DisplayMode(const uint32_t& currentDisplayMode) override
    {
        if (currentDisplayMode)
        {
            g_timeout_add(1, delayedShouldDisplayFalse, NULL);
        }
        else
        {
            g_timeout_add(750, delayedShouldDisplayTrue, NULL);
        }
    }
    virtual void ReverseStatusChanged(const int32_t& reverseStatus) override {}
    virtual void PSMInstallStatusChanged(const uint8_t& psmInstalled) override {}
};

class InputFilterClient : public us::insolit::mazda::connector_proxy,
                          public DBus::ObjectProxy
{
public:
    InputFilterClient(DBus::Connection &connection, const char *path, const char *name)
        : DBus::ObjectProxy(connection, path, name)
    {
    }

    virtual void KeyEvent(const ::DBus::Struct< ::DBus::Struct< uint64_t, uint64_t >, uint16_t, uint16_t, int32_t >& value) override
    {
        struct input_event event;
        event.time.tv_sec = value._1._1;
        event.time.tv_usec = value._1._2;

        event.type = value._2;
        event.code = value._3;
        event.value = value._4;

        //key press
        if (event.type == EV_KEY && (event.value == 1 || event.value == 0)) {

            uint64_t timeStamp = get_cur_timestamp();
            uint32_t scanCode = 0;
            int32_t scrollAmount = 0;
            bool isPressed = (event.value == 1);

            printf("Key code %i value %i\n", (int)event.code, (int)event.value);
            switch (event.code) {
            case KEY_G:
                printf("KEY_G\n");
                scanCode = HUIB_MIC;
                break;
            //Make the music button play/pause
            case KEY_E:
                printf("KEY_E\n");
                scanCode = HUIB_PLAYPAUSE;
                break;
            case KEY_LEFTBRACE:
                printf("KEY_LEFTBRACE\n");
                scanCode = HUIB_NEXT;
                break;
            case KEY_RIGHTBRACE:
                printf("KEY_RIGHTBRACE\n");
                scanCode = HUIB_PREV;
                break;
            case KEY_BACKSPACE:
                printf("KEY_BACKSPACE\n");
                scanCode = HUIB_BACK;
                break;
            case KEY_ENTER:
                printf("KEY_ENTER\n");
                scanCode = HUIB_ENTER;
                break;
            case KEY_LEFT:
                printf("KEY_LEFT\n");
                scanCode = HUIB_LEFT;
                break;
            case KEY_N:
                printf("KEY_N\n");
                if (isPressed)
                {
                    scrollAmount = -1;
                }
                break;
            case KEY_RIGHT:
                printf("KEY_RIGHT\n");
                scanCode = HUIB_RIGHT;
                break;
            case KEY_M:
                printf("KEY_M\n");
                if (isPressed)
                {
                    scrollAmount = 1;
                }
                break;
            case KEY_UP:
                printf("KEY_UP\n");
                scanCode = HUIB_UP;
                break;
            case KEY_DOWN:
                printf("KEY_DOWN\n");
                scanCode = HUIB_DOWN;
                break;
            case KEY_HOME:
                printf("KEY_HOME\n");
                if (isPressed)
                {
                    g_main_loop_quit (gst_app.loop);
                }
                break;
            case KEY_R:
                printf("KEY_R\n");
                if (isPressed)
                {
                    g_timeout_add(1, delayedToggleShouldDisplay, NULL);
                }
                break;
            }
            if (scanCode != 0 || scrollAmount != 0) {
                g_hu->hu_queue_command([timeStamp, scanCode, scrollAmount, isPressed](IHUConnectionThreadInterface& s)
                {
                    HU::InputEvent inputEvent;
                    inputEvent.set_timestamp(timeStamp);
                    if (scanCode != 0)
                    {
                        HU::ButtonInfo* buttonInfo = inputEvent.mutable_button()->add_button();
                        buttonInfo->set_is_pressed(isPressed);
                        buttonInfo->set_meta(0);
                        buttonInfo->set_long_press(false);
                        buttonInfo->set_scan_code(scanCode);
                    }
                    if (scrollAmount != 0)
                    {
                        HU::RelativeInputEvent* rel = inputEvent.mutable_rel_event()->mutable_event();
                        rel->set_delta(scrollAmount);
                        rel->set_scan_code(HUIB_SCROLLWHEEL);
                    }
                    s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
                });
            }
        }
    }
};

static void nightmode_thread_func(std::condition_variable& quitcv, std::mutex& quitmutex) 
{
	int nightmode = NM_NO_VALUE;
	mzd_nightmode_start();
    while (true)
	{		
		int nightmodenow = mzd_is_night_mode_set();

		// We send nightmode status periodically, otherwise Google Maps
		// doesn't switch to nightmode if it's started late. Even if the
		// other AA UI is already in nightmode.
		if (nightmodenow != NM_NO_VALUE) {
			nightmode = nightmodenow;

			g_hu->hu_queue_command([nightmodenow](IHUConnectionThreadInterface& s)
			{
				HU::SensorEvent sensorEvent;
				sensorEvent.add_night_mode()->set_is_night(nightmodenow);

				s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
			});
		}
		
		{
			std::unique_lock<std::mutex> lk(quitmutex);
    		if (quitcv.wait_for(lk, std::chrono::milliseconds(1000)) == std::cv_status::no_timeout)
    		{
    			break;
    		}
		}
	}

	mzd_nightmode_stop();
}


static void signals_handler (int signum)
{
	if (signum == SIGINT)
	{
		if (gst_app.loop && g_main_loop_is_running (gst_app.loop))
		{
			g_main_loop_quit (gst_app.loop);
		}
	}
}


class MazdaEventCallbacks : public IHUConnectionThreadEventCallbacks
{
public:
  virtual int MediaPacket(int chan, uint64_t timestamp, const byte * buf, int len) override
  {
  	GstAppSrc* gst_src = nullptr;
  	if (chan == AA_CH_VID)
  	{
  		gst_src = vid_src;
  	}
  	else if (chan == AA_CH_AUD)
  	{
  		gst_src  = aud_src;
  	}
  	else if (chan == AA_CH_AU1)
	{
		gst_src = au1_src;
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
  	if (gst_app.loop && g_main_loop_is_running (gst_app.loop))
	{
  		g_main_loop_quit(gst_app.loop);
  	}
  }

  virtual void CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel& streamChannel) override
  {
    #if ASPECT_RATIO_FIX
        if (chan == AA_CH_VID)
        {
            auto videoConfig = streamChannel.mutable_video_configs(0);
            //This adds a 15px border to the top and bottom of the stream, but makes the image 16:9. We chop off the borders and stretch
            //vertically in gstreamer
            videoConfig->set_margin_height(30);
        }
    #endif
  }
};

void gps_location_handler(uint64_t timestamp, double lat, double lng, double bearing, double speed, double alt, double accuracy) {
	logd("[LOC][%" PRIu64 "] - Lat: %f Lng: %f Brng: %f Spd: %f Alt: %f Acc: %f \n", 
			timestamp, lat, lng, bearing, speed, alt, accuracy);

	g_hu->hu_queue_command([timestamp, lat, lng, bearing, speed, alt, accuracy](IHUConnectionThreadInterface& s)
	{
		HU::SensorEvent sensorEvent;
		HU::SensorEvent::LocationData* location = sensorEvent.add_location_data();
		location->set_timestamp(timestamp);
		location->set_latitude(static_cast<int32_t>(lat * 1E7));
		location->set_longitude(static_cast<int32_t>(lng * 1E7));

		if (bearing != 0) {
			location->set_bearing(static_cast<int32_t>(bearing * 1E6));
		}

		// AA expects speed in knots, so convert back
		location->set_speed(static_cast<int32_t>((speed / 1.852) * 1E3));

		if (alt != 0) {
			location->set_altitude(static_cast<int32_t>(alt * 1E2));
		}

		location->set_accuracy(static_cast<int32_t>(accuracy * 1E3));

		s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
	});
}

DBus::Glib::BusDispatcher dispatcher;

int main (int argc, char *argv[])
{	
	//Force line-only buffering so we can see the output during hangs
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	hu_log_library_versions();
	hu_install_crash_handler();

    DBus::default_dispatcher = &dispatcher;
    DBus::_init_threading();

    dispatcher.attach(nullptr);
    printf("DBus::Glib::BusDispatcher attached\n");

	//signal (SIGTERM, signals_handler);

	gst_app_t *app = &gst_app;
	int ret = 0;
	errno = 0;
	byte ep_in_addr  = -2;
	byte ep_out_addr = -2;

	/* Init gstreamer pipeline */
	ret = gst_pipeline_init(app);
	if (ret < 0) {
		printf("gst_pipeline_init() ret: %d\n", ret);
		return (-4);
	}


	MazdaEventCallbacks callbacks;
	HUServer headunit(callbacks);

	g_hu = &headunit.GetAnyThreadInterface();

	/* Start AA processing */
	ret = headunit.hu_aap_start (ep_in_addr, ep_out_addr);
	if (ret < 0) {
		printf("Phone is not connected. Connect a supported phone and restart.\n");
		return 0;
	}

	printf("Starting Android Auto...\n");

	/* Open Touchscreen Device */
	int touchfd = open(EVENT_DEVICE_TS, O_RDONLY);

	if (touchfd < 0) {
		fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_TS);
		return -3;
	}

	int quitpiperw[2];
	if (pipe(quitpiperw) < 0) {
		fprintf(stderr, "Pipe failed");
		return -3;
	}
	int quitp_read = quitpiperw[0];
	int quitp_write = quitpiperw[1];

	std::condition_variable quitcv;
	std::mutex quitmutex;
	
	std::thread input_thread([touchfd, quitp_read](){ input_thread_func(touchfd, quitp_read); } );
	std::thread nm_thread([&quitcv, &quitmutex](){ nightmode_thread_func(quitcv, quitmutex); } );

	/* Start gstreamer pipeline and main loop */

	// GPS processing
	mzd_gps_start(&gps_location_handler);

    gst_element_set_state((GstElement*)vid_pipeline, GST_STATE_PLAYING);
    gst_element_set_state((GstElement*)aud_pipeline, GST_STATE_PLAYING);
    gst_element_set_state((GstElement*)au1_pipeline, GST_STATE_PLAYING);

    gst_app.loop = g_main_loop_new (NULL, FALSE);
    //	g_timeout_add_full(G_PRIORITY_HIGH, 1, myMainLoop, (gpointer)app, NULL);

    printf("Starting Android Auto...\n");
    try
    {
        DBus::Connection hmiBus(HMI_BUS_ADDRESS, false);
        hmiBus.register_bus();

        BUCPSAClient bucpsaClient(hmiBus, "/com/jci/bucpsa", "com.jci.bucpsa");
        printf("Created BUCPSAClient\n");
        InputFilterClient ifClient(hmiBus, "/us/insolit/mazda/connector", "us.insolit.mazda.connector");
        printf("Created InputFilterClient\n");

        g_main_loop_run (gst_app.loop);
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: Failed to connect to HMI bus %s: %s", error.name(), error.message());
    }

    gst_element_set_state((GstElement*)vid_pipeline, GST_STATE_NULL);
    gst_element_set_state((GstElement*)aud_pipeline, GST_STATE_NULL);
    gst_element_set_state((GstElement*)au1_pipeline, GST_STATE_NULL);
    gst_element_set_state((GstElement*)mic_pipeline, GST_STATE_NULL);


	printf("quitting...\n");

	//data we write doesn't matter, wake up touch polling thread
	write(quitp_write, &quitp_write, sizeof(quitp_write));

	//wake up night mode polling thread
	quitcv.notify_all();

    printf("waiting for input_thread\n");
	input_thread.join();
    printf("waiting for nm_thread\n");
	nm_thread.join();

	printf("waiting for gps_thread\n");
	mzd_gps_stop();

    printf("shutting down\n");

	close(touchfd);

    g_main_loop_unref(gst_app.loop);
    gst_app.loop = nullptr;

		/* Stop AA processing */
	ret = headunit.hu_aap_shutdown();
	if (ret < 0) {
		printf("hu_aap_shutdown() ret: %d\n", ret);
		ret = -6;
	}

    gst_object_unref(vid_pipeline);
    gst_object_unref(mic_pipeline);
    gst_object_unref(aud_pipeline);
    gst_object_unref(au1_pipeline);

    gst_object_unref(vid_src);
    gst_object_unref(vid_sink);
    gst_object_unref(aud_src);
    gst_object_unref(au1_src);
    gst_object_unref(mic_sink);

	g_hu = nullptr;

	system("kill -SIGUSR2 $(pgrep input_filter)");

	printf("END \n");

	return 0;
}

