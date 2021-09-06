#include <termios.h>
#include <filesystem>
#include "exception.hpp"
#include "Serial.hpp"

using namespace jab::file;
using namespace jab::serial;
using namespace jab::exception;

Serial::Serial(){}

Serial::Serial(const std::filesystem::path &path, int fl):
    FSFile(path, fl){ 
    read_tios();
}

void Serial::read_tios(){
    if(::tcgetattr(*this, &tios) == -1)
        throw posix_exception("Failed retrieving serial port state");
}

void Serial::write_tios(){
    if( ::tcsetattr(*this, TCSADRAIN, &tios) == -1)
        throw posix_exception("Failed writing serial port state");
}

void Serial::setbaud(::speed_t speed){
    if( ::cfsetispeed( &tios, speed ) == -1 || ::cfsetospeed( &tios, speed ) == -1 )
        throw posix_exception("Failed setting serial baud rate");
    write_tios();
}

//Flush output
void Serial::flush(){
    if( ::tcflush( *this, TCOFLUSH ) == -1 )
        throw posix_exception("Failed flushing serial port");
    
    //This flush is for disk files only?
    //File::flush();
}

//Flush output, discard read buffer
void Serial::purge(){
    if( ::tcflush(*this, TCIOFLUSH) == -1 )
        throw posix_exception("failed purging serial port");
}

