#include <memory>
#include <filesystem>
#include <iostream>
#include <sstream>
#include "io.hpp"
#include "FSFile.hpp"
#include "test.hpp"

using namespace std;
using namespace jab::file;

IOTests::IOTests(){    
}

static const filesystem::path io_test_file_path = "test_data.txt";
static const ::size_t io_test_buffer_size = 12;

static std::string make_test_string( RandStream &rnd ){
	using Conf = IOTestsConfig;
	std::stringstream strs;
	const int len = rnd.int_between( Conf::min_string_size, Conf::max_string_size+1 );
	for(int i=0;i<len;++i){
		strs << Conf::text_characters[rnd.get() % sizeof(Conf::text_characters)];
	}
	return strs.str();
}

IOTestString::IOTestString(RandStream &rnd):
	m_string(make_test_string(rnd)){
}

void IOTestString::out( std::ostream &stream ){
	stream << m_string;
}

void IOTestString::in( std::istream &stream ){
	std::string str;
	stream >> str;
	if( str != m_string )
		throw TestException("IO Test string read did not match expected: '" + str + "'!='" + m_string + "'");
}

void IOTestInteger::out(std::ostream &stream){
	stream << m_int;
}

void IOTestInteger::in(std::istream &stream){
	int result;
	stream >> result;
	if( result = m_int )
		throw TestException("IO Test integer did not match expected: " + std::to_string(result) + " != " + std::to_string(m_int));
}

IOTestInteger::IOTestInteger(RandStream &rnd):
	m_int(rnd.get()){
}

IOTestOperationStream::IOTestOperationStream(seed_type seed):
	m_rand(seed){		
}

IOTestOperationStream::seed_type IOTestOperationStream::seed() const{
	return m_rand.seed();
}

enum operation_type{
	text = 0,
	integer = 1,
	operation_max = integer
};

IOTestOperation *IOTestOperationStream::get(){
	auto opno = m_rand.get() % (operation_type::operation_max + 1);
	IOTestOperation *result = nullptr;

	switch(opno){
		case text:
			result = new IOTestString(m_rand);			
			break;

		case integer:
			result = new IOTestInteger(m_rand);
			break;
	}
	return result;
}

void IOTests::run(){

	RandStream rng;
    FSFile_iostream<char> s;

    {
        EllipsisGuard eg("Openining a test data file...");
        s = { io_test_file_path,  flags::rw | flags::create, Conf::io_buffer_size};
        eg.ok();
    }
    s << "Hi. " << endl << "Testing: " << 123 << ". More testing. Etc. Etc." << endl << endl;

	/*
	IOTestOperationStream opstream( Conf::io_test_seed );
	IOTestOperation *op = nullptr;
	for(int i=0; i < Conf::io_test_operations; ++i){
		op = opstream.get();
		op->out(s);
	}
	*/

}