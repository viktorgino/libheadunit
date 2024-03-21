#define LOGTAG "hu_tcp"
#include "TCPTransportStream.h"

#include "hu_uti.h"  // Utilities

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

using namespace AndroidAuto;

TCPTransportStream::~TCPTransportStream() {
    if (itcp_state != hu_STATE_STOPPED) {
        Stop();
    }
}

TCPTransportStream::TCPTransportStream(std::map<std::string, std::string> _settings) : AbstractTransportStream(_settings) {
    settings = _settings;
    wifi_direct = 1;
}

int TCPTransportStream::Write(const byte *buf, int len, int tmo) {
    // milli-second timeout
    if (readfd < 0)
        return (-1);

    fd_set sock_set;
    FD_ZERO(&sock_set);
    FD_SET(readfd, &sock_set);

    timeval tv_timeout;
    tv_timeout.tv_sec = tmo / 1000;
    tv_timeout.tv_usec = tmo * 1000;

    int ret = select(readfd + 1, NULL, &sock_set, NULL, &tv_timeout);
    if (ret <= 0)
        return ret;

    errno = 0;
    ret = write(readfd, buf, len);
    if (ret != len) {  // Write, if can't write full buffer...
        loge("Error write  errno: %d (%s)", errno, strerror(errno));
    }

    return (ret);
}

int TCPTransportStream::itcp_deinit() {  // !!!! Need to better reset and wait
                                           // a while to kill transfers in
                                           // progress and auto-restart properly

    if (readfd >= 0)
        close(readfd);
    readfd = -1;

    if (tcp_so_fd >= 0)
        close(tcp_so_fd);
    tcp_so_fd = -1;

    logd("itcp_deinit done");

    return (0);
}

#define CS_FAM AF_INET

#define CS_SOCK_TYPE SOCK_STREAM
#define RES_DATA_MAX 65536

int TCPTransportStream::itcp_accept() {
    if (tcp_so_fd < 0)
        return (-1);

    struct sockaddr_in  cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    errno = 0;
    int ret = 0;
    if (wifi_direct) {
        loge("Reading from WiFi direct");
        int tries = 0;
        while (readfd < 0) {
            if (tries > 10){
                loge("itcp accept timed out");
                break;
            }
            readfd = accept(tcp_so_fd, (struct sockaddr *)&cli_addr, &cli_len);
            ms_sleep(100);
            tries++;
        }
        if (readfd < 0) {
            loge("Error accept errno: %d (%s)", errno, strerror(errno));
            return (-1);
        }
    } else {
        inet_pton(AF_INET, settings["network_address"].c_str(), &(cli_addr.sin_addr));
        cli_addr.sin_family = AF_INET;
        cli_addr.sin_port = htons(5277);

        ret = connect(tcp_so_fd, (const struct sockaddr *)&cli_addr, cli_len);
        if (ret != 0) {
            if (errno != last_errno)  // avoid spamming the log with the same error
            {
                loge("Error connect errno: %d (%s)", errno, strerror(errno));
                last_errno = errno;
            }
            sleep(5);
            return (-1);
        }
        readfd = tcp_so_fd;
    }

    return (tcp_so_fd);
}

int TCPTransportStream::itcp_init() {
    int net_port = 5000;

    int cmd_len = 0, ctr = 0;

    errno = 0;
    if ((tcp_so_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK , 0)) < 0) {  // Create socket
        loge("gen_server_loop socket  errno: %d (%s)", errno, strerror(errno));
        return (-1);
    }
    int flag = 1;
    int ret = setsockopt(tcp_so_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &flag, sizeof(flag));
    if (ret != 0)
        loge("setsockopt TCP_NODELAY errno: %d (%s)", errno, strerror(errno));
    else
        logd("setsockopt TCP_NODELAY Success");

    if (wifi_direct) {
        struct sockaddr_in  srv_addr;
        socklen_t srv_len = sizeof(struct sockaddr_in);

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = INADDR_ANY;

        srv_addr.sin_port = htons(net_port);
        

        logd("srv_len: %d  fam: %d  addr: 0x%x  port: %d", srv_len, srv_addr.sin_family, ntohl(srv_addr.sin_addr.s_addr), ntohs(srv_addr.sin_port));
        errno = 0;
        if (bind(tcp_so_fd, (struct sockaddr *)&srv_addr, srv_len) < 0) {  // Bind socket to server address
            loge("Error bind  errno: %d (%s)", errno, strerror(errno));
            loge("Inet stream continuing despite bind error");
        }

        // Get command from client
        errno = 0;
        if (listen(tcp_so_fd, 5)) {  // Backlog= 5; likely don't need this
            loge("Error listen  errno: %d (%s)", errno, strerror(errno));
            return (-4);
        }

        logd("itcp_init Ready");
    }

    if (readfd >= 0) {
        itcp_deinit();        
    }
    while (readfd < 0) {      // While we don't have an IO socket file descriptor...
        ret = itcp_accept();  // Try to get one with 100 ms timeout
        if (ret < 0) {
            loge("Error while trying to read from TCP stream");
            return (-1);
        }
    }
    logd("itcp_accept done");

    return (0);
}

int TCPTransportStream::Stop() {
    itcp_state = hu_STATE_STOPPIN;
    logd("  SET: itcp_state: %d (%s)", itcp_state, state_get(itcp_state));
    int ret = itcp_deinit();
    itcp_state = hu_STATE_STOPPED;
    logd("  SET: itcp_state: %d (%s)", itcp_state, state_get(itcp_state));
    return (ret);
}

int TCPTransportStream::Start() {
    int ret = 0;

    if (itcp_state == hu_STATE_STARTED) {
        logd("CHECK: itcp_state: %d (%s)", itcp_state, state_get(itcp_state));
        return (0);
    }

    itcp_state = hu_STATE_STARTIN;
    logd("  SET: itcp_state: %d (%s)", itcp_state, state_get(itcp_state));

    ret = itcp_init();
    if (ret < 0) {
        loge("Error itcp_init");
        itcp_deinit();
        itcp_state = hu_STATE_STOPPED;
        logd("  SET: itcp_state: %d (%s)", itcp_state, state_get(itcp_state));
        return (-1);
    }
    logd("OK itcp_init");

    itcp_state = hu_STATE_STARTED;
    logd("  SET: itcp_state: %d (%s)", itcp_state, state_get(itcp_state));
    return (0);
}
