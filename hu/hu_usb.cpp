
//

#define LOGTAG "hu_usb"
#include "hu_uti.h"                                                  // Utilities
#include "hu_usb.h"
#include <vector>
#include <algorithm>
int iusb_state = 0; // 0: Initial    1: Startin    2: Started    3: Stoppin    4: Stopped


#ifdef __ANDROID_API__
#include <libusb.h>
#else
#include <libusb.h>
#endif

//  #ifdef __ANDROID_API__
#ifndef LIBUSB_LOG_LEVEL_NONE                 
#define LIBUSB_LOG_LEVEL_NONE     0
#endif
#ifndef LIBUSB_LOG_LEVEL_ERROR
#define LIBUSB_LOG_LEVEL_ERROR    1
#endif
#ifndef LIBUSB_LOG_LEVEL_WARNING
#define LIBUSB_LOG_LEVEL_WARNING  2
#endif
#ifndef LIBUSB_LOG_LEVEL_INFO
#define LIBUSB_LOG_LEVEL_INFO     3
#endif
#ifndef LIBUSB_LOG_LEVEL_DEBUG
#define LIBUSB_LOG_LEVEL_DEBUG    4
#endif
//  #endif


// Accessory/ADB/Audio

//    char AAP_VAL_MAN [31] = "Android";
//    char AAP_VAL_MOD [97] = "Android Auto";    // "Android Open Automotive Protocol"


static char AAP_VAL_MAN[] =  "Android";
static char AAP_VAL_MOD[] =  "Android Auto";    // "Android Open Automotive Protocol"
//#define AAP_VAL_DES   "Description"
//#define AAP_VAL_VER   "VersionName"
//#define AAP_VAL_URI   "https://developer.android.com/auto/index.html"
//#define AAP_VAL_SER   "62skidoo"

#define ACC_IDX_MAN   0   // Manufacturer
#define ACC_IDX_MOD   1   // Model
//#define ACC_IDX_DES   2   // Description
//#define ACC_IDX_VER   3   // Version
//#define ACC_IDX_URI   4   // URI
//#define ACC_IDX_SER   5   // Serial Number

#define ACC_REQ_GET_PROTOCOL        51
#define ACC_REQ_SEND_STRING         52
#define ACC_REQ_START               53

//#define ACC_REQ_REGISTER_HID        54
//#define ACC_REQ_UNREGISTER_HID      55
//#define ACC_REQ_SET_HID_REPORT_DESC 56
//#define ACC_REQ_SEND_HID_EVENT      57
#define ACC_REQ_AUDIO               58

#define USB_SETUP_HOST_TO_DEVICE                0x00    // transfer direction - host to device transfer   = USB_DIR_OUT (Output from host)
#define USB_SETUP_DEVICE_TO_HOST                0x80    // transfer direction - device to host transfer   = USB_DIR_IN  (Input  to   host)

//#define USB_SETUP_TYPE_STANDARD                 0x00    // type - standard
//#define USB_SETUP_TYPE_CLASS                    0x20    // type - class
#define USB_SETUP_TYPE_VENDOR                   0x40    // type - vendor   = USB_TYPE_VENDOR

#define USB_SETUP_RECIPIENT_DEVICE              0x00    // recipient - device
//#define USB_SETUP_RECIPIENT_INTERFACE           0x01    // recipient - interface
//#define USB_SETUP_RECIPIENT_ENDPOINT            0x02    // recipient - endpoint
//#define USB_SETUP_RECIPIENT_OTHER               0x03    // recipient - other


struct usbvpid {
  uint16_t vendor;
  uint16_t product;
};

