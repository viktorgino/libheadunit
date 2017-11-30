#include <stdint.h>

struct GPSData
{
    int32_t positionAccuracy;
    uint64_t uTCtime;
    double latitude;
    double longitude;
    int32_t altitude;
    double heading;
    double velocity;
    double horizontalAccuracy;
    double verticalAccuracy;
};

void mzd_gps2_start();

bool mzd_gps2_get(GPSData& data);

void mzd_gps2_stop();

