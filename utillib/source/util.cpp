#include <cctype>
#include <string>
#include <iomanip>
#include <sstream>
#include "util.hpp"

using namespace jab::util;


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