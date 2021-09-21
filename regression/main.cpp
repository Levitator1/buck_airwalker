#include <exception>
#include <iostream>
#include "exception.hpp"
#include "io.hpp"

using namespace jab::exception;

int main(){

    try{
        IOTests io_tests;
        io_tests.run();        
    }
    catch( const std::exception &ex){
        std::cerr << ex << std::endl;
    }

    return 0;
}
