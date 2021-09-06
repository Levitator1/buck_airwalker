#include "exception.hpp"
#include "Socket.hpp"

using namespace std;
using namespace jab::file;
using namespace jab::exception;
using namespace jab::meta;

Socket::Socket(int domain, int type, int protocol):
    File( posix_exception::check(::socket(domain, type, protocol), "Failed creating socket", meta::type<IOError>()) ){    
}

void Socket::connect(const struct ::sockaddr *addr, ::socklen_t addrlen){
    posix_exception::check(::connect( *this, addr, addrlen ), "Socket connect failed", meta::type<IOError>());    
}

void Socket::bind(const struct ::sockaddr *addr, ::socklen_t addrlen){
    posix_exception::check(::bind(*this, addr, addrlen), "Failed to bind socket", meta::type<IOError>() );
}

void Socket::setsockopt(int level, int optname, const void *optval, ::socklen_t optlen){
    posix_exception::check( ::setsockopt(*this, level, optname, optval, optlen), "Failed setting socket options (setsockopt())", meta::type<IOError>() ); 
}

//This is a NOP for a socket. I guess it just transmits everything when it gets around to it.
void Socket::flush(){    
}
