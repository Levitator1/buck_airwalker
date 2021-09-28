#pragma once
#include "test.hpp"
#include "File.hpp"

struct IOTestsConfig{
	static constexpr int io_test_seed = 0;
	static constexpr int io_test_operations = 1000000;
	static constexpr int min_string_size = 1;
	static constexpr int max_string_size = 16;
	static constexpr char text_characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

	//A small value is probably best for triggering lots of cases, or at least lots of operations, for testing purposes
	static constexpr std::streamsize io_buffer_size = 4;
};

class IOTests{
public:
	using Conf = IOTestsConfig;
    IOTests();
    void run();
};

class IOTestOperation{
public:
	using Conf = IOTestsConfig;
	virtual void out( std::ostream &stream ) = 0;
	virtual void in( std::istream &stream ) = 0;
};

class IOTestString:public IOTestOperation{
	std::string m_string;

public:
	IOTestString( RandStream &rnd );
	virtual void out( std::ostream &stream ) override;
	virtual void in( std::istream &stream ) override;
};

class IOTestInteger:public IOTestOperation{
	int m_int;
public:
	IOTestInteger( RandStream &rnd );
	virtual void out( std::ostream &stream ) override;
	virtual void in( std::istream &stream ) override;
};

class IOTestOperationStream{
	RandStream m_rand;
public:
	using seed_type = RandStream::value_type;	

	IOTestOperationStream( seed_type seed = 0  );
	IOTestOperation *get();
	seed_type seed() const;
};


