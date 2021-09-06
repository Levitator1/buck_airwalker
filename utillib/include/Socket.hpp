#pragma once

#include <sys/types.h>
#include <sys/socket.h>

#include "File.hpp"

namespace jab::file{

class Socket : public jab::file::File{
    
public:
    Socket(int domain, int type, int protocol);
    
    virtual void flush() override;
    void setsockopt(int level, int optname, const void *optval, ::socklen_t optlen);
    void bind(const struct ::sockaddr *addr, ::socklen_t addrlen);
    void connect(const struct ::sockaddr *addr, ::socklen_t addrlen);    
};


}