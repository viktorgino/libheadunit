#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
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
#include <algorithm>

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "hu_uti.h"
#include "hu_aap.h"

#include "nm/mzd_nightmode.h"
#include "gps/mzd_gps.h"

#include "audio.h"
#include "main.h"
#include "command_server.h"
#include "callbacks.h"
#include "glib_utils.h"

#define HMI_BUS_ADDRESS "unix:path=/tmp/dbus_hmi_socket"
#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

__asm__(".symver realpath1,realpath1@GLIBC_2.11.1");

gst_app_t gst_app;
IHUAnyThreadInterface* g_hu = nullptr;

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

static void gps_thread_func(std::condition_variable& quitcv2, std::mutex& quitmutex2)
{
    int gpsts;
    int32_t  positionAccuracy;
    uint64_t uTCtime;
    double latitude, longitude;
    int32_t  altitude;
    double heading,  velocity, horizontalAccuracy,  verticalAccuracy;
	
    mzd_gps2_start();
    while (true)
    {
                gpsts = mzd_gps2_get(positionAccuracy, uTCtime, latitude, longitude, altitude, heading, velocity, horizontalAccuracy, verticalAccuracy);

//		printf("GPS data: %d %d %f %f %d %f %f %f %f   \n",positionAccuracy, uTCtime, latitude, longitude, altitude, heading, velocity, horizontalAccuracy, verticalAccuracy);

		if (gpsts == GPS_OK)
		{	
	  	    g_hu->hu_queue_command([uTCtime, latitude, longitude, heading, velocity, altitude, horizontalAccuracy](IHUConnectionThreadInterface& s)
		    {
			HU::SensorEvent sensorEvent;
			HU::SensorEvent::LocationData* location = sensorEvent.add_location_data();
			location->set_timestamp(uTCtime);
			location->set_latitude(static_cast<int32_t>(latitude * 1E7));
			location->set_longitude(static_cast<int32_t>(longitude * 1E7));

			if (heading != 0) {
				location->set_bearing(static_cast<int32_t>(heading * 1E6));
			}

			// AA expects speed in knots, so convert back
			location->set_speed(static_cast<int32_t>((velocity / 1.852) * 1E3));

			if (altitude != 0) {
				location->set_altitude(static_cast<int32_t>(altitude * 1E2));
			}

			location->set_accuracy(static_cast<int32_t>(horizontalAccuracy * 1E3));

			s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
	            });
		}	
		
		
		
        {
            std::unique_lock<std::mutex> lk(quitmutex2);
            if (quitcv2.wait_for(lk, std::chrono::milliseconds(1000)) == std::cv_status::no_timeout)
            {
                break;
            }
        }
    }

    mzd_gps2_stop();
}




int main (int argc, char *argv[])
{
    //Force line-only buffering so we can see the output during hangs
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    hu_log_library_versions();
    hu_install_crash_handler();

    DBus::_init_threading();

    gst_init(&argc, &argv);

    try
    {
        MazdaCommandServerCallbacks commandCallbacks;
        CommandServer commandServer(commandCallbacks);
        if (!commandServer.Start())
        {
            loge("Command server failed to start");
            return 1;
        }

        if (argc >= 2 && strcmp(argv[1], "test") == 0)
        {
            //test mode from the installer, if we got here it's ok
            printf("###TESTMODE_OK###\n");
            return 0;
        }

        printf("Looping\n");
        while (true)
        {
            //Make a new one instead of using the default so we can clean it up each run
            run_on_thread_main_context = g_main_context_new();
            //Recreate this each time, it makes the error handling logic simpler
            DBus::Glib::BusDispatcher dispatcher;
            dispatcher.attach(run_on_thread_main_context);
            printf("DBus::Glib::BusDispatcher attached\n");

            DBus::default_dispatcher = &dispatcher;

            printf("Making debug connections\n");
            DBus::Connection hmiBus(HMI_BUS_ADDRESS, false);
            hmiBus.register_bus();

            DBus::Connection serviceBus(SERVICE_BUS_ADDRESS, false);
            serviceBus.register_bus();

            MazdaEventCallbacks callbacks(serviceBus, hmiBus);
            HUServer headunit(callbacks);
            g_hu = &headunit.GetAnyThreadInterface();
            commandCallbacks.eventCallbacks = &callbacks;

            //Wait forever for a connection
            int ret = headunit.hu_aap_start(HU_TRANSPORT_TYPE::USB, true);
            if (ret < 0) {
                loge("Something bad happened");
                continue;
            }

            gst_app.loop = g_main_loop_new(run_on_thread_main_context, FALSE);
            callbacks.connected = true;

            std::condition_variable quitcv;
            std::mutex quitmutex;

            std::thread nm_thread([&quitcv, &quitmutex](){ nightmode_thread_func(quitcv, quitmutex); } );

            std::condition_variable quitcv2;
            std::mutex quitmutex2;

            std::thread gp_thread([&quitcv2, &quitmutex2](){ gps_thread_func(quitcv2, quitmutex2); } );			
			
            /* Start gstreamer pipeline and main loop */



            printf("Starting Android Auto...\n");

            g_main_loop_run (gst_app.loop);

            commandCallbacks.eventCallbacks = nullptr;

            callbacks.connected = false;
            callbacks.videoFocus = false;
            callbacks.audioFocus = AudioManagerClient::FocusType::NONE;

            printf("quitting...\n");
            //wake up night mode polling thread
            quitcv.notify_all();
			
			//wake up GPS polling thread
			
			quitcv2.notify_all();

            printf("waiting for nm_thread\n");
            nm_thread.join();

            printf("waiting for gps_thread\n");
			gp_thread.join();

            printf("shutting down\n");

            g_main_loop_unref(gst_app.loop);
            gst_app.loop = nullptr;

            /* Stop AA processing */
            ret = headunit.hu_aap_shutdown();
            if (ret < 0) {
                printf("hu_aap_shutdown() ret: %d\n", ret);
                return ret;
            }

            g_main_context_unref(run_on_thread_main_context);
            run_on_thread_main_context = nullptr;
            g_hu = nullptr;
            DBus::default_dispatcher = nullptr;
        }
    }
    catch(DBus::Error& error)
    {
        loge("DBUS Error: %s: %s", error.name(), error.message());
        return 1;
    }

    return 0;
}
