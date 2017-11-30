
#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>

#include "../dbus/generated_cmu.h"

#define LOGTAG "mazda-gps"

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


static GPSLDSCLient *gps_client = NULL;

void mzd_gps2_start()
{
    if (gps_client != NULL)
        return;

    try
    {
        DBus::Connection gpservice_bus(SERVICE_BUS_ADDRESS, false);
        gpservice_bus.register_bus();
        gps_client = new GPSLDSCLient(gpservice_bus, "/com/jci/lds/data", "com.jci.lds.data");
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: Failed to connect to SERVICE bus %s: %s", error.name(), error.message());
        mzd_gps2_stop();
        return;
    }

    printf("GPS service connection established.\n");
}

bool mzd_gps2_get(GPSData& data)
{
    if (gps_client == NULL)
        return false;

    try
    {
        gps_client->GetPosition(data.positionAccuracy, data.uTCtime, data.latitude, data.longitude, data.altitude, data.heading, data.velocity, data.horizontalAccuracy, data.verticalAccuracy);
        logd("GPS data: %d %d %f %f %d %f %f %f %f   \n",data.positionAccuracy, data.uTCtime, data.latitude, data.longitude, data.altitude, data.heading, data.velocity, data.horizontalAccuracy, data.verticalAccuracy);
        return true;
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: GetPosition failed %s: %s", error.name(), error.message());
        return false;
    }
}

void mzd_gps2_stop()
{
    delete gps_client;
    gps_client = nullptr;
}
