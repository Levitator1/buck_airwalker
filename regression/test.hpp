#pragma once
#include <string>
#include "exception.hpp"

//A deterministic stream of random integers
//Implemented with the global C RNG, so it will alter its state
class RandStream{
public:
	using value_type = int;	
	
private:
	value_type m_seed;

public:
	RandStream(value_type seed = 0);
	value_type seed() const;
	value_type get();
	value_type operator()();

	int int_between(int min, int max_plus_1);
};


class TestException:public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};
