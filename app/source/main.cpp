#include <unistd.h>
#include <iostream>
#include "exception.hpp"
#include "BawConfig.hpp"
#include "console.hpp"
#include "baw.hpp"

using namespace std;
using namespace jab::util;
using namespace jab::exception;
using namespace k3yab::bawns;

static void show_banner(){

	jab::util::console.out() << k3yab::bawns::Config::application_name << " V" << VERSION << endl
		<< "AX.25/Netrom network discovery tool" << endl
		<< endl;
}

int run( int argc, char *argv[] ){
	console.init();
	show_banner();

    k3yab::bawns::Config config(argc, argv);
    baw app(config);
	app.run();
	return 0;	
}

int main( int argc, char *argv[]){
	try{
		auto result = run(argc, argv);
		std::cout << "Done" << std::endl;
		return result;
	}
	catch(const std::exception &ex){
		std::cout << "Unexpected exception..." << std::endl;
		std::cout << ex;		
	}
	catch(...){
		std::cout << "Unhandled exception type! Madness and chaos prevail!" << std::endl;
	}

	std::cout << "Exiting on error." << std::endl;
	return -1;
}

