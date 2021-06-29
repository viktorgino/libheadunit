#pragma once

//#define NDEBUG // Ensure debug stuff

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include <string.h>
#include <signal.h>

#include <pthread.h>

#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <dirent.h>                                                   // For opendir (), readdir (), closedir (), DIR, struct dirent.

#include <libudev.h>
// Enables for hex_dump:
extern int ena_hd_hu_aad_dmp;
extern int ena_hd_tra_send;
extern int ena_hd_tra_recv;

extern int ena_log_aap_send;

extern int ena_log_extra;
extern int ena_log_verbo;

typedef unsigned char byte;

#define hu_LOG_EXT   1
#define hu_LOG_VER   2
#define hu_LOG_DEB   3
#define hu_LOG_WAR   5
#define hu_LOG_ERR   6

#ifdef NDEBUG

#define  logx(...)
#define  logv(...)
#define  logd(...)
#define  logw(...)
#define  loge(...)

#else

#define STR(s) STR2(s)
#define STR2(s) #s

#define  logx(...)  hu_log(hu_LOG_EXT,__FILE__ ":" STR(__LINE__),__FUNCTION__,__VA_ARGS__)
#define  logv(...)  hu_log(hu_LOG_VER,__FILE__ ":" STR(__LINE__),__FUNCTION__,__VA_ARGS__)
#define  logd(...)  hu_log(hu_LOG_DEB,__FILE__ ":" STR(__LINE__),__FUNCTION__,__VA_ARGS__)
#define  logw(...)  hu_log(hu_LOG_WAR,__FILE__ ":" STR(__LINE__),__FUNCTION__,__VA_ARGS__)
#define  loge(...)  hu_log(hu_LOG_ERR,__FILE__ ":" STR(__LINE__),__FUNCTION__,__VA_ARGS__)

#endif

int hu_log (int prio, const char * tag, const char * func, const char * fmt, ...);


unsigned long ms_sleep        (unsigned long ms);
void hex_dump                 (const char * prefix, int width, unsigned char * buf, int len);

void hu_log_library_versions();

