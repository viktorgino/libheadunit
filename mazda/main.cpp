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
#include <functional>


#include "hu_uti.h"
#include "hu_aap.h"

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
	GstPipeline *pipeline;
	GstAppSrc *src;
	GstElement *sink;
	GstElement *decoder;
	GstElement *queue;
	guint sourceid;
} gst_app_t;


static gst_app_t gst_app;

GstElement *mic_pipeline, *mic_sink;
GstElement *aud_pipeline, *aud_src;
GstElement *au1_pipeline, *au1_src;

GAsyncQueue *sendqueue;
typedef std::function<void()> SendQueueFunc;

typedef struct {
	int fd;
	int x;
	int y;
	HU::TouchInfo::TOUCH_ACTION action;
	int action_recvd;
} mytouchscreen;

mytouchscreen mTouch = (mytouchscreen){0,0,0,(HU::TouchInfo::TOUCH_ACTION)0,0};

typedef struct {
	int fd;
	uint8_t action;
} mycommander;

mycommander mCommander = (mycommander){0,0};



static inline void queueSend(SendQueueFunc&& sendFunc)
{
	g_async_queue_push(sendqueue, new SendQueueFunc(sendFunc));
}


int mic_change_state = 0;

static void read_mic_data (GstElement * sink);

static gboolean read_data(gst_app_t *app)
{
	GstBuffer *buffer;
	guint8 *ptr;
	GstFlowReturn ret;
	int iret;
	char *vbuf;
	char *abuf;
	int res_len = 0;

	iret = hu_aap_recv_process ();					   

	if (iret != 0) {
		printf("hu_aap_recv_process() iret: %d\n", iret);
		g_main_loop_quit(app->loop);		
		return FALSE;
	}

	/* Is there a video buffer queued? */
	vbuf = vid_read_head_buf_get (&res_len);

	if (vbuf != NULL) {

		//buffer = gst_buffer_new();
		//gst_buffer_set_data(buffer, vbuf, res_len);
		buffer = gst_buffer_new_and_alloc(res_len);
		memcpy(GST_BUFFER_DATA(buffer),vbuf,res_len);

		ret = gst_app_src_push_buffer(app->src, buffer);

		if(ret !=  GST_FLOW_OK){
			printf("push buffer returned %d for %d bytes \n", ret, res_len);
			return FALSE;
		}
	}
	
	/* Is there an audio buffer queued? */
	abuf = aud_read_head_buf_get (&res_len);
	if (abuf != NULL) {

		//buffer = gst_buffer_new();
		//gst_buffer_set_data(buffer, abuf, res_len);
		
		buffer = gst_buffer_new_and_alloc(res_len);
		memcpy(GST_BUFFER_DATA(buffer),abuf,res_len);

		if (res_len <= 2048 + 96)
			ret = gst_app_src_push_buffer((GstAppSrc *)au1_src, buffer);
		else
			ret = gst_app_src_push_buffer((GstAppSrc *)aud_src, buffer);

		if(ret !=  GST_FLOW_OK){
			printf("push buffer returned %d for %d bytes \n", ret, res_len);
			return FALSE;
		}
	}	

	return TRUE;
}

static int shouldRead = FALSE;

static void start_feed (GstElement * pipeline, guint size, void *app)
{
	shouldRead = TRUE;
}

