#include <cctype>
#include <cstring>
#include <string>
#include <iomanip>
#include <sstream>
#include "util.hpp"

using namespace jab::util;

::size_t jab::util::strnlenlt(const char *str, ::size_t n){
	auto result = ::strnlen(str, n);
	if(result == n)
		throw std::invalid_argument("Unterminated string");
	else
		return result;
}

std::string jab::util::hex_format(const std::string &str){
    std::istringstream strin(str);
    std::ostringstream strout;
    
    strout << std::hex << std::setprecision(2);
    int ch;
    while( ch = strin.get(), !strin.fail() ){
        if(::isprint(ch)){
            strout.put( (char)ch );
        }
        else{
            strout << " 0x" << ch << " ";
        }
    }
    return strout.str();
}