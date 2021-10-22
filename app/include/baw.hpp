#pragma once
#include <string>
#include <stdexcept>
#include <filesystem>
#include "concurrency/thread_pool.hpp"
#include "BawConfig.hpp"
#include "state_file.hpp"


namespace k3yab::bawns{

using path_type = std::filesystem::path;


class baw{
    bawns::Config m_config;
	bawns::state::StateFile m_state;

public:
    baw( const bawns::Config &config );

	void run();
	const bawns::Config &config() const;
	bawns::state::StateFile &state();
	const bawns::state::StateFile &state() const;	
};

//A thread pool task that visits a node
class node_task{
	baw *m_appp;
	std::string m_callsign;

public:
	node_task(); //terminate worker thread
	node_task( baw &app, const std::string &call);

	jab::util::Console::out_type print() const;
	int operator()();
};

class baw_exception : public std::runtime_error{
public:
    baw_exception(const std::string &msg);
};

class config_exception : public baw_exception{
    using baw_exception::baw_exception;
};

}
