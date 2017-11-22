
#define GPS_NO_VALUE -1
#define GPS_OK 0

void mzd_gps2_start();

int mzd_gps2_get(int32_t& p1, uint64_t& p2, double& p3, double& p4, int32_t& p5, double& p6, double& p7, double& p8, double& p9);

void mzd_gps2_stop();

