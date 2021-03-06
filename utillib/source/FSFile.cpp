#include <fcntl.h>
#include <unistd.h>
#include <string>
#include "meta.hpp"
#include "exception.hpp"
#include "FSFile.hpp"

using namespace std;
using namespace std::string_literals;
using namespace jab::file;
using namespace jab::exception;
using namespace jab;


//If "fl" bits are present in "in", then OR "bits" into "out"
static void check_ofl(int in, int &out, int fl, int bits){
    out |= ((in & fl) == fl) ? bits : 0;
}

static int do_open( const char *path, int fl ){
    int ofl = 0;
    if( fl & r && fl & w){
        ofl |= O_RDWR;
    }
    else{
        check_ofl(fl, ofl, r, O_RDONLY);
        check_ofl(fl, ofl, w, O_WRONLY);
    }
    check_ofl(fl, ofl, ndelay, O_NDELAY);
    check_ofl(fl, ofl, noctty, O_NOCTTY);
    check_ofl(fl, ofl, nonblock, O_NONBLOCK);
    check_ofl(fl, ofl, create, O_CREAT);
    
    return posix_exception::check( ::open(path, ofl, S_IRUSR | S_IWUSR),  [path](){ return "Error opening file: "s + path; }, meta::type<IOError>() );    
}

FSFile::FSFile(const std::filesystem::path &path, int fl):
    File(do_open(path.native().c_str(), fl)){
}

void FSFile::truncate(std::streamsize sz){
	posix_exception::check( ::ftruncate(fd(), sz), "Error truncating file"s, meta::type<IOError>() );
}