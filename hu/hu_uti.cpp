
  // Utilities: Used by many

//#ifndef UTILS_INCLUDED

//  #define UTILS_INCLUDED

//#define  GENERIC_CLIENT

#define LOGTAG "hu_uti"
#include "hu_uti.h"
#include "hu.pb.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <openssl/ssl.h>


#include <string.h>
#include <signal.h>

#include <pthread.h>

#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <dirent.h>                                                   // For opendir (), readdir (), closedir (), DIR, struct dirent.
#include <sys/utsname.h>
#include <libusb.h>
#include <execinfo.h>
#if CMU
#include "backtrace.h"
#else
#include <backtrace.h>
#endif

int gen_server_loop_func (unsigned char * cmd_buf, int cmd_len, unsigned char * res_buf, int res_max);
int gen_server_poll_func (int poll_ms);

  // Log stuff:

int ena_log_extra   = 0;//1;//0;
int ena_log_verbo   = 0;//1;
int ena_log_debug   = 0;
int ena_log_warni   = 1;
int ena_log_error   = 1;

int ena_log_aap_send = 0;

// Enables for hex_dump:
int ena_hd_hu_aad_dmp = 1;        // Higher level
int ena_hd_tra_send   = 0;        // Lower  level
int ena_hd_tra_recv   = 0;

int ena_log_hexdu = 1;//1;    // Hex dump master enable
int max_hex_dump  = 64;//32;


const  char * prio_get (int prio) {
  switch (prio) {
    case hu_LOG_EXT: return ("X");
    case hu_LOG_VER: return ("V");
    case hu_LOG_DEB: return ("D");
    case hu_LOG_WAR: return ("W");
    case hu_LOG_ERR: return ("E");
  }
  return ("?");
}

int hu_log (int prio, const char * tag, const char * func, const char * fmt, ...) {

  if (! ena_log_extra && prio == hu_LOG_EXT)
    return -1;
  if (! ena_log_verbo && prio == hu_LOG_VER)
    return -1;
  if (! ena_log_debug && prio == hu_LOG_DEB)
    return -1;
  if (! ena_log_warni && prio == hu_LOG_WAR)
    return -1;
  if (! ena_log_error && prio == hu_LOG_ERR)
    return -1;


  va_list ap;
  va_start (ap, fmt); 
#ifdef __ANDROID_API__
  char tag_str [512] = {0};
  snprintf (tag_str, sizeof (tag_str), "%32.32s", func);
  __android_log_vprint (prio, tag_str, fmt, ap);
#else
  char log_line [4096] = {0};
  va_list aq;
  va_start (aq, fmt); 
  int len = vsnprintf (log_line, sizeof (log_line), fmt, aq);
  //Time doesn't work on CMU anyway, always says 1970
  printf ("%s: %s: %s : %s\n", prio_get (prio), tag, func, log_line);
#endif

  // if (prio == hu_LOG_ERR)
  // {
  //   raise(SIGTRAP);
  // }

  return (0);
}

unsigned long ms_sleep (unsigned long ms) {
  usleep (ms * 1000L);
  return (ms);
}


#define HD_MW   256
void hex_dump (const char * prefix, int width, unsigned char * buf, int len) {
  if (0)//! strncmp (prefix, "AUDIO: ", strlen ("AUDIO: ")))
    len = len;
  else if (! ena_log_hexdu)
    return;
  //loge ("hex_dump prefix: \"%s\"  width: %d   buf: %p  len: %d", prefix, width, buf, len);

  if (buf == NULL || len <= 0)
    return;

  if (len > max_hex_dump)
    len = max_hex_dump;

  char tmp  [3 * HD_MW + 8] = "";                                     // Handle line widths up to HD_MW
  char line [3 * HD_MW + 8] = "";
  if (width > HD_MW)
    width = HD_MW;
  int i, n;
  line [0] = 0;

  if (prefix)
    //strlcpy (line, prefix, sizeof (line));
    strlcat (line, prefix, sizeof (line));

  snprintf (tmp, sizeof (tmp), " %8.8x ", 0);
  strlcat (line, tmp, sizeof (line));

  for (i = 0, n = 1; i < len; i ++, n ++) {                           // i keeps incrementing, n gets reset to 0 each line

    snprintf (tmp, sizeof (tmp), "%2.2x ", buf [i]);
    strlcat (line, tmp, sizeof (line));                               // Append 2 bytes hex and space to line

    if (n == width) {                                                 // If at specified line width
      n = 0;                                                          // Reset position in line counter
      logd (line);                                                    // Log line

      line [0] = 0;
      if (prefix)
        //strlcpy (line, prefix, sizeof (line));
        strlcat (line, prefix, sizeof (line));

      //snprintf (tmp, sizeof (tmp), " %8.8x ", i + 1);
      snprintf (tmp, sizeof (tmp), "     %4.4x ", i + 1);
      strlcat (line, tmp, sizeof (line));
    }
    else if (i == len - 1)                                            // Else if at last byte
      logd (line);                                                    // Log line
  }
}

void hu_log_library_versions()
{
  utsname sysinfo;
  if (uname(&sysinfo) < 0)
  {
    printf("uname failed\n");
  }
  else
  {
    printf("uname:\n sysname: %s\n release: %s\n version: %s\n machine: %s\n", sysinfo.sysname, sysinfo.release, sysinfo.version, sysinfo.machine);
  }
  printf("libprotoversion: %s\n",google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION).c_str());

  const libusb_version* usbversion = libusb_get_version();
  printf("libusb_get_version:\n");
  printf(" version: %u.%u.%u.%u\n", (unsigned int)usbversion->major, (unsigned int)usbversion->minor, (unsigned int)usbversion->micro, (unsigned int)usbversion->nano);
  printf(" rc: %s\n describe: %s\n", usbversion->rc, usbversion->describe);

  SSL_library_init();
  printf("openssl version: %s (%#010lx)\n", SSLeay_version(SSLEAY_VERSION), SSLeay());
}

static backtrace_state* g_bt_state = NULL;

static void crash_handler(int sig) 
{
  // print out all the frames to stderr
  printf("Error: signal %s:\n", strsignal(sig));

  //This would be nice, but it prints no names :(
  //backtrace_symbols_fd(array, size, STDOUT_FILENO);
  int skip = 2;
  backtrace_print(g_bt_state, skip, stdout);

  exit(1);
}

void hu_install_crash_handler()
{
  g_bt_state = backtrace_create_state(NULL, 1, NULL, NULL);
  signal(SIGSEGV, &crash_handler);
  signal(SIGILL, &crash_handler);
  signal(SIGFPE, &crash_handler);
  signal(SIGBUS, &crash_handler);
  signal(SIGSYS, &crash_handler);
  signal(SIGXCPU, &crash_handler);
  signal(SIGXFSZ, &crash_handler);
}