#include "hu_aap.h"
#include <thread>
#include <libusb.h>
#include <vector>
#include <mutex>
#include <condition_variable>

const char* iusb_error_get(int error);

class HUTransportStreamUSB : public HUTransportStream
{
    HU_STATE isub_state = hu_STATE_INITIAL;

    libusb_context* iusb_ctx = NULL;
    libusb_device_handle * iusb_dev_hndl      = NULL;
    libusb_device *               iusb_best_device  = NULL;
    int   iusb_ep_in          = -1;
    int   iusb_ep_out         = -1;

    int   iusb_best_vendor    = 0;
    int   iusb_best_product   = 0;
    char  iusb_curr_man [256] = {0};
    char  iusb_best_man [256] = {0};
    char  iusb_curr_pro [256] = {0};
    char  iusb_best_pro [256] = {0};

    int pipe_write_fd = -1;
    int error_write_fd = -1;

    //usb recv thread state
    std::vector<byte> recv_temp_buffer;
    std::thread usb_recv_thread;
    void usb_recv_thread_main();
    int start_usb_recv();
    void libusb_callback(libusb_transfer *transfer);
    static void libusb_callback_tramp(libusb_transfer *transfer);

    void libusb_callback_send(libusb_transfer *transfer);
    static void libusb_callback_send_tramp(libusb_transfer *transfer);

    bool abort_usbthread = false;

    int iusb_init (byte ep_in_addr, byte ep_out_addr);
    int iusb_deinit ();
    int iusb_oap_start ();
public:
    ~HUTransportStreamUSB();
    HUTransportStreamUSB();
    virtual int Start(byte ep_in_addr, byte ep_out_addr) override;
    virtual int Stop() override;
    virtual int Write(const byte* buf, int len, int tmo) override;
};
