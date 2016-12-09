#pragma once

#include <cstdint>
#include <functional>

/** Starts GPS data collection thread. It'll read the serial port and call the callback on
    started thread with read location.
    Callback parameters: epoch timestamp, latitude, longitude, bearing, speed, altitude. **/
void mzd_gps_start(std::function<void(uint64_t, double, double, int, double, double)> location_cb);
/** Stop the GPS collection thread. **/
void mzd_gps_stop();