static void stop_feed (GstElement * pipeline, void *app)
{
	shouldRead = FALSE;
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

//	app->pipeline = (GstPipeline*)gst_parse_launch("appsrc name=mysrc is-live=true block=false max-latency=1000000 ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 ! mfw_v4lsink max-lateness=1000000000 sync=false async=false", &error);

	app->pipeline = (GstPipeline*)gst_parse_launch("appsrc name=mysrc is-live=true block=false max-latency=1000000 ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 ! mfw_isink name=mysink axis-left=25 axis-top=0 disp-width=751 disp-height=480 max-lateness=1000000000 sync=false async=false", &error);
		
	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	
	bus = gst_pipeline_get_bus(app->pipeline);
	gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);
	gst_object_unref(bus);

	app->src = (GstAppSrc*)gst_bin_get_by_name (GST_BIN (app->pipeline), "mysrc");
	app->sink = (GstElement*)gst_bin_get_by_name (GST_BIN (app->pipeline), "mysink");
	
	gst_app_src_set_stream_type(app->src, GST_APP_STREAM_TYPE_STREAM);

	g_signal_connect(app->src, "need-data", G_CALLBACK(start_feed), app);
		
	g_signal_connect(app->src, "enough-data", G_CALLBACK(stop_feed), app);

	aud_pipeline = gst_parse_launch("appsrc name=audsrc is-live=true block=false max-latency=1000000 ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=48000, channels=2 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	aud_src = gst_bin_get_by_name (GST_BIN (aud_pipeline), "audsrc");
	
	gst_app_src_set_stream_type((GstAppSrc *)aud_src, GST_APP_STREAM_TYPE_STREAM);


	au1_pipeline = gst_parse_launch("appsrc name=au1src is-live=true block=false max-latency=1000000 ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=16000, channels=1 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	au1_src = gst_bin_get_by_name (GST_BIN (au1_pipeline), "au1src");
	
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

	return tp.tv_sec * 1000000000 + tp.tv_nsec;	
}


static void aa_touch_event(HU::TouchInfo::TOUCH_ACTION action, unsigned int x, unsigned int y) 
{
	uint64_t timeStamp = get_cur_timestamp();
	queueSend([action, x, y, timeStamp]()
	{
		HU::InputEvent inputEvent;
		inputEvent.set_timestamp(timeStamp);
		HU::TouchInfo* touchEvent = inputEvent.mutable_touch();
		touchEvent->set_action(action);
		HU::TouchInfo::Location* touchLocation = touchEvent->add_location();
		touchLocation->set_x(x);
		touchLocation->set_y(y);
		touchLocation->set_pointer_id(0);

		/* Send touch event */
		
		hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
	});
}

static const int max_size = 8192;


static void read_mic_data (GstElement * sink)
{		
	GstBuffer *gstbuf;
	int ret;
	
	g_signal_emit_by_name (sink, "pull-buffer", &gstbuf,NULL);
	
	if (gstbuf) {

		/* if mic is stopped, don't bother sending */	

		if (mic_change_state == 0) {
			printf("Mic stopped.. dropping buffers \n");
			gst_buffer_unref(gstbuf);
			return;
		}
		
		gint mic_buf_sz;
		mic_buf_sz = GST_BUFFER_SIZE (gstbuf);
		
		int idx;
		
		if (mic_buf_sz <= 64) {
			printf("Mic data < 64 \n");
			gst_buffer_unref(gstbuf);
			return;
		}

		uint64_t timeStamp = get_cur_timestamp();
		queueSend([gstbuf, timeStamp, mic_buf_sz]()
		{
			hu_aap_enc_send_media_packet(1, AA_CH_MIC, HU_PROTOCOL_MESSAGE::MediaData0, timeStamp , GST_BUFFER_DATA(gstbuf), mic_buf_sz);
			gst_buffer_unref(gstbuf);
		});
	}
} 

int nightmode = 0;

gboolean touch_poll_event(gpointer data)
{
	int mic_ret = hu_aap_mic_get ();
	
	if (mic_change_state == 0 && mic_ret == 2) {
		printf("SHAI1 : Mic Started\n");
		mic_change_state = 2;
		gst_element_set_state (mic_pipeline, GST_STATE_PLAYING);
	}
		
	if (mic_change_state == 2 && mic_ret == 1) {
		printf("SHAI1 : Mic Stopped\n");
		mic_change_state = 0;
		gst_element_set_state (mic_pipeline, GST_STATE_READY);
	}	
	
	struct input_event event[64];
	const size_t ev_size = sizeof(struct input_event);
	const size_t buffer_size = ev_size * 64;
	ssize_t size;
	gst_app_t *app = (gst_app_t *)data;
	
	fd_set set;
	struct timeval timeout;
	int unblocked;

	FD_ZERO(&set);
	FD_SET(mTouch.fd, &set);

	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;
	
	unblocked = select(mTouch.fd + 1, &set, NULL, NULL, &timeout);

	if (unblocked == -1) {
		printf("Error in read...\n");
		g_main_loop_quit(app->loop);
		return FALSE;
	}
	else if (unblocked == 0) {
			return TRUE;
	}
	
	size = read(mTouch.fd, &event, buffer_size);
	
	if (size == 0 || size == -1)
		return FALSE;
	
	if (size < ev_size) {
		printf("Error size when reading\n");
		g_main_loop_quit(app->loop);
		return FALSE;
	}
	
	int num_chars = size / ev_size;
	
	int i;
	for (i=0;i < num_chars;i++) {
		switch (event[i].type) {
			case EV_ABS:
				switch (event[i].code) {
					case ABS_MT_POSITION_X:
						{
							//account for letterboxing
							printf("input x %i\n", event[i].value);
							float floatPixel = ((event[i].value - 100) / 4095.0f) * 800.0f;
							const float floatBorder = 25.0f / 800.0f;
							floatPixel = (floatPixel / 750.0f) - floatBorder;
														
							mTouch.x = (int)(floatPixel * 800.0f);
							printf("touch x %i\n", mTouch.x);
						}
						break;
					case ABS_MT_POSITION_Y:
						mTouch.y = event[i].value * 480/4095;
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

	
	return TRUE;
}

GMainLoop *mainloop;

int displayStatus = 1;

inline int dbus_message_decode_timeval(DBusMessageIter *iter, struct timeval *time)
{
	DBusMessageIter sub;
	uint64_t tv_sec = 0;
	uint64_t tv_usec = 0;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRUCT) {
		return TRUE;
	}

	dbus_message_iter_recurse(iter, &sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT64) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &tv_sec);
	dbus_message_iter_next(&sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT64) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &tv_usec);

	dbus_message_iter_next(iter);

	time->tv_sec = tv_sec;
	time->tv_usec = tv_usec;

	return TRUE;
}
inline int dbus_message_decode_input_event(DBusMessageIter *iter, struct input_event *event)
{
	DBusMessageIter sub;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRUCT) {
		return TRUE;
	}

	dbus_message_iter_recurse(iter, &sub);

	if (!dbus_message_decode_timeval(&sub, &event->time)) {
		return FALSE;
	}

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT16) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &event->type);
	dbus_message_iter_next(&sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT16) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &event->code);
	dbus_message_iter_next(&sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INT32) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &event->value);

	dbus_message_iter_next(iter);

	return TRUE;
}

