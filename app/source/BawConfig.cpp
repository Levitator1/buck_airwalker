#include <string>
#include <iostream>
#include "BawConfig.hpp"

using namespace std::string_literals;
using namespace k3yab::bawns;

void Config::show_usage(int argc, char *argv[]){
	std::cout << "Usage: " << std::string(argv[0]) << " [--help | -h] [-j <no. of threads>] [-f state file path] <local node>" << std::endl << std::endl;
	std::cout << "	--help, -h		This help" << std::endl;
	std::cout << "	-j <count>		Max number of simultaneous parallel AX.25 connections" << std::endl;
	std::cout << "	-f <path>		Path of state file to load and append node discoveries" << std::endl;
	std::cout << "					defaults to '" << Config::default_state_path  << "'" << std::endl;
	std::cout << "	<local node>	Local address or callsign to use, typically the user's hyphenated callsign" << std::endl << std::endl;
	std::cout << "On stdin, pipe or type a list of root nodes at which to begin querying, one callsign per line" << std::endl;
	std::cout << std::endl;
}

static void demand(int argc, int &i, const std::string &msg){
	if(i >= argc)
		throw ConfigError("Missing expected argument: " + msg);
}

static void demand_next(int argc, int &i, const std::string &msg){
	demand( argc, ++i, msg );	
}

static int get_int(const char *arg){
	try{
		return std::atoi(arg);
	}
	catch( const std::exception &ex ){
		std::throw_with_nested( ConfigError("Error parsing integer argument") );
	}
}

static int process_switches(Config &conf, int argc, char *argv[]){

	if(argc < 2)
		return 1;

	int i = 1;
	std::string arg;

	while( i < argc){
		arg = argv[i];
		if(arg[0] != '-')
			return i;

		if( arg == "-h" || arg == "--help" )
			Config::show_usage(argc, argv);
		else if( arg == "-j" ){	
			demand_next( argc, i, "thread count");		
			conf.threads = get_int( argv[i] );
			if(conf.threads < 1)
				throw ConfigError("Thread count must be >= 1");						
		}
		else if( arg == "-f" ){
			demand_next( argc, i, "state file path" );
			conf.state_path = argv[i];
		}
		else{
			throw ConfigError("Unrecognized switch: " + arg);
		}
		
		++i;
	}
	return i;
}

Config::Config( int argc, char *argv[]){
	int i = process_switches( *this, argc, argv ); //non-positional switches
	demand(argc, i, "Expected local address or callsign for binding client sockets");
	local_address = argv[i++];
	if(i < argc)
		throw ConfigError("Unexpected argument: "s + argv[i]);
}
