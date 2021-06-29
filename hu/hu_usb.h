#include <libusb.h>
#include <poll.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "hu_aap.h"


namespace AndroidAuto {

class HUTransportStreamUSB : public HUTransportStream {
   public:
    ~HUTransportStreamUSB();
    HUTransportStreamUSB(std::map<std::string, std::string> _settings);
    virtual int Start() override;
    virtual int Stop() override;
    virtual int Write(const byte* buf, int len, int tmo) override;

   private:
    libusb_context* m_usbContext = NULL;
    libusb_device_handle* m_usbDeviceHandle = NULL;
    enum HU_STATE m_state = hu_STATE_INITIAL;

    int iusb_ep_in = -1;
    int iusb_ep_out = -1;

    int m_pipeWriteFD = -1;
    int m_errorWriteFD = -1;

    int abort_usb_thread_pipe_read_fd = -1;
    int abort_usb_thread_pipe_write_fd = -1;
    std::mutex usb_thread_event_fds_lock;
    std::vector<pollfd> usb_thread_event_fds;

    // usb recv thread state
    std::vector<byte> m_tempReceiveBuffer;
    std::thread usb_recv_thread;

    void usb_recv_thread_main();
    int start_usb_recv();

    void libusb_callback(libusb_transfer* transfer);
    static void libusb_callback_tramp(libusb_transfer* transfer);

    void libusb_callback_send(libusb_transfer* transfer);
    static void libusb_callback_send_tramp(libusb_transfer* transfer);

    void libusb_callback_pollfd_added(int fd, short events);
    static void libusb_callback_pollfd_added_tramp(int fd, short events,
                                                   void* user_data);

    void libusb_callback_pollfd_removed(int fd);
    static void libusb_callback_pollfd_removed_tramp(int fd, void* user_data);

    libusb_device_handle* find_oap_device();
};
}
