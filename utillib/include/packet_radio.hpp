#pragma once
#include <sys/socket.h>
#include <netax25/ax25.h>
#include <string>
#include <vector>
#include <Socket.hpp>

namespace jab::file{

//This is basically just a callsign followed by a hyphen and integer (SSID) 0-15, all 
//suitably encoded for use as a network address.
//It involves left-shifting all of the characters so that the low-order bit can be used
//as a string terminator. Also, the SSID is super weird and it involves adding a number
//from 0-15 to ASCII '0', so I think I will let libax25 deal with it, since it makes no sense to me.
//Seems like it would have made the most sense to just transmit an 8-bit integer, especially since
//the protocol is not limited to text-only, AFAIK, but whatever.
class AX25Address : public ::ax25_address{
public:
    AX25Address() = default;    				//uninitialized, equivalent to the bare struct
    AX25Address(const std::string &call_sign); 	//Initialized to the encoded call_sign with SSID

    //factory method
    static inline AX25Address addr( const std::string &host ){
        return AX25Address(host);
    }
};

//AX.25 and Netrom sockaddrs seem to be implemented identically except that the
//address family is different. I guess they do about the same thing, but 
//Netrom has more routing logic.
class RadioSockAddr : public ::full_sockaddr_ax25, public jab::file::sockaddr_interface<RadioSockAddr>{
public:
    RadioSockAddr( const std::string &dest, const std::vector<std::string> &route = std::vector<std::string>());
};

class AX25SockAddr : public RadioSockAddr{
public:
    AX25SockAddr( const std::string &dest, const std::vector<std::string> &route = std::vector<std::string>() );
};

class NetromSockAddr : public RadioSockAddr{
public:
    NetromSockAddr( const std::string &dest, const std::vector<std::string> &route = std::vector<std::string>() );
};

}
