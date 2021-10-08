#pragma once
#include <filesystem>
#include "config.h"

namespace k3yab::bawns{

class ConfigError:public std::invalid_argument{
	using base_type = std::invalid_argument;

public:
	using base_type::base_type;
};

struct Config{
	
	constexpr static char application_name[] = {"Buck Airwalker"};
	constexpr static char default_state_path[] = "baw_state.bin";
	int threads = 1;
	std::filesystem::path state_path = default_state_path; 

	Config(int argc, char *argv[]);
	static void show_usage(int argc, char *argv[]);
};

}