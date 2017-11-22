
#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "../dbus/generated_cmu.h"

#define LOGTAG "mazda-nm"

#include "hu_uti.h"

#include "mzd_gps.h"

#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

class GPSLDSCLient : public com::jci::lds::data_proxy,
                     public DBus::ObjectProxy
{
public:
    GPSLDSCLient(DBus::Connection &connection, const char *path, const char *name)
        : DBus::ObjectProxy(connection, path, name)
    {
    }

    virtual void GPSDiagnostics(const uint8_t& dTCId, const uint8_t& dTCAction) override {}
};


static DBus::Connection *gpservice_bus = NULL;
static GPSLDSCLient *gps_client = NULL;

void mzd_gps2_start() {
    if (gpservice_bus != NULL)
        return;

    try
    {
        gpservice_bus = new DBus::Connection(SERVICE_BUS_ADDRESS, false);
        gpservice_bus->register_bus();
        gps_client = new GPSLDSCLient(*gpservice_bus, "/com/jci/lds/data", "com.jci.lds.data");
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: Failed to connect to SERVICE bus %s: %s", error.name(), error.message());
        delete gps_client;
        delete gpservice_bus;
        gps_client = nullptr;
        gpservice_bus = nullptr;
        return;
    }

    printf("GPS service connection established.\n");
}

int mzd_gps2_get(int32_t& p1, uint64_t& p2, double& p3, double& p4, int32_t& p5, double& p6, double& p7, double& p8, double& p9) {
	
	int32_t  positionAccuracy;
	uint64_t uTCtime;
	double latitude, longitude;
	int32_t  altitude;
	double heading,  velocity, horizontalAccuracy,  verticalAccuracy;
	
	
    if (gpservice_bus == NULL) return
            GPS_NO_VALUE;

    try
    {
         gps_client->GetPosition(positionAccuracy, uTCtime, latitude, longitude, altitude, heading, velocity, horizontalAccuracy, verticalAccuracy);

		 p1 = positionAccuracy;
		 p2 = uTCtime;
		 p3 = latitude;
		 p4 = longitude;
		 p5 = altitude;
		 p6 = heading;
		 p7 = velocity;
		 p8 = horizontalAccuracy;
		 p9 = verticalAccuracy;

		 return GPS_OK;
  
//		 printf("GPS data: %d %d %f %f %d %f %f %f %f   \n",positionAccuracy, uTCtime, latitude, longitude, altitude, heading, velocity, horizontalAccuracy, verticalAccuracy);
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: GetPosition failed %s: %s", error.name(), error.message());
        return GPS_NO_VALUE;
    }
}

void mzd_gps2_stop() {
    delete gps_client;
    delete gpservice_bus;
    gps_client = nullptr;
    gpservice_bus = nullptr;
}
