#pragma once
#include <string>
#include <stdexcept>
#include <filesystem>
#include "BawConfig.hpp"
#include "concurrency/thread_pool.hpp"

namespace k3yab::bawns{

using path_type = std::filesystem::path;


class baw{
    bawns::Config m_config;

public:
    baw( const bawns::Config &config ):
        m_config(config){            
    }

	void run();
};

class baw_exception : public std::runtime_error{
public:
    baw_exception(const std::string &msg);
};

class config_exception : public baw_exception{
    using baw_exception::baw_exception;
};

}
