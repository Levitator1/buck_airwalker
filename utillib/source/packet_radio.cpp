#include "netax25/axlib.h"
#include "packet_radio.hpp"
#include <algorithm>
#include "exception.hpp"

using namespace std;
using namespace jab::file;
using namespace jab::exception;
using namespace std::string_literals;

//There's no indication in the man file that there's any POSIX-style
//or other error messaging. It just either fails or not.
//
//Takes a callsign of the form: X-Y
//Where X is 1-6 alpha-numerics, the hyphen is literal, and Y is 
//an integer 0-15.
AX25Address::AX25Address(const std::string &call_sign){
    auto result = ::ax25_aton_entry(call_sign.c_str(), this->ax25_call);
    if(result == -1)
        throw InvalidNodeName(call_sign);
}

RadioSockAddr::RadioSockAddr( const std::string &dest, const std::vector<std::string> &route){
    auto routen = route.size(), routemax = sizeof(this->fsa_digipeater);
    if(routen > routemax)
        throw AddressRouteTooLong("Route length of "s + to_string(routen) + 
            " nodes is more than hard system limit: " + to_string(routemax) );
    
    this->fsa_ax25.sax25_call = AX25Address(dest);
    this->fsa_ax25.sax25_ndigis = routen;

    std::transform( route.begin(), route.end(), this->fsa_digipeater, AX25Address::addr);
}

AX25SockAddr::AX25SockAddr(const std::string &dest, const std::vector<std::string> &route):
    RadioSockAddr(dest, route){

    this->fsa_ax25.sax25_family = AF_AX25;
}

NetromSockAddr::NetromSockAddr(const std::string &dest, const std::vector<std::string> &route):
    RadioSockAddr(dest, route){

    this->fsa_ax25.sax25_family = AF_NETROM;
}
