#pragma once

#include <sys/types.h>
#include <sys/socket.h>

#include "File.hpp"

namespace jab::file{

class Socket : public jab::file::File{
    bool m_timeout_as_eof = false;

public:
    Socket(int domain, int type, int protocol);
    
    virtual void flush() override;
    void setsockopt(int level, int optname, const void *optval, ::socklen_t optlen);
	::socklen_t getsockopt(int level, int optname, void *optval, ::socklen_t optlen) const;

	template<typename T>
	void setsockopt(int level, int optname, const T &obj){
		this->setsockopt(level, optname, &obj, sizeof(obj));
	}

	template<typename T>
	::socklen_t getsockopt(int level, int optname, T &obj) const{
		return this->getsockopt(level, optname, &obj, sizeof(obj));
	}

    void bind(const struct ::sockaddr *addr, ::socklen_t addrlen);
    void connect(const struct ::sockaddr *addr, ::socklen_t addrlen);

	unsigned long rx_timeout() const;
	void rx_timeout(unsigned long);

	//Rather than report a posix error by throwing an exception, a timeout will be treated
	//like a temporary EOF, returning 0 bytes transferred
	bool timeout_as_eof() const;
	void timeout_as_eof(bool v);

	virtual std::streamsize read(char *data, std::streamsize len) override;
    virtual std::streamsize write(const char *data, std::streamsize len) override;
};

template<typename Char = char, typename Traits = std::char_traits<Char>>
class Socket_iostream:public File_iostream<Char, Socket, Traits>{
	using base_type = File_iostream<Char, Socket, Traits>;

public:
	using base_type::base_type;

	Socket_iostream(int domain, int type, int protocol):
		base_type( Socket(domain, type, protocol) ){
	}
};

//Mixin that provides a derived class with a sockaddr interface
template<typename This>
class sockaddr_interface{
	using this_type = This;

	template<typename T>
	auto downcast( T *thisp ){
		return static_cast<jab::util::copy_cv_t<T, this_type> *>(thisp);
	}

public:
	::sockaddr *sa(){ return reinterpret_cast<::sockaddr *>( downcast(this) ); }
	const ::sockaddr *sa() const{ return reinterpret_cast<const ::sockaddr *>( downcast(this) ); }
	operator ::sockaddr *(){ return sa(); }
	operator const ::sockaddr *(){ return sa(); }
};

}