static gboolean delayedShouldDisplayTrue(gpointer data)
{
	gst_app_t *app = &gst_app;
	g_object_set(G_OBJECT(app->sink), "should-display", TRUE, NULL);
	displayStatus = TRUE;

	return FALSE;
}


static DBusHandlerResult handle_dbus_message(DBusConnection *c, DBusMessage *message, void *p)
{
	struct timespec tp;
	gst_app_t *app = &gst_app;
	DBusMessageIter iter;

	if (strcmp("KeyEvent", dbus_message_get_member(message)) == 0)
	{
		struct input_event event;

		dbus_message_iter_init(message, &iter);
		dbus_message_decode_input_event(&iter, &event);

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
					g_main_loop_quit (mainloop);
				}
				break;
			case KEY_R:
				printf("KEY_R\n");
				if (isPressed)
				{
					if (displayStatus)
					{
						g_object_set(G_OBJECT(app->sink), "should-display", FALSE, NULL);
						displayStatus = FALSE;
					}
					else
					{
						g_object_set(G_OBJECT(app->sink), "should-display", TRUE, NULL);
						displayStatus = TRUE;
					}
				}
				break;
			}
			if (scanCode != 0 || scrollAmount != 0) {
			 	queueSend([timeStamp, scanCode, scrollAmount, isPressed]()
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
					hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
				});
			}
		}
		//key release
		//else if (event.type == EV_KEY && event.value == 1)

	}
	else if (strcmp("DisplayMode", dbus_message_get_member(message)) == 0)
	{
		int displayMode;
		if (dbus_message_iter_init(message, &iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32)
		{
			dbus_message_iter_get_basic(&iter, &displayMode);
			if (displayMode)
			{
				g_object_set(G_OBJECT(app->sink), "should-display", FALSE, NULL);
				displayStatus = FALSE;
			}
			else
			{
				g_timeout_add(750, delayedShouldDisplayTrue, NULL);
			}
		}

	}

	dbus_message_unref(message);
	return DBUS_HANDLER_RESULT_HANDLED;

}

