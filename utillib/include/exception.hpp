#pragma once
#include <stdexcept>
#include <system_error>
#include "meta.hpp"

namespace jab{
namespace exception{

//A common means of responding to an error in a background thread,
//which is to print the error trace to stderr via the concurrent global console object
class DefaultBackgroundExceptionHandler{

	static constexpr char default_message[] = "Error in background thread...";
	std::string m_msg;

public:
	DefaultBackgroundExceptionHandler( const std::string &msg = default_message );
	void operator()( const std::exception_ptr &) const;
};

class InvalidAddress : public std::invalid_argument{
public:
    using std::invalid_argument::invalid_argument;
};

class InvalidNodeName : public InvalidAddress{
public:
    InvalidNodeName(const std::string &addr);
};

class AddressRouteTooLong : public InvalidAddress, public std::length_error{
public:
    AddressRouteTooLong(const std::string &msg);
};

class IOError : public std::runtime_error{
public:
	IOError( const std::string &msg = "IO Error" );
    //using std::runtime_error::runtime_error;
    //virtual const char *what() const noexcept override;
 };

//Throw this with errno set to represent an exception of the POSIX API
class posix_exception:public std::system_error{
public:
    posix_exception(const std::string &);

    //-1 is the usual error code for a posix call.
    //There are probably some exceptions, though.
    template<typename Result, typename WrapEx>
    static inline Result check(Result result, const std::string &msg, meta::type<WrapEx> wrap, Result errcode = -1){
        try{
            return result == errcode ? throw posix_exception(msg) : result;
        }
        catch(...){
            std::throw_with_nested( WrapEx() );
        }
    }

    template<typename Result, typename WrapEx, typename F, typename OP = decltype( std::declval<F>().operator()() )>
    static inline Result check(Result result, F msg_func, meta::type<WrapEx> wrap, Result errcode = -1){
        try{
            return result == errcode ? throw posix_exception(msg_func()) : result;
        }
        catch(...){
            std::throw_with_nested( WrapEx() );
        }
    }

    /*
    //Lazy error message generation
    template<typename F, typename OP = decltype( std::declval<F>().operator()() )>
    static inline int check(int result, F msg_func, int errcode = -1){
        return result == errcode ? throw posix_exception(msg_func()) : result;
    }
    */
};

void print_exception( std::ostream &stream, const std::exception &ex );
std::ostream &operator<<( std::ostream &stream,  const std::exception &ex );

template<class Arg0, class Arg1, class... Args>
void nest_exceptions(Arg0 ex0, Arg1 ex1, Args... exn){
	try{
		next_exceptions(ex1, exn...);
	}
	catch(Arg1 x){
		std::throw_with_nested(ex0);
	}
}

template<class Arg0>
void nest_exceptions(Arg0 ex0){
	throw ex0;
}

}    
}

