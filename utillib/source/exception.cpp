#include <cerrno>
#include "exception.hpp"

using namespace std::string_literals;
using namespace jab::exception;

//IOError::IOError( const std::string &msg ):
//  std::runtime_error(msg){}

InvalidNodeName::InvalidNodeName(const std::string &addr):
    InvalidAddress("Invalid network address: "s + addr){}

AddressRouteTooLong::AddressRouteTooLong(const std::string &msg):
    InvalidAddress(msg),
    std::length_error(msg){        
}

const char *IOError::what() const noexcept{
    return "IO error";
}

posix_exception::posix_exception(const std::string &wha):
    std::system_error( errno, std::generic_category(),  wha){
}
