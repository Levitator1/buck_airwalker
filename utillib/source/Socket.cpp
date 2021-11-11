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

::socklen_t Socket::getsockopt(int level, int optname, void *optval, ::socklen_t optlen) const{	
    posix_exception::check( ::getsockopt(*this, level, optname, optval, &optlen), "Failed setting socket options (setsockopt())", meta::type<IOError>() );
	return optlen;
}

//This is a NOP for a socket. I guess it just transmits everything when it gets around to it.
void Socket::flush(){    
}

unsigned long Socket::rx_timeout() const{
	::timeval t;
	this->getsockopt( SOL_SOCKET, SO_RCVTIMEO, t);

	unsigned long result = t.tv_sec * 1000;
	return result + t.tv_usec / 1000;
}

void Socket::rx_timeout(unsigned long v){
	::timeval t;
	t.tv_sec = v / 1000;
	t.tv_usec = (v % 1000) * 1000;
	this->setsockopt(SOL_SOCKET, SO_RCVTIMEO, t);
}

bool Socket::timeout_as_eof() const{
	return m_timeout_as_eof;
}

void Socket::timeout_as_eof(bool v){
	m_timeout_as_eof = v;
}

std::streamsize Socket::read(char *data, std::streamsize len){
	auto result = ::recv(fd(), data, len, 0);
	if(result == -1){
		if( m_timeout_as_eof && (errno == EAGAIN || errno == EWOULDBLOCK)  )
			return 0;
		else
			posix_exception::check(result, "Error receiving from socket", meta::type<IOError>());
	}
	return result;
}

std::streamsize Socket::write(const char *data, std::streamsize len){
	auto result = ::send(fd(), data, len, 0);
	if(result == -1){
		if( m_timeout_as_eof && (errno == EAGAIN || errno == EWOULDBLOCK)  )
			return 0;
		else
			posix_exception::check(result, "Error sending to socket", meta::type<IOError>());
	}
	return result;
}
