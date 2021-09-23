#include <memory>
#include <filesystem>
#include <iostream>
#include "io.hpp"
#include "FSFile.hpp"
#include "test.hpp"

using namespace std;
using namespace jab::file;

IOTests::IOTests(){    
}

static const filesystem::path io_test_file_path = "test_data.txt";
static const ::size_t io_test_buffer_size = 12;

void IOTests::run(){

    FSFile_iostream<char> s;

    {
        EllipsisGuard eg("Openining a test data file...");
        s = { io_test_file_path,  flags::rw | flags::create};
        eg.ok();
    }

    s << "Hi. " << endl << "Testing: " << 123 << ". More testing. Etc. Etc." << endl << endl;
}