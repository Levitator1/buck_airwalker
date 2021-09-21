#include "baw.hpp"

using namespace std;
using namespace k3yab::bawns;

baw_exception::baw_exception(const string &msg):
    runtime_error(msg){}


baw_config::baw_config(){}

baw_config::baw_config( int argc, const char *const argv[] ){
    if(argc > 2)
        throw config_exception( "Too many arguments" );

    if(argc < 2)
        throw config_exception( "Need the path to a file listing root node addresses" );

    node_file_path = argv[1];
}