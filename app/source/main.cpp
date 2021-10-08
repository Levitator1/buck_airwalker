#include <iostream>
#include "BawConfig.hpp"
#include "baw.hpp"

using namespace std;
using namespace k3yab::bawns;

static void show_banner(){

	cout << k3yab::bawns::Config::application_name << " V" << VERSION << endl;
	cout << "AX.25/Netrom network discovery tool" << endl;
	cout << endl;
}

int main( int argc, char *argv[]){    
	show_banner();
    k3yab::bawns::Config config(argc, argv);
    baw app(config);
	app.run();
}

