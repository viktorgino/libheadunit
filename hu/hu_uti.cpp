
  // Utilities: Used by many

//#ifndef UTILS_INCLUDED

//  #define UTILS_INCLUDED

//#define  GENERIC_CLIENT

#define LOGTAG "hu_uti"
#include "hu_uti.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>


#include <string.h>
#include <signal.h>

#include <pthread.h>

#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <dirent.h>                                                   // For opendir (), readdir (), closedir (), DIR, struct dirent.


int gen_server_loop_func (unsigned char * cmd_buf, int cmd_len, unsigned char * res_buf, int res_max);
int gen_server_poll_func (int poll_ms);

  // Log stuff:

int ena_log_extra   = 1;//1;//0;
int ena_log_verbo   = 1;//1;
int ena_log_debug   = 1;
int ena_log_warni   = 1;
int ena_log_error   = 1;

int ena_log_aap_send = 0;

// Enables for hex_dump:
int ena_hd_hu_aad_dmp = 1;        // Higher level
int ena_hd_tra_send   = 0;        // Lower  level
int ena_hd_tra_recv   = 0;

int ena_log_hexdu = 0;//1;    // Hex dump master enable
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
  time_t timet = time (NULL);
  const time_t * timep = & timet;
  char asc_time [512] = "";
  ctime_r (timep, asc_time);
  int len_time = strlen (asc_time);
  asc_time [len_time - 1] = 0;        // Remove trailing \n
  printf ("%s %s: %s:: %s\n", & asc_time [11], prio_get (prio), tag, log_line);
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
