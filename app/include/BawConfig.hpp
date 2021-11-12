#pragma once
#include <string>
#include <filesystem>
#include "config.h"

namespace k3yab::bawns{

class ConfigError:public std::invalid_argument{
	using base_type = std::invalid_argument;

public:
	using base_type::base_type;
};

struct Config{
	
	constexpr static char application_name[] = "Buck Airwalker";
	constexpr static char default_state_path[] = "baw_state.bin";

	//Since we will be dealing with undelmited messages of unknown length, we need a timeout to decide when a reply has completed.
	//This is in ms.
	static constexpr unsigned long response_timeout = 15 * 1000;

	std::string local_address; //local address to bind to, which will typically be the user's callsign, usually hyphenated
	int threads = 1; 
	std::filesystem::path state_path = default_state_path; 

	Config(int argc, char *argv[]);
	static void show_usage(int argc, char *argv[]);

};

}