static void * input_thread(void *app) {

	DBusConnection *hmi_bus;
	DBusError error;

	hmi_bus = dbus_connection_open(HMI_BUS_ADDRESS, &error);

	if (!hmi_bus) {
		printf("DBUS: failed to connect to HMI bus: %s: %s\n", error.name, error.message);
	}

	if (!dbus_bus_register(hmi_bus, &error)) {
		printf("DBUS: failed to register with HMI bus: %s: %s\n", error.name, error.message);
	}

	dbus_connection_add_filter(hmi_bus, handle_dbus_message, NULL, NULL);
	dbus_bus_add_match(hmi_bus, "type='signal',interface='us.insolit.mazda.connector',member='KeyEvent'", &error);
	dbus_bus_add_match(hmi_bus, "type='signal',interface='com.jci.bucpsa',member='DisplayMode'", &error);

	while (touch_poll_event(app)) {
		//commander_poll_event(app);		
		dbus_connection_read_write_dispatch(hmi_bus, 100);
		//ms_sleep(100);
	}
}

static void * nightmode_thread(void *app) 
{

	// Initialize HMI bus
	DBusConnection *hmi_bus;
	DBusError error;

	hmi_bus = dbus_connection_open(HMI_BUS_ADDRESS, &error);

	if (!hmi_bus) {
		printf("DBUS: failed to connect to HMI bus: %s: %s\n", error.name, error.message);
	}

	if (!dbus_bus_register(hmi_bus, &error)) {
		printf("DBUS: failed to register with HMI bus: %s: %s\n", error.name, error.message);
	}

	// Wait for mainloop to start
	ms_sleep(100);

	while (g_main_loop_is_running (mainloop)) {

		DBusMessage *msg = dbus_message_new_method_call("com.jci.BLM_TIME", "/com/jci/BLM_TIME", "com.jci.BLM_TIME", "GetClock");
		DBusPendingCall *pending = NULL;

		if (!msg) {
			printf("DBUS: failed to create message \n");
		}

		if (!dbus_connection_send_with_reply(hmi_bus, msg, &pending, -1)) {
			printf("DBUS: failed to send message \n");
		}

		dbus_connection_flush(hmi_bus);
		dbus_message_unref(msg);

		dbus_pending_call_block(pending);
		msg = dbus_pending_call_steal_reply(pending);
		if (!msg) {
			printf("DBUS: received null reply \n");
		}

		dbus_uint32_t nm_hour;
		dbus_uint32_t nm_min;
		dbus_uint32_t nm_timestamp;
		dbus_uint64_t nm_calltimestamp;
		if (!dbus_message_get_args(msg, &error, DBUS_TYPE_UINT32, &nm_hour,
					DBUS_TYPE_UINT32, &nm_min,
					DBUS_TYPE_UINT32, &nm_timestamp,
					DBUS_TYPE_UINT64, &nm_calltimestamp,
					DBUS_TYPE_INVALID)) {
			printf("DBUS: failed to get result %s: %s\n", error.name, error.message);
		}

		dbus_message_unref(msg);

		int nightmodenow = 1;

		if (nm_hour >= 6 && nm_hour <= 18)
			nightmodenow = 0;

		if (nightmode != nightmodenow) {

			nightmode = nightmodenow;
			queueSend([nightmodenow]()
			{
				HU::SensorEvent sensorEvent;
				sensorEvent.add_night_mode()->set_is_night(nightmodenow);

				hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
			});
		}

		sleep(600);		
	}
}



gboolean myMainLoop(gpointer app)
{
	if (shouldRead)
	{
		read_data((gst_app_t*)app);
	}

	SendQueueFunc* cmd;

	while (cmd = (SendQueueFunc*)g_async_queue_try_pop(sendqueue))
	{
		(*cmd)();
		delete cmd;
	}

	return TRUE; 
}

