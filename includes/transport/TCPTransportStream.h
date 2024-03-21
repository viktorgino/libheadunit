#pragma once

#include <netinet/in.h>

#include "defs.h"
#include "AbstractTransportStream.h"
#include "hu_uti.h"  // Utilities

namespace AndroidAuto {
class TCPTransportStream : public AbstractTransportStream {
    int tcp_so_fd = -1;

    int wifi_direct = 0;
    int itcp_deinit();
    int itcp_accept();
    int itcp_init();
    std::map<std::string, std::string> settings;

public:
    ~TCPTransportStream();
    TCPTransportStream(std::map<std::string, std::string> _settings);
    virtual int Start() override;
    virtual int Stop() override;
    virtual int Write(const byte* buf, int len, int tmo) override;

private:
    enum HU_STATE itcp_state = HU_STATE::hu_STATE_INITIAL;
    int last_errno = 0;  // store last error printed
};
}  // namespace AndroidAuto
