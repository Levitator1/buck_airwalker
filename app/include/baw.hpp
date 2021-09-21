#pragma once
#include <string>
#include <stdexcept>
#include <filesystem>

namespace k3yab::bawns{

using path_type = std::filesystem::path;

struct baw_config{

    baw_config();
    baw_config(int argc, const char * const argv[]);

    path_type node_file_path;
};

class baw{
    baw_config m_config;

public:
    baw( const baw_config &config ):
        m_config(config){            
    }
};

class baw_exception : std::runtime_error{
public:
    baw_exception(const std::string &msg);
};

class config_exception : baw_exception{
    using baw_exception::baw_exception;
};

}
