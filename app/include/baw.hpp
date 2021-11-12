#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <regex>
#include "concurrency/thread_pool.hpp"
#include "BawConfig.hpp"
#include "state_file.hpp"


namespace k3yab::bawns{

using path_type = std::filesystem::path;

class baw{
    bawns::Config m_config;
	bawns::state::StateFile m_state;

public:
	baw() = default;
    baw( const bawns::Config &config );

	void run();
	const bawns::Config &config() const;
	bawns::state::StateFile &state();
	const bawns::state::StateFile &state() const;
	static void send_command( std::ostream &stream, const std::string &);
};

//A thread pool task that visits a node
class node_task{

	static const std::regex callsign_regex;

	baw *m_appp;
	std::string m_callsign;

	std::string parse_callsign(std::string &str) const;

	//Main process
	int run();

	//Eat stream data until there is an RX timeout
	void eat_stream( std::istream &stream );

	//first is list of nodes representing route.
	//second is any forwarding node specified, which may be blank
	using route_result_type = std::pair< std::vector<std::string>, std::string >;

	//The KPC3P BBS appliance has a long-form J command which lists
	//all known hosts and their via. 
	route_result_type try_j_l_command( std::iostream &stream );

	bool bbs_mode( std::iostream &stream );

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