static void * main_thread(void *app) {

	ms_sleep(100);

	while (mainloop && g_main_loop_is_running (mainloop)) {
		myMainLoop(app);
	}
}


static int gst_loop(gst_app_t *app)
{
	int ret;
	GstStateChangeReturn state_ret;

	state_ret = gst_element_set_state((GstElement*)app->pipeline, GST_STATE_PLAYING);
	state_ret = gst_element_set_state((GstElement*)aud_pipeline, GST_STATE_PLAYING);
	state_ret = gst_element_set_state((GstElement*)au1_pipeline, GST_STATE_PLAYING);

	//	g_warning("set state returned %d\n", state_ret);

	app->loop = g_main_loop_new (NULL, FALSE);

	mainloop = app->loop;

	//	g_timeout_add_full(G_PRIORITY_HIGH, 1, myMainLoop, (gpointer)app, NULL);

	printf("Starting Android Auto...\n");
	g_main_loop_run (app->loop);

	// TO-DO
	state_ret = gst_element_set_state((GstElement*)app->pipeline, GST_STATE_NULL);
	//	g_warning("set state null returned %d\n", state_ret);

	gst_object_unref(app->pipeline);
	gst_object_unref(mic_pipeline);
	gst_object_unref(aud_pipeline);
	gst_object_unref(au1_pipeline);

	return ret;
}

static void signals_handler (int signum)
{
	if (signum == SIGINT)
	{
		if (mainloop && g_main_loop_is_running (mainloop))
		{
			g_main_loop_quit (mainloop);
		}
	}
}

int main (int argc, char *argv[])
{	
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	printf("libprotoversion: %s\n",google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION).c_str());

	signal (SIGTERM, signals_handler);

	gst_app_t *app = &gst_app;
	int ret = 0;
	errno = 0;
	byte ep_in_addr  = -2;
	byte ep_out_addr = -2;

	/* Start AA processing */
	ret = hu_aap_start (ep_in_addr, ep_out_addr);
	if (ret == -1)
	{
		printf("Phone switched to accessory mode. Attempting once more.\n");
		sleep(1);
		ret = hu_aap_start (ep_in_addr, ep_out_addr);
	}

	if (ret < 0) {
		if (ret == -2)
		{
			printf("Phone is not connected. Connect a supported phone and restart.\n");
			return 0;
		}
		else if (ret == -1)
			printf("Phone switched to accessory mode. Restart to enter AA mode.\n");
		else
			printf("hu_app_start() ret: %d\n", ret);
		return (ret);
	}

	printf("Starting Android Auto...\n");

	/* Init gstreamer pipeline */
	ret = gst_pipeline_init(app);
	if (ret < 0) {
		printf("gst_pipeline_init() ret: %d\n", ret);
		return (-4);
	}

	/* Open Touchscreen Device */
	mTouch.fd = open(EVENT_DEVICE_TS, O_RDONLY);

	if (mTouch.fd == -1) {
		fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_TS);
		return -3;
	}

	sendqueue = g_async_queue_new();

	pthread_t iput_thread;

	pthread_create(&iput_thread, NULL, &input_thread, (void *)app);

	pthread_t nm_thread;

	pthread_create(&nm_thread, NULL, &nightmode_thread, (void *)app);


	pthread_t mn_thread;

	pthread_create(&mn_thread, NULL, &main_thread, (void *)app);

	/* Start gstreamer pipeline and main loop */
	ret = gst_loop(app);
	if (ret < 0) {
		printf("gst_loop() ret: %d\n", ret);
		ret = -5;
	}

	/* Stop AA processing */
	ret = hu_aap_stop ();
	if (ret < 0) {
		printf("hu_aap_stop() ret: %d\n", ret);
		ret = -6;
	}

	close(mTouch.fd);

	pthread_cancel(nm_thread);
	pthread_cancel(mn_thread);
	pthread_cancel(iput_thread);
	system("kill -SIGUSR2 $(pgrep input_filter)");

	printf("END \n");

	return 0;
}
