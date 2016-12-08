
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

const char * state_get (int state) {
  switch (state) {
    case hu_STATE_INITIAL:                                           // 0
      return ("hu_STATE_INITIAL");
    case hu_STATE_STARTIN:                                           // 1
      return ("hu_STATE_STARTIN");
    case hu_STATE_STARTED:                                           // 2
      return ("hu_STATE_STARTED");
    case hu_STATE_STOPPIN:                                           // 3
      return ("hu_STATE_STOPPIN");
    case hu_STATE_STOPPED:                                           // 4
      return ("hu_STATE_STOPPED");
  }
  return ("hu_STATE Unknown error");
}


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


    // Buffers: Audio, Video, identical code, should generalize

#define aud_buf_BUFS_SIZE    65536 * 4      // Up to 256 Kbytes
int aud_buf_bufs_size = aud_buf_BUFS_SIZE;

#define   NUM_aud_buf_BUFS   16            // Maximum of NUM_aud_buf_BUFS - 1 in progress; 1 is never used
int num_aud_buf_bufs = NUM_aud_buf_BUFS;

char aud_buf_bufs [NUM_aud_buf_BUFS] [aud_buf_BUFS_SIZE];

int aud_buf_lens [NUM_aud_buf_BUFS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int aud_buf_buf_tail = 0;    // Tail is next index for writer to write to.   If head = tail, there is no info.
int aud_buf_buf_head = 0;    // Head is next index for reader to read from.

int aud_buf_errs = 0;
int aud_max_bufs = 0;
int aud_sem_tail = 0;
int aud_sem_head = 0;

char * aud_write_tail_buf_get (int len) {                          // Get tail buffer to write to

  if (len > aud_buf_BUFS_SIZE) {
    loge ("!!!!!!!!!! aud_write_tail_buf_get too big len: %d", len);   // E/aud_write_tail_buf_get(10699): !!!!!!!!!! aud_write_tail_buf_get too big len: 66338
    return (NULL);
  }

  int bufs = aud_buf_buf_tail - aud_buf_buf_head;
  if (bufs < 0)                                                       // If underflowed...
    bufs += num_aud_buf_bufs;                                         // Wrap
  //logd ("aud_write_tail_buf_get start bufs: %d  head: %d  tail: %d", bufs, aud_buf_buf_head, aud_buf_buf_tail);

  if (bufs > aud_max_bufs)                                            // If new maximum buffers in progress...
    aud_max_bufs = bufs;                                              // Save new max
  if (bufs >= num_aud_buf_bufs - 1) {                                 // If room for another (max = NUM_aud_buf_BUFS - 1)
    loge ("aud_write_tail_buf_get out of aud_buf_bufs");
    aud_buf_errs ++;
    //aud_buf_buf_tail = aud_buf_buf_head = 0;                        // Drop all buffers
    return (NULL);
  }

  int max_retries = 4;
  int retries = 0;
  for (retries = 0; retries < max_retries; retries ++) {
    aud_sem_tail ++;
    if (aud_sem_tail == 1)
      break;
    aud_sem_tail --;
    loge ("aud_sem_tail wait");
    ms_sleep (10);
  }
  if (retries >= max_retries) {
    loge ("aud_sem_tail could not be acquired");
    return (NULL);
  }

  if (aud_buf_buf_tail < 0 || aud_buf_buf_tail > num_aud_buf_bufs - 1)   // Protect
    aud_buf_buf_tail &= num_aud_buf_bufs - 1;

  aud_buf_buf_tail ++;

  if (aud_buf_buf_tail < 0 || aud_buf_buf_tail > num_aud_buf_bufs - 1)
    aud_buf_buf_tail &= num_aud_buf_bufs - 1;

  char * ret = aud_buf_bufs [aud_buf_buf_tail];
  aud_buf_lens [aud_buf_buf_tail] = len;

  //logd ("aud_write_tail_buf_get done  ret: %p  bufs: %d  tail len: %d  head: %d  tail: %d", ret, bufs, len, aud_buf_buf_head, aud_buf_buf_tail);

  aud_sem_tail --;

  return (ret);
}

char * aud_read_head_buf_get (int * len) {                              // Get head buffer to read from

  if (len == NULL) {
    loge ("!!!!!!!!!! aud_read_head_buf_get");
    return (NULL);
  }
  * len = 0;

  int bufs = aud_buf_buf_tail - aud_buf_buf_head;
  if (bufs < 0)                                                       // If underflowed...
    bufs += num_aud_buf_bufs;                                          // Wrap
  //logd ("aud_read_head_buf_get start bufs: %d  head: %d  tail: %d", bufs, aud_buf_buf_head, aud_buf_buf_tail);

  if (bufs <= 0) {                                                    // If no buffers are ready...
    if (ena_log_extra)
      logd ("aud_read_head_buf_get no aud_buf_bufs");
    //aud_buf_errs ++;  // Not an error; just no data
    //aud_buf_buf_tail = aud_buf_buf_head = 0;                          // Drop all buffers
    return (NULL);
  }

  int max_retries = 4;
  int retries = 0;
  for (retries = 0; retries < max_retries; retries ++) {
    aud_sem_head ++;
    if (aud_sem_head == 1)
      break;
    aud_sem_head --;
    loge ("aud_sem_head wait");
    ms_sleep (10);
  }
  if (retries >= max_retries) {
    loge ("aud_sem_head could not be acquired");
    return (NULL);
  }

  if (aud_buf_buf_head < 0 || aud_buf_buf_head > num_aud_buf_bufs - 1)   // Protect
    aud_buf_buf_head &= num_aud_buf_bufs - 1;

  aud_buf_buf_head ++;

  if (aud_buf_buf_head < 0 || aud_buf_buf_head > num_aud_buf_bufs - 1)
    aud_buf_buf_head &= num_aud_buf_bufs - 1;

  char * ret = aud_buf_bufs [aud_buf_buf_head];
  * len = aud_buf_lens [aud_buf_buf_head];

  //logd ("aud_read_head_buf_get done  ret: %p  bufs: %d  head len: %d  head: %d  tail: %d", ret, bufs, * len, aud_buf_buf_head, aud_buf_buf_tail);

  aud_sem_head --;

  return (ret);
}



#define vid_buf_BUFS_SIZE    65536 * 4      // Up to 256 Kbytes
int vid_buf_bufs_size = vid_buf_BUFS_SIZE;

#define   NUM_vid_buf_BUFS   16            // Maximum of NUM_vid_buf_BUFS - 1 in progress; 1 is never used
int num_vid_buf_bufs = NUM_vid_buf_BUFS;

char vid_buf_bufs [NUM_vid_buf_BUFS] [vid_buf_BUFS_SIZE];

int vid_buf_lens [NUM_vid_buf_BUFS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int vid_buf_buf_tail = 0;    // Tail is next index for writer to write to.   If head = tail, there is no info.
int vid_buf_buf_head = 0;    // Head is next index for reader to read from.

int vid_buf_errs = 0;
int vid_max_bufs = 0;
int vid_sem_tail = 0;
int vid_sem_head = 0;

char * vid_write_tail_buf_get (int len) {                          // Get tail buffer to write to

  if (len > vid_buf_BUFS_SIZE) {
    loge ("!!!!!!!!!! vid_write_tail_buf_get too big len: %d", len);   // E/vid_write_tail_buf_get(10699): !!!!!!!!!! vid_write_tail_buf_get too big len: 66338
    return (NULL);
  }

  int bufs = vid_buf_buf_tail - vid_buf_buf_head;
  if (bufs < 0)                                                       // If underflowed...
    bufs += num_vid_buf_bufs;                                         // Wrap
  //logd ("vid_write_tail_buf_get start bufs: %d  head: %d  tail: %d", bufs, vid_buf_buf_head, vid_buf_buf_tail);

  if (bufs > vid_max_bufs)                                            // If new maximum buffers in progress...
    vid_max_bufs = bufs;                                              // Save new max
  if (bufs >= num_vid_buf_bufs - 1) {                                 // If room for another (max = NUM_vid_buf_BUFS - 1)
    loge ("vid_write_tail_buf_get out of vid_buf_bufs");
    vid_buf_errs ++;
    //vid_buf_buf_tail = vid_buf_buf_head = 0;                        // Drop all buffers
    return (NULL);
  }

  int max_retries = 4;
  int retries = 0;
  for (retries = 0; retries < max_retries; retries ++) {
    vid_sem_tail ++;
    if (vid_sem_tail == 1)
      break;
    vid_sem_tail --;
    loge ("vid_sem_tail wait");
    ms_sleep (10);
  }
  if (retries >= max_retries) {
    loge ("vid_sem_tail could not be acquired");
    return (NULL);
  }

  if (vid_buf_buf_tail < 0 || vid_buf_buf_tail > num_vid_buf_bufs - 1)   // Protect
    vid_buf_buf_tail &= num_vid_buf_bufs - 1;

  vid_buf_buf_tail ++;

  if (vid_buf_buf_tail < 0 || vid_buf_buf_tail > num_vid_buf_bufs - 1)
    vid_buf_buf_tail &= num_vid_buf_bufs - 1;

  char * ret = vid_buf_bufs [vid_buf_buf_tail];
  vid_buf_lens [vid_buf_buf_tail] = len;

  //logd ("vid_write_tail_buf_get done  ret: %p  bufs: %d  tail len: %d  head: %d  tail: %d", ret, bufs, len, vid_buf_buf_head, vid_buf_buf_tail);

  vid_sem_tail --;

  return (ret);
}

char * vid_read_head_buf_get (int * len) {                              // Get head buffer to read from

  if (len == NULL) {
    loge ("!!!!!!!!!! vid_read_head_buf_get");
    return (NULL);
  }
  * len = 0;

  int bufs = vid_buf_buf_tail - vid_buf_buf_head;
  if (bufs < 0)                                                       // If underflowed...
    bufs += num_vid_buf_bufs;                                          // Wrap
  //logd ("vid_read_head_buf_get start bufs: %d  head: %d  tail: %d", bufs, vid_buf_buf_head, vid_buf_buf_tail);

  if (bufs <= 0) {                                                    // If no buffers are ready...
    if (ena_log_extra)
      logd ("vid_read_head_buf_get no vid_buf_bufs");
    //vid_buf_errs ++;  // Not an error; just no data
    //vid_buf_buf_tail = vid_buf_buf_head = 0;                          // Drop all buffers
    return (NULL);
  }

  int max_retries = 4;
  int retries = 0;
  for (retries = 0; retries < max_retries; retries ++) {
    vid_sem_head ++;
    if (vid_sem_head == 1)
      break;
    vid_sem_head --;
    loge ("vid_sem_head wait");
    ms_sleep (10);
  }
  if (retries >= max_retries) {
    loge ("vid_sem_head could not be acquired");
    return (NULL);
  }

  if (vid_buf_buf_head < 0 || vid_buf_buf_head > num_vid_buf_bufs - 1)   // Protect
    vid_buf_buf_head &= num_vid_buf_bufs - 1;

  vid_buf_buf_head ++;

  if (vid_buf_buf_head < 0 || vid_buf_buf_head > num_vid_buf_bufs - 1)
    vid_buf_buf_head &= num_vid_buf_bufs - 1;

  char * ret = vid_buf_bufs [vid_buf_buf_head];
  * len = vid_buf_lens [vid_buf_buf_head];

  //logd ("vid_read_head_buf_get done  ret: %p  bufs: %d  head len: %d  head: %d  tail: %d", ret, bufs, * len, vid_buf_buf_head, vid_buf_buf_tail);

  vid_sem_head --;

  return (ret);
}


