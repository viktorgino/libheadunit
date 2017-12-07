
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
        if (data.uTCtime == 0 || data.positionAccuracy == 0 || data.horizontalAccuracy > 80)
            return false;

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

bool GPSData::IsSame(const GPSData& other) const
{
    if (uTCtime == 0 && other.uTCtime == 0)
        return true; //other members don't matter
    return positionAccuracy == other.positionAccuracy &&
            uTCtime == other.uTCtime &&
            int32_t(latitude * 1E7) == int32_t(other.latitude * 1E7) &&
            int32_t(longitude * 1E7) == int32_t(other.longitude * 1E7) &&
            altitude == other.altitude &&
            int32_t(heading * 1E7) == int32_t(other.heading * 1E7) &&
            int32_t(velocity * 1E7) == int32_t(other.velocity * 1E7) &&
            int32_t(horizontalAccuracy * 1E7) == int32_t(other.horizontalAccuracy * 1E7) &&
            int32_t(verticalAccuracy * 1E7) == int32_t(other.verticalAccuracy * 1E7);
}

