#pragma once
#include <termios.h>
#include <filesystem>
#include "FSFile.hpp"

namespace jab{
namespace serial{

class Serial:public jab::file::FSFile{
protected:
    ::termios tios;
    void read_tios();
    void write_tios();

public:
    Serial();
    Serial(const std::filesystem::path &path, int fl);
    void setbaud(::speed_t);
    void flush() override;
    
    //Like flush, but purge the RX buffer
    void purge();
};  

}
}

