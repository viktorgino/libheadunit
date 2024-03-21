#pragma once

#include <functional>
#include <map>
#include <string>
#include <thread>

namespace AndroidAuto {
class AbstractTransportStream {
protected:
    int readfd = -1;
    // optional if required for pipe, etc
    int errorfd = -1;

public:
    virtual ~AbstractTransportStream() {
    }
    inline AbstractTransportStream(std::map<std::string, std::string>) {
    }
    virtual int Start() = 0;
    virtual int Stop() = 0;
    virtual int Write(const unsigned char *buf, int len, int tmo) = 0;

    inline int GetReadFD() {
        return readfd;
    }
    inline int GetErrorFD() {
        return errorfd;
    }
};
}