const char * iusb_error_get (int error) {
  #if CMU
   switch (error) 
      { 
      case LIBUSB_SUCCESS: 
              return "Success"; 
      case LIBUSB_ERROR_IO: 
              return "Input/output error"; 
      case LIBUSB_ERROR_INVALID_PARAM: 
              return "Invalid parameter"; 
      case LIBUSB_ERROR_ACCESS: 
              return "Access denied (insufficient permissions)"; 
      case LIBUSB_ERROR_NO_DEVICE: 
              return "No such device (it may have been disconnected)"; 
      case LIBUSB_ERROR_NOT_FOUND: 
              return "Entity not found"; 
      case LIBUSB_ERROR_BUSY: 
              return "Resource busy"; 
      case LIBUSB_ERROR_TIMEOUT: 
              return "Operation timed out"; 
      case LIBUSB_ERROR_OVERFLOW: 
              return "Overflow"; 
      case LIBUSB_ERROR_PIPE: 
              return "Pipe error"; 
      case LIBUSB_ERROR_INTERRUPTED: 
              return "System call interrupted (perhaps due to signal)"; 
      case LIBUSB_ERROR_NO_MEM: 
              return "Insufficient memory"; 
      case LIBUSB_ERROR_NOT_SUPPORTED: 
              return "Operation not supported or unimplemented on this platform"; 
      case LIBUSB_ERROR_OTHER: 
              return "Other error"; 
      } 
      return "Unknown error"; 
  #else
  return libusb_strerror((libusb_error)error);
  #endif
}

