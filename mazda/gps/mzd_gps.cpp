#include "mzd_gps.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>

#define LOGTAG "mzd-gps"
#include "hu_uti.h"

#include "minmea.h"

static bool running = false;

typedef struct {
    uint64_t timestamp;         // Epoch seconds
    double latitude;            // Degrees decimal
    double longitude;           // Degrees decimal
    int32_t bearing;                // Degress compass [0-360]
    double speed;               // Speed in km/h
    bool altitude_valid;
    double altitude;            // Altitude in m
} hu_location_t;

void process_gps(std::function<void(uint64_t, double, double, int, double, double)> location_cb) {

    // Wait for the rest of HU to properly start
    ms_sleep(500);

    FILE* fp;
    char* gps_line = NULL;
    size_t len = 0;
    ssize_t read;

    // GPS hardware is attached to ttymxc2 on CMU and communicates
    // via text NMEA protocol.
    fp = fopen("/dev/ttymxc2", "r");
    if (fp == NULL) return;

    // We get commands in batches so we need to watch to properly group them. 
    // We'll need RMC, GGA packets - RMC carries most data, GGA carries altitude. They should be sent 
    // as part of the same batch. 
    bool got_rmc = false;
    bool got_gga = false;

    hu_location_t pos;
    printf("GPS collection thread started.");

    while (running && ((read = getline(&gps_line, &len, fp)) != -1)) {
        switch(minmea_sentence_id(gps_line, false)) {
            case MINMEA_SENTENCE_RMC: {
                // Maybe we didn't get GGA before, dispatch the packet and continue.
                if (got_rmc) {
                    if (!got_gga) pos.altitude_valid = false;
                    location_cb(pos.timestamp, pos.latitude, pos.longitude, pos.bearing, pos.speed, pos.altitude);
                    memset(&pos, 0, sizeof(pos));
                    logd("Had to send partial location due to missing GGA!");
                }

                struct minmea_sentence_rmc frame;
                if (minmea_parse_rmc(&frame, gps_line)) {
                    // Check for zero values and skip them
                    if (frame.latitude.value == 0 || frame.longitude.value == 0) continue;

                    struct timespec ts;
                    minmea_gettime(&ts, &frame.date, &frame.time);
                    pos.timestamp = (ts.tv_sec * 1000L) + (ts.tv_nsec / 1000000L);

                    logd("RMC [TS: %ld Lat: %f Lng: %f Spd: %f Cour: %f]",
                        pos.timestamp, minmea_tofloat(frame.latitude), minmea_tofloat(frame.longitude), minmea_tofloat(frame.speed) * 1.852f, minmea_tofloat(frame.course));

                    pos.latitude = minmea_tocoord(&frame.latitude);
                    pos.longitude = minmea_tocoord(&frame.longitude);
                    pos.bearing = (int32_t)minmea_tofloat(&frame.course);
                    pos.speed = minmea_tofloat(&frame.speed);
                    pos.speed = ((double)pos.speed * 1.852);   // knots to kmh
                    got_rmc = true;
                }

            } break;
            case MINMEA_SENTENCE_GGA: {
                struct minmea_sentence_gga frame;
                if (minmea_parse_gga(&frame, gps_line)) {
                    if (frame.altitude_units == 'M') {
                        logd("GGA: [Alt: %f]", minmea_tofloat(frame.altitude));

                        pos.altitude_valid = true;
                        pos.altitude = minmea_rescale(&frame.altitude, 1E2);
                    }
                    got_gga = true;
                }
            } break;
        }

        if (got_gga && got_rmc) {
            location_cb(pos.timestamp, pos.latitude, pos.longitude, pos.bearing, pos.speed, pos.altitude);
            memset(&pos, 0, sizeof(pos));
            got_gga = false;
            got_rmc = false;
        }
    }

    fclose(fp);
}

void mzd_gps_start(std::function<void(uint64_t, double, double, int, double, double)> location_cb) {
    std::thread(process_gps, location_cb);
}

void mzd_gps_stop() {
    running = false;
}