
#include "hu_aap.h"
#include <netinet/in.h>

namespace AndroidAuto {
class HUTransportStreamTCP : public HUTransportStream
{
    int tcp_so_fd = -1;
    struct sockaddr_in  cli_addr = {0};
    socklen_t cli_len = 0;
    struct sockaddr_in  srv_addr = {0};
    socklen_t srv_len = 0;

    int wifi_direct = 0;
    int itcp_deinit ();
    int itcp_accept ();
    int itcp_init();
    std::map<std::string, std::string> settings;
 public:
    ~HUTransportStreamTCP();
    HUTransportStreamTCP(std::map<std::string, std::string> _settings);
    virtual int Start() override;
    virtual int Stop() override;
    virtual int Write(const byte* buf, int len, int tmo) override;
};
}