int HUTransportStreamUSB::Write(const byte * buf, int len, int tmo) {

  int bytes_xfrd = 0;
  int usb_err = libusb_bulk_transfer(iusb_dev_hndl, iusb_ep_out, (byte*)buf, len, &bytes_xfrd, tmo);
  const char* dir = "send";
    //unsigned long ms_duration = ms_get () - ms_start;
  if (usb_err < 0 )
    loge ("Done dir: %s  len: %d  bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));
  else if (ena_log_verbo && usb_err < 0)// && (ena_hd_tra_send || ep == iusb_ep_in))
    logd ("Done dir: %s  len: %d  bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));
  else if (ena_log_extra)
    logw ("Done dir: %s  len: %d  bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));

  if (bytes_xfrd <= 0 && usb_err < 0) {

    loge ("Done dir: %s  len: %d  bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));

    return (-1);
  }

  return (bytes_xfrd);
}

static int iusb_control_transfer (libusb_device_handle * usb_hndl, uint8_t req_type, uint8_t req_val, uint16_t val, uint16_t idx, byte * buf, uint16_t len, unsigned int tmo) {

  //if (ena_log_verbo)
    logd ("Start usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_hndl, req_type, req_val, val, idx, buf, len, tmo);

  int usb_err = libusb_control_transfer (usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
  if (usb_err < 0) {
    loge ("Error usb_err: %d (%s)  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, iusb_error_get (usb_err), usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
    return (-1);
  }
  //if (ena_log_verbo)
    logd ("Done usb_err: %d  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
  return (0);
}


int HUTransportStreamUSB::iusb_oap_start () {
  byte oap_protocol_data [2] = {0, 0};
  int oap_protocol_level   = 0;

  uint8_t       req_type= USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE;
  uint8_t       req_val = ACC_REQ_GET_PROTOCOL;
  uint16_t      val     = 0;
  uint16_t      idx     = 0;
  byte *        data    = oap_protocol_data;
  uint16_t      len     = sizeof (oap_protocol_data);
  unsigned int  tmo     = 1000;
                                                                      // Get OAP Protocol level
  val = 0;
  idx = 0;
  int usb_err = iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, idx, data, len, tmo);
  if (usb_err != 0) {
    loge ("Done iusb_control_transfer usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    return (-1);
  }

  logd ("Done iusb_control_transfer usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

  oap_protocol_level = data [1] << 8 | data [0];
  if (oap_protocol_level < 2) {
    loge ("Error oap_protocol_level: %d", oap_protocol_level);
    return (-1);
  }
  logd ("oap_protocol_level: %d", oap_protocol_level);
  
  ms_sleep (50);//1);                                                        // Sometimes hangs on the next transfer

//if (iusb_best_vendor == USB_VID_GOO)
//  {logd ("Done USB_VID_GOO 1"); return (0); }

  req_type = USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE;
  req_val = ACC_REQ_SEND_STRING;

  iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_MAN,  (byte*)AAP_VAL_MAN, strlen (AAP_VAL_MAN) + 1, tmo);
  iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_MOD,  (byte*)AAP_VAL_MOD, strlen (AAP_VAL_MOD) + 1, tmo);
  //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_DES, AAP_VAL_DES, strlen (AAP_VAL_DES) + 1, tmo);
  //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_VER, AAP_VAL_VER, strlen (AAP_VAL_VER) + 1, tmo);
  //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_URI, AAP_VAL_URI, strlen (AAP_VAL_URI) + 1, tmo);
  //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_SER, AAP_VAL_SER, strlen (AAP_VAL_SER) + 1, tmo);

  logd ("Accessory IDs sent");

//if (iusb_best_vendor == USB_VID_GOO)
//  {logd ("Done USB_VID_GOO 2"); return (0); }

/*
  req_val = ACC_REQ_AUDIO;
  val = 1;                                                            // 0 for no audio (default),    1 for 2 channel, 16-bit PCM at 44100 KHz
  idx = 0;
  if (iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, idx, NULL, 0, tmo) < 0) {
    loge ("Error Audio mode request sent");
    return (-1);
  }
  logd ("OK Audio mode request sent");
*/
  req_val = ACC_REQ_START;
  val = 0;
  idx = 0;
  if (iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, idx, NULL, 0, tmo) < 0) {
    loge ("Error Accessory mode start request sent");
    return (-1);
  }
  logd ("OK Accessory mode start request sent");

  return (0);
}

int HUTransportStreamUSB::iusb_deinit () {                                              // !!!! Need to better reset and wait a while to kill transfers in progress and auto-restart properly

  //if (ctrl_transfer != NULL)
  //  libusb_free_transfer (ctrl_transfer);                           // Free all transfers individually

  if (iusb_dev_hndl != NULL) {
     int usb_err = libusb_release_interface (iusb_dev_hndl, 0);          // Can get a crash inside libusb_release_interface()
    if (usb_err != 0)
      loge ("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    else
      logd ("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

    libusb_reset_device (iusb_dev_hndl);

    usb_err = libusb_attach_kernel_driver (iusb_dev_hndl, 0);
    logd ("Done libusb_attach_kernel_driver. Failing here is ok if no kernel driver usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));


    libusb_close (iusb_dev_hndl);
    logd ("Done libusb_close");
    iusb_dev_hndl = NULL;
  }

  if (iusb_ctx)
  {
    libusb_exit (iusb_ctx); // Put here or can get a crash from pulling cable
    iusb_ctx = nullptr;
  }

  logd ("Done");

  return (0);
}


static struct usbvpid iusb_vendor_get (libusb_device * device) {

  struct usbvpid dev = {0,0};

  if (device == NULL)
    return dev;

  struct libusb_device_descriptor desc = {0};

  int usb_err = libusb_get_device_descriptor (device, & desc);
  if (usb_err != 0) {
    loge ("Error usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    return dev;
  }
      
  dev.vendor  = desc.idVendor;
  dev.product = desc.idProduct;
  logd ("Done usb_err: %d  vendor:product = 0x%04x:0x%04x", usb_err, dev.vendor, dev.product);
  return (dev);
}

static int iusb_vendor_priority_get (int vendor) {
  if (vendor == USB_VID_GOO)
    return (10);
  if (vendor == USB_VID_HTC)
    return (9);
  if (vendor == USB_VID_MOT)
    return (8);
  if (vendor == USB_VID_SAM)
    return (7);
  if (vendor == USB_VID_SON)
    return (6);
  if (vendor == USB_VID_LGE)
    return (5);
  if (vendor == USB_VID_LGD)
    return (5);
  if (vendor == USB_VID_ACE)
    return (5);
  if (vendor == USB_VID_HUA)
    return (5);
  if (vendor == USB_VID_PAN)
    return (5);
  if (vendor == USB_VID_ZTE)
    return (5);
  if (vendor == USB_VID_GAR)      
    return (5);
  if (vendor == USB_VID_O1A)
    return (4);
  if (vendor == USB_VID_QUA)
    return (3);
  if (vendor == USB_VID_ONE)
    return (5);
  if (vendor == USB_VID_XIA)
    return (5);
  if (vendor == USB_VID_ASU)
    return (5);
  if (vendor == USB_VID_MEI)
    return (5);      
  if (vendor == USB_VID_LEN)
    return (5);      
  if (vendor == USB_VID_OPO)
    return (5);      
  if (vendor == USB_VID_LEE)
    return (5);      
  if (vendor == USB_VID_ZUK)
    return (5);      
  if (vendor == USB_VID_BB)
    return (5);      
  if (vendor == USB_VID_BQ)
    return (5);      

  return (0);
}


int HUTransportStreamUSB::iusb_init (byte ep_in_addr, byte ep_out_addr) {
  logd ("ep_in_addr: %d  ep_out_addr: %d", ep_in_addr, ep_out_addr);

  iusb_ep_in  = -1;
  iusb_ep_out = -1;
  iusb_best_device = NULL;
  iusb_best_vendor = 0;

  int usb_err = libusb_init (&iusb_ctx);
  if (usb_err < 0) {
    loge ("Error libusb_init usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    return (-1);
  }
  logd ("OK libusb_init usb_err: %d", usb_err);

  libusb_set_debug (iusb_ctx, LIBUSB_LOG_LEVEL_INFO);    // DEBUG);//
  logd ("Done libusb_set_debug");

  libusb_device ** list;
  usb_err = libusb_get_device_list (iusb_ctx, & list);                // Get list of USB devices
  if (usb_err < 0) {
    loge ("Error libusb_get_device_list cnt: %d", usb_err, iusb_error_get (usb_err));
    return (-1);
  }
  ssize_t cnt = usb_err;
  logd ("Done libusb_get_device_list cnt: %d", cnt);
  int idx = 0;
  int iusb_best_vendor_priority = 0;

  libusb_device * device;
  for (idx = 0; idx < cnt; idx ++) {                                  // For all USB devices...
    device = list [idx];
    struct usbvpid dev = iusb_vendor_get (device); 
    int vendor = dev.vendor;
    int product = dev.product;
    printf ("iusb_vendor_get vendor: 0x%04x  device: %p \n", vendor, device);
    if (vendor) {
      int vendor_priority = iusb_vendor_priority_get (vendor);
      //if (iusb_best_vendor_priority <  vendor_priority) {  // For first
      if (iusb_best_vendor_priority < vendor_priority) {  // For last
        iusb_best_vendor_priority = vendor_priority;
        iusb_best_vendor = vendor;
        iusb_best_product = product;
        iusb_best_device = device;
        strncpy (iusb_best_man, iusb_curr_man, sizeof (iusb_best_man));
        strncpy (iusb_best_pro, iusb_curr_pro, sizeof (iusb_best_pro));
      }
    }
  }

  
  if (iusb_best_vendor == 0 || iusb_best_device == NULL) {                                             // If no vendor...
    printf ("Error device not found iusb_best_vendor: 0x%04x  iusb_best_device: %p \n", iusb_best_vendor, iusb_best_device);
    libusb_free_device_list (list, 1);                                // Free device list now that we are finished with it
    return (-2);
  }
  printf ("Device found iusb_best_vendor: 0x%04x  iusb_best_device: %p  iusb_best_man: \"%s\"  iusb_best_pro: \"%s\" \n", iusb_best_vendor, iusb_best_device, iusb_best_man, iusb_best_pro);


  usb_err = libusb_open (iusb_best_device, & iusb_dev_hndl);
  logd ("libusb_open usb_err: %d (%s)  iusb_dev_hndl: %p  list: %p", usb_err, iusb_error_get (usb_err), iusb_dev_hndl, list);

  libusb_free_device_list (list, 1);                                  // Free device list now that we are finished with it

  if (usb_err != 0) {
    loge ("Error libusb_open usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    return (-1);
  }
  logd ("Done libusb_open iusb_dev_hndl: %p", iusb_dev_hndl);


  usb_err = libusb_detach_kernel_driver (iusb_dev_hndl, 0);
  logd ("Done libusb_detach_kernel_driver. Failing here is ok if no kernel driver. usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

  usb_err = libusb_claim_interface (iusb_dev_hndl, 0);
  if (usb_err) {
    loge ("Error libusb_claim_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    return (-1);
  }

  logd ("OK libusb_claim_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

  struct libusb_config_descriptor * config = NULL;
  usb_err = libusb_get_config_descriptor (iusb_best_device, 0, & config);
  if (usb_err != 0) {
    logd ("Expected Error libusb_get_config_descriptor usb_err: %d (%s)  errno: %d (%s)", usb_err, iusb_error_get (usb_err), errno, strerror (errno));    // !! ???? Normal error now ???
    //return (-1);

    if (ep_in_addr == 255) {// && ep_out_addr == 0)
      iusb_ep_in  = 129;                                  // Set  input endpoint
      iusb_ep_out =   2;                                  // Set output endpoint
      return (0);
    }

    iusb_ep_in  = ep_in_addr; //129;                                  // Set  input endpoint
    iusb_ep_out = ep_out_addr;//  2;                                  // Set output endpoint
    return (0);
  }

  //if (ep_in_addr == 255 && ep_out_addr == 0) {    // If USB forced


  int num_int = config->bNumInterfaces;                               // Get number of interfaces
  logd ("Done get_config_descriptor config: %p  num_int: %d", config, num_int);

  const struct libusb_interface            * inter;
  const struct libusb_interface_descriptor * interdesc;
  const struct libusb_endpoint_descriptor  * epdesc;

  for (idx = 0; idx < num_int; idx ++) {                              // For all interfaces...
    inter = & config->interface [idx];
    int num_altsetting = inter->num_altsetting;
    logd ("num_altsetting: %d", num_altsetting);
    int j = 0;
    for (j = 0; j < inter->num_altsetting; j ++) {                    // For all alternate settings...
      interdesc = & inter->altsetting [j];
      int num_int = interdesc->bInterfaceNumber;
      logd ("num_int: %d", num_int);
      int num_eps = interdesc->bNumEndpoints;
      logd ("num_eps: %d", num_eps);
      int k = 0;
      for (k = 0; k < num_eps; k ++) {                                // For all endpoints...
        epdesc = & interdesc->endpoint [k];
        if (epdesc->bDescriptorType == LIBUSB_DT_ENDPOINT) {          // 5
          if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
            int ep_add = epdesc->bEndpointAddress;
            if (ep_add & LIBUSB_ENDPOINT_DIR_MASK) {
              if (iusb_ep_in < 0) {
                iusb_ep_in = ep_add;                                   // Set input endpoint
                logd ("iusb_ep_in: 0x%02x", iusb_ep_in);
              }
            }
            else {
              if (iusb_ep_out < 0) {
                iusb_ep_out = ep_add;                                  // Set output endpoint
                logd ("iusb_ep_out: 0x%02x", iusb_ep_out);
              }
            }
            if (iusb_ep_in > 0 && iusb_ep_out > 0) {                    // If we have both endpoints now...
              libusb_free_config_descriptor (config);

              if (ep_in_addr != 255 && (iusb_ep_in != ep_in_addr || iusb_ep_out != ep_out_addr)) {
                logw ("MISMATCH Done endpoint search iusb_ep_in: 0x%02x  iusb_ep_out: 0x%02x  ep_in_addr: 0x%02x  ep_out_addr: 0x%02x", iusb_ep_in, iusb_ep_out, ep_in_addr, ep_out_addr);    // Favor libusb over passed in
              }
              else
                logd ("Match Done endpoint search iusb_ep_in: 0x%02x  iusb_ep_out: 0x%02x  ep_in_addr: 0x%02x  ep_out_addr: 0x%02x", iusb_ep_in, iusb_ep_out, ep_in_addr, ep_out_addr);

              return (0);
            }
          }
        }
      }
    }
  }
                                                                      // Else if DON'T have both endpoints...
  loge ("Error in and/or out endpoints unknown iusb_ep_in: 0x%02x  iusb_ep_out: 0x%02x", iusb_ep_in, iusb_ep_out);
  libusb_free_config_descriptor (config);

  if (iusb_ep_in == -1)
    iusb_ep_in  = ep_in_addr; //129;                                  // Set  input endpoint
  if (iusb_ep_out == -1)
    iusb_ep_out = ep_out_addr;//  2;                                  // Set output endpoint

  if (iusb_ep_in == -1 || iusb_ep_out == -1)
    return (-1);

  loge ("!!!!! FIXED EP's !!!!!");
  return (0);
}

HUTransportStreamUSB::HUTransportStreamUSB()
{

}

HUTransportStreamUSB::~HUTransportStreamUSB()
{
  if (iusb_state != hu_STATE_STOPPED)
  {
    Stop();
  }
}

int HUTransportStreamUSB::Stop() {
  iusb_state = hu_STATE_STOPPIN;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));

  
  close(readfd);
  close(pipe_write_fd);
  readfd = -1;
  pipe_write_fd = -1;

  close(errorfd);
  close(error_write_fd);
  errorfd = -1;
  error_write_fd = -1;

  int ret = iusb_deinit ();

  if (usb_recv_thread.joinable())
  {
    usb_recv_thread.join();
  }

  iusb_state = hu_STATE_STOPPED;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
  return (ret);
}

void HUTransportStreamUSB::usb_recv_thread_main()
{
  pthread_setname_np(pthread_self(), "usb_recv_thread_main");
  while(!abort_usbthread)
  {
    int iusb_state = libusb_handle_events_completed(iusb_ctx, nullptr);
    if (iusb_state || abort_usbthread)
    {
      break;
    }

    size_t bytesToWrite = pipe_write_overflow.size();
    if (bytesToWrite > 0)
    {
      unsigned char *buffer = pipe_write_overflow.data();
      size_t written = 0;
     
      ssize_t ret = 0;
      errno = 0;
      while (bytesToWrite > 0 && (ret = write(pipe_write_fd, buffer, bytesToWrite) >= 0))
      {
        //logd("Wrote %d of %d bytes", ret, transfer->actual_length);
        buffer += ret;
        bytesToWrite -= ret;
        written += ret;
      }

      pipe_write_overflow.erase(pipe_write_overflow.begin(), pipe_write_overflow.begin() + written);
      if (ret < 0)
      {
        if (errno == EAGAIN)
        {
          //ignore this here
        }
        else
        {
          loge("libusb_callback: write failed");
          abort_usbthread = true;
          return;
        }
      }
      else
      {
        //done start a new read
        start_usb_recv();
      }
    }
  }
  logw("libusb_handle_events_completed: %d (%s)", iusb_state, state_get (iusb_state));

  if (abort_usbthread)
  {
    logw("USB thread exit");
  }
  else
  {
    loge("libusb_callback read failed");
  }
  //Wake up the reader if required
  int errData = -1;
  if (write(error_write_fd, &errData, sizeof(errData)) < 0)
  {
    if (!abort_usbthread)
    {
      loge("Writing error data failed");
    }
  }

}

void HUTransportStreamUSB::libusb_callback(libusb_transfer *transfer)
{
  logd("libusb_callback %d %d %d", transfer->status, LIBUSB_TRANSFER_COMPLETED, LIBUSB_TRANSFER_OVERFLOW);
  libusb_transfer_status recv_last_status = transfer->status;
  if (recv_last_status == LIBUSB_TRANSFER_COMPLETED || recv_last_status == LIBUSB_TRANSFER_OVERFLOW)
  {
    if (recv_last_status == LIBUSB_TRANSFER_OVERFLOW)
    {
      logw("LIBUSB_TRANSFER_OVERFLOW");
      recv_temp_buffer.resize(recv_temp_buffer.size() * 2);
      start_usb_recv();
    }
    else
    {
      size_t bytesToWrite = transfer->actual_length;
      unsigned char *buffer = transfer->buffer;
      
      ssize_t ret = 0;
      errno = 0;
      while (bytesToWrite > 0)
      {
        ret = write(pipe_write_fd, buffer, bytesToWrite);
        if (ret < 0)
          break;
        logd("Wrote %d of %d bytes", ret, transfer->actual_length);
        buffer += ret;
        bytesToWrite -= ret;
      }
      if (ret < 0)
      {
        if (errno == EAGAIN)
        {
          size_t cur =  pipe_write_overflow.size();
          pipe_write_overflow.resize(cur + bytesToWrite);
          memcpy(&pipe_write_overflow[cur], buffer, bytesToWrite);
          loge("Overflow %d", bytesToWrite);
        }
        else
        {
          loge("libusb_callback: write failed");
          abort_usbthread = true;
        }
      }
      else
      {
        start_usb_recv();
      }
    }
  }
  else
  {
    loge("libusb_callback: abort");
    abort_usbthread = true;
  }
  libusb_free_transfer(transfer);
}

int HUTransportStreamUSB::start_usb_recv()
{
    libusb_transfer *transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(transfer, iusb_dev_hndl, iusb_ep_in, 
      recv_temp_buffer.data(), recv_temp_buffer.size(), &libusb_callback_tramp, this, 0);

    int iusb_state = libusb_submit_transfer(transfer);
    if (iusb_state < 0)
    {
      loge("  Failed: libusb_submit_transfer: %d (%s)", iusb_state, iusb_error_get (iusb_state));
      libusb_free_transfer(transfer);
    }
    else
    {
      logd(" libusb_submit_transfer for %d bytes", recv_temp_buffer.size());
    }
    return iusb_state;
}

void HUTransportStreamUSB::libusb_callback_tramp(libusb_transfer *transfer)
{
  reinterpret_cast<HUTransportStreamUSB*>(transfer->user_data)->libusb_callback(transfer);
}

int HUTransportStreamUSB::Start(byte ep_in_addr, byte ep_out_addr) {
  int ret = 0;

  if (iusb_state == hu_STATE_STARTED) {
    logd ("CHECK: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
    return (0);
  }

  //#include <sys/prctl.h>
  //ret = prctl (PR_GET_DUMPABLE, 1, 0, 0, 0);  // 1 = SUID_DUMP_USER
  //logd ("prctl ret: %d", ret);

  iusb_state = hu_STATE_STARTIN;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));

  iusb_best_vendor = 0;
  int tries = 0;
  while (iusb_best_vendor != USB_VID_GOO  && (iusb_best_product < 0x2d00 || iusb_best_product > 0x2d05) && tries ++ < 4) { //2000) {

    ret = iusb_init (ep_in_addr, ep_out_addr);
    if (ret < 0) {
      loge ("Error iusb_init");
      iusb_deinit ();
      iusb_state = hu_STATE_STOPPED;
      logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
      return (ret);
    }
    logd ("OK iusb_init");

    printf("SHAI1 iusb_best_product id %d \n", iusb_best_product);

    if (iusb_best_vendor == USB_VID_GOO && (iusb_best_product >= 0x2d00 && iusb_best_product <= 0x2d05) ) {
//      if (iusb_best_vendor == USB_VID_GOO ) {
      logd ("Already OAP/AA mode, no need to call iusb_oap_start()");

      iusb_state = hu_STATE_STARTED;
      logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
      break;
    } 

    ret = iusb_oap_start ();
    if (ret < 0) {
      loge ("Error iusb_oap_start");
      iusb_deinit ();
      iusb_state = hu_STATE_STOPPED;
      logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
      return (-2);
    }
    logd ("OK iusb_oap_start");

    if (iusb_best_vendor != USB_VID_GOO && (iusb_best_product < 0x2d00 || iusb_best_product > 0x2d05)) {
      iusb_deinit ();
      ms_sleep (4000);//!!!!!!!!!!!!!!      (1000);                                                // 600 ms not enough; 700 seems OK
      logd ("Done iusb_best_vendor != USB_VID_GOO ms_sleep()");
    }
    else
      logd ("Done iusb_best_vendor == USB_VID_GOO");
  }

  if (iusb_state != hu_STATE_STARTED && iusb_best_vendor != USB_VID_GOO && (iusb_best_product < 0x2d00 || iusb_best_product > 0x2d05)) {
    loge ("No Google AA/Accessory mode iusb_best_vendor: 0x%x", iusb_best_vendor);
    iusb_deinit ();
    iusb_state = hu_STATE_STOPPED;
    logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
    return (-3);
  }

  int pipefd[2] = {-1,-1};
  if (pipe(pipefd) < 0)
  {
    loge("Pipe create failed");
    return -1;
  }
  readfd = pipefd[0];
  pipe_write_fd = pipefd[1];

  if (pipe(pipefd) < 0)
  {
    loge("Error pipe create failed");
    return -1;
  }
  errorfd = pipefd[0];
  error_write_fd = pipefd[1];

  abort_usbthread = false;
  usb_recv_thread = std::thread([this]{ this->usb_recv_thread_main(); });

  recv_temp_buffer.resize(1024);
  start_usb_recv();

  iusb_state = hu_STATE_STARTED;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
  return (0);
}

